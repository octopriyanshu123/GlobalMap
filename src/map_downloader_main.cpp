// Download geo-referenced satellite tiles for a region (Module 2, step 1).
//
// Tiles are Web-Mercator XYZ tiles (EPSG:3857). Output layout:
//   <out>/tiles/<z>/<x>/<y>.jpg     raw tile cache (re-runs skip existing tiles)
//   <out>/tile_index.json           per-tile lat/lon bounds, used by MapTileManager
//   <out>/mosaic.jpg + .jgw + .prj  optional stitched map (--mosaic)
//
// Examples:
//   ./map_downloader --center 28.6139 77.2090 --radius-m 200 --zoom 19 --out data/map_delhi --mosaic
//   ./map_downloader --bbox 72.86 19.06 72.89 19.09 --zoom 18 --out data/map --dry-run

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "global_map/tile_math.hpp"
#include "global_map/tile_stitcher.hpp"

namespace fs = std::filesystem;
using namespace global_map;

namespace {

const std::map<std::string, std::string> kSources = {
    {"esri", "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}"},
};

struct Args {
  bool has_bbox = false, has_center = false;
  BBox bbox{};
  double center_lat = 0, center_lon = 0;
  double radius_m = 1000.0;
  int zoom = 19;
  std::string source = "esri";
  std::string out = "data/map";
  int workers = 8;
  int64_t max_tiles = 50000;
  bool mosaic = false, dry_run = false;
};

void usage(const char* prog) {
  std::printf(
      "usage: %s (--bbox MIN_LON MIN_LAT MAX_LON MAX_LAT | --center LAT LON) [options]\n"
      "  --radius-m M     half-width of the area when using --center (default 1000)\n"
      "  --zoom Z         19 ~0.3 m/px, 18 ~0.6 m/px, 17 ~1.2 m/px (default 19)\n"
      "  --source S       'esri' or a URL template with {z}/{x}/{y}\n"
      "  --out DIR        output folder (default data/map)\n"
      "  --workers N      parallel downloads (default 8)\n"
      "  --max-tiles N    safety limit (default 50000)\n"
      "  --mosaic         also stitch a geo-referenced mosaic.jpg\n"
      "  --dry-run        only print the tile count and size estimate\n",
      prog);
}

Args parseArgs(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    const std::string k = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) throw std::runtime_error(k + " needs a value");
      return argv[++i];
    };
    if (k == "-h" || k == "--help") {
      usage(argv[0]);
      std::exit(0);
    } else if (k == "--bbox") {
      a.bbox.min_lon = std::stod(next());
      a.bbox.min_lat = std::stod(next());
      a.bbox.max_lon = std::stod(next());
      a.bbox.max_lat = std::stod(next());
      a.has_bbox = true;
    } else if (k == "--center") {
      a.center_lat = std::stod(next());
      a.center_lon = std::stod(next());
      a.has_center = true;
    } else if (k == "--radius-m") {
      a.radius_m = std::stod(next());
    } else if (k == "--zoom") {
      a.zoom = std::stoi(next());
    } else if (k == "--source") {
      a.source = next();
    } else if (k == "--out") {
      a.out = next();
    } else if (k == "--workers") {
      a.workers = std::max(1, std::stoi(next()));
    } else if (k == "--max-tiles") {
      a.max_tiles = std::stoll(next());
    } else if (k == "--mosaic") {
      a.mosaic = true;
    } else if (k == "--dry-run") {
      a.dry_run = true;
    } else {
      throw std::runtime_error("unknown argument: " + k);
    }
  }
  if (a.has_bbox == a.has_center) throw std::runtime_error("give exactly one of --bbox or --center");
  if (a.zoom < 0 || a.zoom > 23) throw std::runtime_error("--zoom must be 0..23");
  return a;
}

std::string formatUrl(std::string tpl, int z, int64_t x, int64_t y) {
  auto replace = [&tpl](const std::string& key, const std::string& val) {
    for (size_t p; (p = tpl.find(key)) != std::string::npos;) tpl.replace(p, key.size(), val);
  };
  replace("{z}", std::to_string(z));
  replace("{x}", std::to_string(x));
  replace("{y}", std::to_string(y));
  return tpl;
}

size_t writeToString(char* ptr, size_t size, size_t nmemb, void* userdata) {
  static_cast<std::string*>(userdata)->append(ptr, size * nmemb);
  return size * nmemb;
}

std::string tilePath(int z, int64_t x, int64_t y) {
  return "tiles/" + std::to_string(z) + "/" + std::to_string(x) + "/" + std::to_string(y) + ".jpg";
}

// Returns "cached", "ok", "missing" or "failed".
std::string fetch(CURL* curl, const std::string& url, const fs::path& path, int retries = 4) {
  std::error_code ec;
  if (fs::exists(path, ec) && fs::file_size(path, ec) > 0) return "cached";
  for (int attempt = 0; attempt < retries; ++attempt) {
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    const CURLcode res = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (res == CURLE_OK && status == 200 && !body.empty()) {
      fs::create_directories(path.parent_path());
      const fs::path tmp = path.string() + ".part";
      std::ofstream(tmp, std::ios::binary).write(body.data(), static_cast<std::streamsize>(body.size()));
      fs::rename(tmp, path);
      return "ok";
    }
    if (res == CURLE_OK && status == 404) return "missing";
    std::this_thread::sleep_for(std::chrono::seconds(1 << attempt));
  }
  return "failed";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Args args = parseArgs(argc, argv);
    const BBox bb = args.has_center ? bboxFromCenter(args.center_lat, args.center_lon, args.radius_m) : args.bbox;
    const int z = args.zoom;
    const TileXY t0 = lonlatToTile(bb.min_lon, bb.max_lat, z);  // north-west corner
    const TileXY t1 = lonlatToTile(bb.max_lon, bb.min_lat, z);  // south-east corner
    const int64_t cols = t1.x - t0.x + 1, rows = t1.y - t0.y + 1, count = cols * rows;

    const double mid_lat = (bb.min_lat + bb.max_lat) / 2.0;
    std::printf("bbox lon[%.6f, %.6f] lat[%.6f, %.6f]\n", bb.min_lon, bb.max_lon, bb.min_lat, bb.max_lat);
    std::printf("zoom %d: %.2f m/px, %lldx%lld = %lld tiles (~%.1f MB)\n", z, gsd(mid_lat, z),
                static_cast<long long>(cols), static_cast<long long>(rows), static_cast<long long>(count),
                count * 20.0 / 1024.0);
    if (args.dry_run) return 0;
    if (count > args.max_tiles) {
      std::fprintf(stderr, "%lld tiles exceeds --max-tiles %lld; shrink the area or raise the limit\n",
                   static_cast<long long>(count), static_cast<long long>(args.max_tiles));
      return 1;
    }
    if (args.mosaic && count > 4096) {
      std::fprintf(stderr, "mosaic would exceed 16k x 16k px; drop --mosaic and use the tile cache instead\n");
      return 1;
    }

    const auto src = kSources.find(args.source);
    const std::string url_tpl = src != kSources.end() ? src->second : args.source;
    const fs::path out_dir = args.out;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    std::atomic<int64_t> next_idx{0}, done{0};
    std::mutex stats_mutex;
    std::map<std::string, int64_t> stats;

    auto worker = [&]() {
      CURL* curl = curl_easy_init();
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt(curl, CURLOPT_USERAGENT, "vo-drone-localization/0.1 (research)");
      for (int64_t i; (i = next_idx++) < count;) {
        const int64_t x = t0.x + i / rows, y = t0.y + i % rows;
        const std::string res = fetch(curl, formatUrl(url_tpl, z, x, y), out_dir / tilePath(z, x, y));
        const int64_t d = ++done;
        std::lock_guard<std::mutex> lock(stats_mutex);
        ++stats[res];
        if (d % 100 == 0 || d == count) {
          std::printf("\r%lld/%lld {", static_cast<long long>(d), static_cast<long long>(count));
          for (const auto& [k, v] : stats) std::printf(" %s: %lld", k.c_str(), static_cast<long long>(v));
          std::printf(" }");
          std::fflush(stdout);
        }
      }
      curl_easy_cleanup(curl);
    };
    std::vector<std::thread> pool;
    for (int i = 0; i < args.workers; ++i) pool.emplace_back(worker);
    for (auto& t : pool) t.join();
    curl_global_cleanup();
    std::printf("\n");

    nlohmann::json index = {
        {"crs", "EPSG:3857"},
        {"zoom", z},
        {"tile_size", kTileSize},
        {"source", url_tpl},
        {"bbox_lonlat", {bb.min_lon, bb.min_lat, bb.max_lon, bb.max_lat}},
        {"tile_range", {{"x", {t0.x, t1.x}}, {"y", {t0.y, t1.y}}}},
        {"gsd_m_per_px", gsd(mid_lat, z)},
        {"tiles", nlohmann::json::array()},
    };
    for (int64_t x = t0.x; x <= t1.x; ++x) {
      for (int64_t y = t0.y; y <= t1.y; ++y) {
        const std::string path = tilePath(z, x, y);
        if (!fs::exists(out_dir / path)) continue;
        const LonLat nw = tileToLonLat(static_cast<double>(x), static_cast<double>(y), z);
        const LonLat se = tileToLonLat(static_cast<double>(x + 1), static_cast<double>(y + 1), z);
        index["tiles"].push_back(
            {{"x", x}, {"y", y}, {"path", path}, {"bounds_lonlat", {nw.lon, se.lat, se.lon, nw.lat}}});
      }
    }
    fs::create_directories(out_dir);
    std::ofstream(out_dir / "tile_index.json") << index.dump(1);
    std::printf("index: %zu tiles -> %s\n", index["tiles"].size(), (out_dir / "tile_index.json").c_str());

    if (args.mosaic) stitch(out_dir.string());
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
  return 0;
}
