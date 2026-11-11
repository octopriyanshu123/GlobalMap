#include "global_map/map_image.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>
#include <opencv2/imgcodecs.hpp>

#include "global_map/tile_stitcher.hpp"

namespace fs = std::filesystem;

namespace global_map {

LonLat MapImage::pixelToLonLat(double col, double row) const {
  const double mx = info.mercator_left + col * info.mercator_m_per_px;
  const double my = info.mercator_top - row * info.mercator_m_per_px;
  return {rad2deg(mx / kEarthRadius), rad2deg(std::atan(std::sinh(my / kEarthRadius)))};
}

cv::Point2d MapImage::lonLatToPixel(double lon, double lat) const {
  const double mx = deg2rad(lon) * kEarthRadius;
  const double my = std::asinh(std::tan(deg2rad(lat))) * kEarthRadius;
  return {(mx - info.mercator_left) / info.mercator_m_per_px, (info.mercator_top - my) / info.mercator_m_per_px};
}

std::optional<MosaicInfo> loadMosaicInfo(const std::string& map_dir) {
  const fs::path path = fs::path(map_dir) / "mosaic.json";
  if (!fs::exists(path)) return std::nullopt;
  std::ifstream f(path);
  if (!f) throw std::runtime_error("cannot open " + path.string());
  const nlohmann::json j = nlohmann::json::parse(f);

  const auto& b = j.at("bounds_lonlat");
  MosaicInfo m;
  m.image = j.at("image");
  m.crs = j.at("crs");
  m.width = j.at("width");
  m.height = j.at("height");
  m.zoom = j.at("zoom");
  m.bounds = {b.at(0), b.at(1), b.at(2), b.at(3)};
  m.mercator_left = j.at("mercator_top_left")[0];
  m.mercator_top = j.at("mercator_top_left")[1];
  m.mercator_m_per_px = j.at("mercator_m_per_px");
  m.ground_m_per_px = j.at("ground_m_per_px");
  m.tiles_placed = j.at("tiles_placed");
  m.tiles_expected = j.at("tiles_expected");
  return m;
}

MapImage loadMapImage(const std::string& map_dir_in) {
  const std::string map_dir = resolveMapDir(map_dir_in);
  auto info = loadMosaicInfo(map_dir);
  if (!info) throw std::runtime_error("no mosaic.json in " + map_dir + "; run tile_stitcher first");

  const fs::path image_path = fs::path(map_dir) / info->image;
  MapImage map{*info, cv::imread(image_path.string(), cv::IMREAD_COLOR)};
  if (map.image.empty()) throw std::runtime_error("cannot read " + image_path.string());
  if (map.image.cols != info->width || map.image.rows != info->height) {
    throw std::runtime_error(image_path.string() + " is " + std::to_string(map.image.cols) + "x" +
                             std::to_string(map.image.rows) + " px but mosaic.json says " +
                             std::to_string(info->width) + "x" + std::to_string(info->height));
  }
  return map;
}

}  // namespace global_map
