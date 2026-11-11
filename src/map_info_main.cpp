// Read the JSON files of a downloaded map folder and print what they contain.
//
// Usage:
//   map_info <map_dir>
//
// If the folder has a mosaic, it is also loaded and its centre pixel converted to lon/lat.
//
// Examples:
//   ./map_info data/map_delhi

#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "global_map/map_image.hpp"
#include "global_map/tile_math.hpp"
#include "global_map/tile_stitcher.hpp"

namespace fs = std::filesystem;
using namespace global_map;

namespace {

struct TileEntry {
  int64_t x, y;
  std::string path;
  BBox bounds;
};

// Contents of tile_index.json (written by map_downloader).
struct TileIndex {
  std::string crs, source;
  int zoom = 0, tile_size = 0;
  BBox bbox{};
  TileXY t0{}, t1{};  // north-west and south-east tile, inclusive
  double gsd_m_per_px = 0;
  std::vector<TileEntry> tiles;
};

BBox readBBox(const nlohmann::json& j) { return {j.at(0), j.at(1), j.at(2), j.at(3)}; }

nlohmann::json readJson(const fs::path& path) {
  std::ifstream f(path);
  if (!f) throw std::runtime_error("cannot open " + path.string());
  return nlohmann::json::parse(f);
}

TileIndex loadTileIndex(const fs::path& map_dir) {
  const nlohmann::json j = readJson(map_dir / "tile_index.json");
  TileIndex idx;
  idx.crs = j.at("crs");
  idx.source = j.at("source");
  idx.zoom = j.at("zoom");
  idx.tile_size = j.at("tile_size");
  idx.bbox = readBBox(j.at("bbox_lonlat"));
  idx.t0 = {j.at("tile_range").at("x")[0], j.at("tile_range").at("y")[0]};
  idx.t1 = {j.at("tile_range").at("x")[1], j.at("tile_range").at("y")[1]};
  idx.gsd_m_per_px = j.at("gsd_m_per_px");
  for (const auto& t : j.at("tiles")) {
    idx.tiles.push_back({t.at("x"), t.at("y"), t.at("path"), readBBox(t.at("bounds_lonlat"))});
  }
  return idx;
}

// Approximate ground width/height of a lon/lat box in metres.
void bboxSizeM(const BBox& b, double& w, double& h) {
  const double mid_lat = deg2rad((b.min_lat + b.max_lat) / 2.0);
  w = deg2rad(b.max_lon - b.min_lon) * kEarthRadius * std::cos(mid_lat);
  h = deg2rad(b.max_lat - b.min_lat) * kEarthRadius;
}

void printTileIndex(const TileIndex& idx, const fs::path& map_dir) {
  const int64_t cols = idx.t1.x - idx.t0.x + 1, rows = idx.t1.y - idx.t0.y + 1;
  int64_t on_disk = 0;
  for (const auto& t : idx.tiles) on_disk += fs::exists(map_dir / t.path);
  double w, h;
  bboxSizeM(idx.bbox, w, h);

  std::printf("tile_index.json\n");
  std::printf("  crs          %s\n", idx.crs.c_str());
  std::printf("  source       %s\n", idx.source.c_str());
  std::printf("  zoom         %d (%d px tiles, %.3f m/px)\n", idx.zoom, idx.tile_size, idx.gsd_m_per_px);
  std::printf("  bbox         lon[%.6f, %.6f] lat[%.6f, %.6f] (~%.0f x %.0f m)\n", idx.bbox.min_lon,
              idx.bbox.max_lon, idx.bbox.min_lat, idx.bbox.max_lat, w, h);
  std::printf("  tile range   x[%lld, %lld] y[%lld, %lld] = %lldx%lld\n", static_cast<long long>(idx.t0.x),
              static_cast<long long>(idx.t1.x), static_cast<long long>(idx.t0.y), static_cast<long long>(idx.t1.y),
              static_cast<long long>(cols), static_cast<long long>(rows));
  std::printf("  tiles        %zu listed / %lld expected, %lld on disk\n", idx.tiles.size(),
              static_cast<long long>(cols * rows), static_cast<long long>(on_disk));
}

void printMosaicInfo(const MosaicInfo& m, const TileIndex& idx) {
  std::printf("mosaic.json\n");
  std::printf("  image        %s (%lldx%lld px, %s)\n", m.image.c_str(), static_cast<long long>(m.width),
              static_cast<long long>(m.height), m.crs.c_str());
  std::printf("  zoom         %d\n", m.zoom);
  std::printf("  bounds       lon[%.6f, %.6f] lat[%.6f, %.6f]\n", m.bounds.min_lon, m.bounds.max_lon,
              m.bounds.min_lat, m.bounds.max_lat);
  std::printf("  top-left     %.2f, %.2f (EPSG:3857 m)\n", m.mercator_left, m.mercator_top);
  std::printf("  resolution   %.4f m/px mercator, %.4f m/px ground\n", m.mercator_m_per_px, m.ground_m_per_px);
  std::printf("  tiles        %d/%d placed\n", m.tiles_placed, m.tiles_expected);

  // The mosaic is rebuilt separately, so it can be left over from an earlier download.
  double mx, my, size;
  tileToMercator(idx.t0.x, idx.t0.y, idx.zoom, mx, my, size);
  const int64_t expected = (idx.t1.x - idx.t0.x + 1) * (idx.t1.y - idx.t0.y + 1);
  if (m.zoom != idx.zoom || std::abs(m.mercator_left - mx) > 1e-3 || std::abs(m.mercator_top - my) > 1e-3 ||
      m.tiles_expected != expected) {
    std::printf("  WARNING: mosaic does not match tile_index.json; re-run tile_stitcher\n");
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2 || std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
    std::printf("usage: %s <map_dir>\n  map_dir   folder created by map_downloader (contains tile_index.json)\n",
                argv[0]);
    return argc == 2 ? 0 : 2;
  }
  try {
    const fs::path map_dir = resolveMapDir(argv[1]);
    std::printf("map folder: %s\n\n", map_dir.c_str());
    const TileIndex idx = loadTileIndex(map_dir);
    printTileIndex(idx, map_dir);
    if (const auto mosaic = loadMosaicInfo(map_dir)) {
      std::printf("\n");
      printMosaicInfo(*mosaic, idx);

      const MapImage map = loadMapImage(map_dir);
      const LonLat c = map.pixelToLonLat(map.image.cols / 2.0, map.image.rows / 2.0);
      std::printf("\nloaded %s: %dx%d px, %d channels, centre lon %.6f lat %.6f\n", map.info.image.c_str(),
                  map.image.cols, map.image.rows, map.image.channels(), c.lon, c.lat);
    }
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
  return 0;
}
