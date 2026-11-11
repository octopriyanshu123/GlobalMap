#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace global_map {

constexpr int64_t kMaxMosaicPixels = 400'000'000;  // ~20k x 20k; beyond this use the tile cache

// Finds a folder containing tile_index.json, relative to the current folder or up to
// three parent folders. Throws std::runtime_error listing available map folders if not found.
std::string resolveMapDir(const std::string& map_dir);

// Combines the tiles listed in <map_dir>/tile_index.json into one image and writes
// <out>.jpg|.png, a world file (.jgw/.pgw), <out>.prj and <out>.json next to it.
// Returns the metadata written to <out>.json. Throws std::runtime_error on failure.
nlohmann::json stitch(const std::string& map_dir, std::string out = "", int quality = 95,
                      int64_t max_pixels = kMaxMosaicPixels);

}  // namespace global_map
