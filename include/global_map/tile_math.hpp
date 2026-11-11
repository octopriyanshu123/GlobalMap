#pragma once
// Web-Mercator (EPSG:3857) XYZ tile math shared by the downloader and stitcher.
// At zoom z the world is 2^z x 2^z tiles of 256x256 px; x grows east, y grows south.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace global_map {

constexpr double kEarthRadius = 6378137.0;
constexpr int kTileSize = 256;
constexpr double kMaxLat = 85.0511;  // Mercator is infinite at the poles; web maps cut here

constexpr const char* kEpsg3857Wkt =
    "PROJCS[\"WGS 84 / Pseudo-Mercator\",GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\","
    "SPHEROID[\"WGS 84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],"
    "UNIT[\"degree\",0.0174532925199433]],PROJECTION[\"Mercator_1SP\"],"
    "PARAMETER[\"central_meridian\",0],PARAMETER[\"scale_factor\",1],"
    "PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0],"
    "UNIT[\"metre\",1],AUTHORITY[\"EPSG\",\"3857\"]]";

struct TileXY {
  int64_t x, y;
};

struct LonLat {
  double lon, lat;
};

struct BBox {
  double min_lon, min_lat, max_lon, max_lat;
};

inline double deg2rad(double d) { return d * M_PI / 180.0; }
inline double rad2deg(double r) { return r * 180.0 / M_PI; }

inline TileXY lonlatToTile(double lon, double lat, int z) {
  lat = std::clamp(lat, -kMaxLat, kMaxLat);
  const double n = std::ldexp(1.0, z);
  auto x = static_cast<int64_t>((lon + 180.0) / 360.0 * n);
  auto y = static_cast<int64_t>((1.0 - std::asinh(std::tan(deg2rad(lat))) / M_PI) / 2.0 * n);
  const auto max_idx = static_cast<int64_t>(n) - 1;
  return {std::clamp<int64_t>(x, 0, max_idx), std::clamp<int64_t>(y, 0, max_idx)};
}

// North-west corner of tile (x, y). Fractional x/y give any point inside a tile,
// e.g. x = x0 + pixel_col / 256.0 converts a mosaic pixel back to lon/lat.
inline LonLat tileToLonLat(double x, double y, int z) {
  const double n = std::ldexp(1.0, z);
  return {x / n * 360.0 - 180.0, rad2deg(std::atan(std::sinh(M_PI * (1.0 - 2.0 * y / n))))};
}

// North-west corner of tile (x, y) in EPSG:3857 metres, and the tile's width in metres.
inline void tileToMercator(int64_t x, int64_t y, int z, double& mx, double& my, double& size) {
  const double extent = M_PI * kEarthRadius;
  size = 2.0 * extent / std::ldexp(1.0, z);
  mx = -extent + static_cast<double>(x) * size;
  my = extent - static_cast<double>(y) * size;
}

// Ground sampling distance in metres/pixel.
inline double gsd(double lat, int z) {
  return std::cos(deg2rad(lat)) * 2.0 * M_PI * kEarthRadius / (kTileSize * std::ldexp(1.0, z));
}

inline BBox bboxFromCenter(double lat, double lon, double radius_m) {
  const double dlat = rad2deg(radius_m / kEarthRadius);
  const double dlon = rad2deg(radius_m / (kEarthRadius * std::cos(deg2rad(lat))));
  return {lon - dlon, lat - dlat, lon + dlon, lat + dlat};
}

}  // namespace global_map
