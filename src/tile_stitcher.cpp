#include "global_map/tile_stitcher.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <stdexcept>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "global_map/tile_math.hpp"

namespace fs = std::filesystem;

namespace global_map {

std::string resolveMapDir(const std::string& map_dir) {
  fs::path base = fs::current_path();
  std::vector<fs::path> roots;
  for (int i = 0; i < 4; ++i) {
    roots.push_back(base);
    if (!base.has_parent_path() || base.parent_path() == base) break;
    base = base.parent_path();
  }
  for (const auto& root : roots) {
    const fs::path cand = fs::path(map_dir).is_absolute() ? fs::path(map_dir) : root / map_dir;
    if (fs::is_regular_file(cand / "tile_index.json")) return cand.lexically_normal().string();
  }

  std::set<std::string> found;
  for (const auto& root : {roots.front(), roots.size() > 1 ? roots[1] : roots.front()}) {
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
      if (it->is_directory() && it->path().filename() == "tiles") {
        it.disable_recursion_pending();  // don't walk millions of tile folders
      } else if (it->path().filename() == "tile_index.json") {
        found.insert(fs::relative(it->path().parent_path()).string());
      }
    }
  }
  std::string msg = "no tile_index.json in '" + map_dir + "' (current folder: " +
                    fs::current_path().string() + ")";
  if (found.empty()) {
    msg += "\nrun map_downloader first";
  } else {
    msg += "\navailable map folders:";
    for (const auto& f : found) msg += "\n  " + f;
  }
  throw std::runtime_error(msg);
}

nlohmann::json stitch(const std::string& map_dir_in, std::string out, int quality, int64_t max_pixels) {
  const fs::path map_dir = resolveMapDir(map_dir_in);
  std::ifstream index_file(map_dir / "tile_index.json");
  const nlohmann::json index = nlohmann::json::parse(index_file);

  const int z = index.at("zoom");
  const int ts = index.at("tile_size");
  const int64_t x0 = index.at("tile_range").at("x")[0], x1 = index.at("tile_range").at("x")[1];
  const int64_t y0 = index.at("tile_range").at("y")[0], y1 = index.at("tile_range").at("y")[1];
  const int64_t w = (x1 - x0 + 1) * ts, h = (y1 - y0 + 1) * ts;
  if (w * h > max_pixels) {
    throw std::runtime_error("mosaic would be " + std::to_string(w) + "x" + std::to_string(h) + " px (> " +
                             std::to_string(max_pixels) +
                             " px); stitch a smaller area or raise --max-pixels");
  }

  fs::path out_path = out.empty() ? map_dir / "mosaic.jpg" : fs::path(out);
  std::string ext = out_path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  if (ext != ".jpg" && ext != ".jpeg" && ext != ".png") throw std::runtime_error("output must be .jpg or .png");

  cv::Mat mosaic = cv::Mat::zeros(static_cast<int>(h), static_cast<int>(w), CV_8UC3);
  int placed = 0;
  for (const auto& t : index.at("tiles")) {
    const fs::path path = map_dir / t.at("path").get<std::string>();
    if (!fs::exists(path)) continue;
    cv::Mat tile = cv::imread(path.string(), cv::IMREAD_COLOR);
    if (tile.empty()) {
      std::fprintf(stderr, "skipping corrupt tile %s\n", path.c_str());
      continue;
    }
    if (tile.cols != ts || tile.rows != ts) cv::resize(tile, tile, cv::Size(ts, ts));
    const int64_t col = (t.at("x").get<int64_t>() - x0) * ts;
    const int64_t row = (t.at("y").get<int64_t>() - y0) * ts;
    tile.copyTo(mosaic(cv::Rect(static_cast<int>(col), static_cast<int>(row), ts, ts)));
    ++placed;
  }
  const int64_t expected = (x1 - x0 + 1) * (y1 - y0 + 1);

  std::vector<int> params;
  if (ext != ".png") params = {cv::IMWRITE_JPEG_QUALITY, quality};
  if (!cv::imwrite(out_path.string(), mosaic, params)) {
    throw std::runtime_error("failed to write " + out_path.string());
  }

  // World file: pixel size and centre of the top-left pixel, in EPSG:3857.
  double mx, my, size;
  tileToMercator(x0, y0, z, mx, my, size);
  const double px = size / ts;
  fs::path world = out_path;
  world.replace_extension(ext == ".png" ? ".pgw" : ".jgw");
  {
    std::ofstream f(world);
    f << std::setprecision(17) << px << "\n0\n0\n" << -px << "\n" << mx + px / 2 << "\n" << my - px / 2 << "\n";
  }
  fs::path prj = out_path;
  prj.replace_extension(".prj");
  std::ofstream(prj) << kEpsg3857Wkt;

  const LonLat nw = tileToLonLat(static_cast<double>(x0), static_cast<double>(y0), z);
  const LonLat se = tileToLonLat(static_cast<double>(x1 + 1), static_cast<double>(y1 + 1), z);
  nlohmann::json meta = {
      {"image", out_path.filename().string()},
      {"width", w},
      {"height", h},
      {"zoom", z},
      {"crs", "EPSG:3857"},
      {"bounds_lonlat", {nw.lon, se.lat, se.lon, nw.lat}},
      {"mercator_top_left", {mx, my}},
      {"mercator_m_per_px", px},
      {"ground_m_per_px", px * std::cos(deg2rad((nw.lat + se.lat) / 2.0))},
      {"tiles_placed", placed},
      {"tiles_expected", expected},
  };
  fs::path meta_path = out_path;
  meta_path.replace_extension(".json");
  std::ofstream(meta_path) << meta.dump(1);

  std::printf("%d/%lld tiles -> %s (%lldx%lld px, %.2f m/px)\n", placed, static_cast<long long>(expected),
              out_path.c_str(), static_cast<long long>(w), static_cast<long long>(h),
              meta["ground_m_per_px"].get<double>());
  std::printf("bounds lon[%.6f, %.6f] lat[%.6f, %.6f]\n", nw.lon, se.lon, se.lat, nw.lat);
  return meta;
}

}  // namespace global_map
