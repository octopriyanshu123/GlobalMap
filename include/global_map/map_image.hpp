#pragma once
// Load the stitched map image (mosaic.jpg/.png + mosaic.json) and convert between
// its pixels and lon/lat.

#include <cstdint>
#include <optional>
#include <string>

#include <opencv2/core.hpp>

#include "global_map/tile_math.hpp"

namespace global_map {

// Contents of mosaic.json (written by tile_stitcher).
struct MosaicInfo {
  std::string image, crs;
  int64_t width = 0, height = 0;
  int zoom = 0;
  BBox bounds{};
  double mercator_left = 0, mercator_top = 0;  // EPSG:3857 metres of the image's top-left corner
  double mercator_m_per_px = 0, ground_m_per_px = 0;
  int tiles_placed = 0, tiles_expected = 0;
};

struct MapImage {
  MosaicInfo info;
  cv::Mat image;  // BGR, info.width x info.height

  // (col, row) are pixel coordinates measured from the image's top-left corner;
  // (0.5, 0.5) is the centre of the first pixel.
  LonLat pixelToLonLat(double col, double row) const;
  cv::Point2d lonLatToPixel(double lon, double lat) const;
};

// Reads <map_dir>/mosaic.json, or returns nullopt if the folder has no mosaic.
std::optional<MosaicInfo> loadMosaicInfo(const std::string& map_dir);

// Loads the mosaic image and its metadata. map_dir is resolved like resolveMapDir().
// Throws std::runtime_error if there is no mosaic or the image doesn't match mosaic.json.
MapImage loadMapImage(const std::string& map_dir);

}  // namespace global_map
