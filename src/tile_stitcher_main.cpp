// Combine downloaded map tiles into one geo-referenced image.
//
// Usage:
//   tile_stitcher <map_dir> [--out mosaic.jpg|.png] [--quality 95] [--max-pixels N]
//
// Examples:
//   ./tile_stitcher data/map_delhi
//   ./tile_stitcher data/map_delhi --out data/map_delhi/delhi.png

#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

#include "global_map/tile_stitcher.hpp"

static void usage(const char* prog) {
  std::printf(
      "usage: %s <map_dir> [--out PATH.jpg|.png] [--quality 95] [--max-pixels N]\n"
      "  map_dir   folder created by map_downloader (contains tile_index.json)\n",
      prog);
}

int main(int argc, char** argv) {
  std::string map_dir, out;
  int quality = 95;
  int64_t max_pixels = global_map::kMaxMosaicPixels;

  try {
    for (int i = 1; i < argc; ++i) {
      const std::string a = argv[i];
      auto next = [&]() -> std::string {
        if (i + 1 >= argc) throw std::runtime_error(a + " needs a value");
        return argv[++i];
      };
      if (a == "-h" || a == "--help") {
        usage(argv[0]);
        return 0;
      } else if (a == "--out") {
        out = next();
      } else if (a == "--quality") {
        quality = std::stoi(next());
      } else if (a == "--max-pixels") {
        max_pixels = std::stoll(next());
      } else if (!a.empty() && a[0] != '-' && map_dir.empty()) {
        map_dir = a;
      } else {
        throw std::runtime_error("unknown argument: " + a);
      }
    }
    if (map_dir.empty()) {
      usage(argv[0]);
      return 2;
    }
    global_map::stitch(map_dir, out, quality, max_pixels);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
  return 0;
}
