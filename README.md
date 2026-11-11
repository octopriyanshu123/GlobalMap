# GlobalMap (C++): satellite map downloader and tile stitcher

Step 1 of the global visual localization module: download geo-referenced satellite imagery for the flight area, and optionally combine it into one image that the map matcher can use as a reference.

| Program | What it does |
|---|---|
| `map_downloader` | Downloads XYZ map tiles for an area, saves them to disk and writes `tile_index.json` |
| `tile_stitcher` | Combines the tiles of a downloaded folder into one geo-referenced image |

The Python versions in [`../map_downloader.py`](../map_downloader.py) and [`../tile_stitcher.py`](../tile_stitcher.py) take the same options and produce the same output, so either one can read a folder created by the other.

---

## 1. Build

Dependencies (Ubuntu 22.04 package names):

| Library | Package | Used for |
|---|---|---|
| libcurl | `libcurl4-openssl-dev` | HTTP downloads |
| OpenCV 4 (`core`, `imgcodecs`, `imgproc`) | `libopencv-dev` | Reading, combining and writing images |
| nlohmann/json 3 | `nlohmann-json3-dev` | `tile_index.json` and mosaic metadata |
| C++17 compiler, CMake ≥ 3.16 | `build-essential cmake` | |

```bash
cd GlobalMap/cpp
cmake -S . -B build
cmake --build build -j
```

The build produces `build/map_downloader`, `build/tile_stitcher` and the static library `build/libglobal_map.a`.

## 2. Usage

### Download an area

```bash
# Centre point + radius (radius = distance from the centre to each edge, so 200 m gives a 400 m square)
./build/map_downloader --center 28.6139 77.2090 --radius-m 200 --zoom 19 --out data/map_delhi

# Bounding box: note the order MIN_LON MIN_LAT MAX_LON MAX_LAT
./build/map_downloader --bbox 77.2069 28.6121 77.2110 28.6157 --zoom 19 --out data/map_delhi

# Only print how many tiles it would be and how much space they need
./build/map_downloader --center 28.6139 77.2090 --radius-m 5000 --zoom 19 --dry-run

# Download and stitch in one step
./build/map_downloader --center 28.6139 77.2090 --radius-m 200 --zoom 19 --out data/map_delhi --mosaic
```

| Option | Default | Meaning |
|---|---|---|
| `--center LAT LON` | | Centre of the area, in decimal degrees, **latitude first** |
| `--radius-m M` | `1000` | Distance from the centre to each edge, in metres (used with `--center`) |
| `--bbox MIN_LON MIN_LAT MAX_LON MAX_LAT` | | The area as a box, **longitude first** |
| `--zoom Z` | `19` | Detail level: 19 ≈ 0.3 m/px, 18 ≈ 0.6 m/px, 17 ≈ 1.2 m/px |
| `--source S` | `esri` | `esri`, or any tile URL containing `{z}`, `{x}` and `{y}` |
| `--out DIR` | `data/map` | Output folder |
| `--workers N` | `8` | Number of parallel downloads |
| `--max-tiles N` | `50000` | Safety limit. Raise it on purpose for bigger areas. |
| `--mosaic` | off | Also stitch the tiles into one image (at most 4096 tiles) |
| `--dry-run` | off | Print the tile count and estimated size, then stop |

Running the downloader again on the same folder skips tiles that are already saved. An interrupted download can therefore be resumed by re-running the same command.

Pick the zoom level from the flight altitude:

| Altitude above ground | Zoom | Resolution |
|---|---|---|
| below 50 m | 19 | ~0.3 m/px |
| 50–150 m | 18 | ~0.6 m/px |
| 150–500 m | 17 | ~1.2 m/px |
| above 500 m | 16 | ~2.4 m/px |

### Stitch a downloaded folder

```bash
./build/tile_stitcher data/map_delhi                                 # -> data/map_delhi/mosaic.jpg
./build/tile_stitcher data/map_delhi --out data/map_delhi/delhi.png  # lossless PNG
```

| Option | Default | Meaning |
|---|---|---|
| `map_dir` | (required) | Folder that contains `tile_index.json` |
| `--out PATH` | `<map_dir>/mosaic.jpg` | Output image, `.jpg` or `.png` |
| `--quality Q` | `95` | JPEG quality |
| `--max-pixels N` | `400000000` | Refuse to build images larger than this (about 20k × 20k) |

`map_dir` is looked up relative to the current folder and up to three parent folders. If it isn't found, the error message lists the map folders that do exist.

## 3. Output files

```
data/map_delhi/
├── tiles/19/<x>/<y>.jpg   one 256×256 image per tile
├── tile_index.json        list of tiles with their lat/lon bounds
├── mosaic.jpg             stitched image                     (tile_stitcher / --mosaic)
├── mosaic.jgw             world file: pixel → map position   (.pgw for PNG)
├── mosaic.prj             map projection (EPSG:3857)
└── mosaic.json            size, bounds and metres/pixel, for the matcher
```

**`tile_index.json`**
```json
{
 "crs": "EPSG:3857", "zoom": 19, "tile_size": 256,
 "source": "https://server.arcgisonline.com/.../tile/{z}/{y}/{x}",
 "bbox_lonlat": [77.20695, 28.61210, 77.21105, 28.61570],
 "tile_range": {"x": [374584, 374590], "y": [218620, 218626]},
 "gsd_m_per_px": 0.262,
 "tiles": [
  {"x": 374584, "y": 218620, "path": "tiles/19/374584/218620.jpg",
   "bounds_lonlat": [west, south, east, north]}
 ]
}
```

**`mosaic.json`**
```json
{
 "image": "mosaic.jpg", "width": 1792, "height": 1792, "zoom": 19, "crs": "EPSG:3857",
 "bounds_lonlat": [77.206421, 28.611651, 77.211227, 28.615871],
 "mercator_top_left": [8594579.46, 3326845.22],
 "mercator_m_per_px": 0.2986,
 "ground_m_per_px": 0.2621,
 "tiles_placed": 49, "tiles_expected": 49
}
```

All `bounds_lonlat` values use the order `[west, south, east, north]`, i.e. `[min_lon, min_lat, max_lon, max_lat]`.

---

## 4. Tile calculation logic

The formulas live in [`include/global_map/tile_math.hpp`](include/global_map/tile_math.hpp).

### 4.1 The XYZ tile grid

Web maps (Esri, Google, OpenStreetMap) use the Web Mercator projection, **EPSG:3857**, and cut the world into square tiles:

- At zoom `z` the whole world is a grid of **2^z × 2^z tiles**, and each tile is **256 × 256 px**.
- **x** is the column, counted west to east starting at longitude −180°.
- **y** is the row, counted **north to south** starting at latitude ≈ +85.05°.
- At zoom 19 the grid is 524,288 × 524,288 tiles.

```
            x →   0     1     2   ...  2^z−1
   y   0  ┌─────┬─────┬─────┬─────┐   ← ~85.05° N
   ↓   1  ├─────┼─────┼─────┼─────┤
       …  ├─────┼─────┼─────┼─────┤
  2^z−1   └─────┴─────┴─────┴─────┘   ← ~85.05° S
        −180°                     +180°
```

Mercator stretches the map toward the poles until the poles are infinitely far away. Web maps therefore cut the world at **±85.0511°**, which is the latitude that makes the map an exact square. `lonlatToTile()` clamps latitude to that range (`kMaxLat`).

### 4.2 Latitude/longitude → tile number (`lonlatToTile`)

```
n = 2^z
x = floor( (lon + 180) / 360 × n )
y = floor( (1 − asinh(tan(lat)) / π) / 2 × n )
```

- **x** is linear: longitude is spread evenly across the columns.
- **y** uses the Mercator formula `asinh(tan(lat))`, which grows faster toward the poles.
- `floor` keeps the whole tile number. The fractional part is the position inside the tile (fraction × 256 = pixel).

### 4.3 Tile number → latitude/longitude (`tileToLonLat`)

This is the inverse. It returns the **north-west corner** of tile `(x, y)`:

```
lon = x / n × 360 − 180
lat = atan( sinh( π × (1 − 2y / n) ) )
```

Fractional inputs give any point inside a tile. The south-east corner of a tile is `tileToLonLat(x + 1, y + 1, z)`.

### 4.4 Tile number → Mercator metres (`tileToMercator`)

EPSG:3857 coordinates are metres on a square from −20,037,508.34 m to +20,037,508.34 m (`π × 6378137`) on both axes:

```
extent    = π × 6378137
tile_size = 2 × extent / 2^z          (≈ 76.44 m at zoom 19)
mx = −extent + x × tile_size          (west edge)
my =  extent − y × tile_size          (north edge)
```

### 4.5 Ground resolution (`gsd`)

The real ground distance covered by one pixel gets smaller toward the poles:

```
gsd = cos(lat) × 2π × 6378137 / (256 × 2^z)      metres per pixel
```

| Zoom | Equator | Delhi (28.6° N) |
|---|---|---|
| 17 | 1.19 m/px | 1.05 m/px |
| 18 | 0.60 m/px | 0.52 m/px |
| 19 | 0.30 m/px | 0.26 m/px |

`mercator_m_per_px` in `mosaic.json` is the pixel size in projected metres, which is the same at every latitude. `ground_m_per_px` is the real distance on the ground. Use `ground_m_per_px` when comparing with the drone camera's resolution.

### 4.6 Centre + radius → bounding box (`bboxFromCenter`)

```
dlat = radius / 6378137                       (radians, then converted to degrees)
dlon = radius / (6378137 × cos(lat))          (radians, then converted to degrees)
box  = [lon − dlon, lat − dlat, lon + dlon, lat + dlat]
```

One degree of longitude is shorter away from the equator, which is why `dlon` is divided by `cos(lat)`.

### 4.7 Which tiles to download

The tile range comes from two opposite corners of the box:

```
(x0, y0) = lonlatToTile(min_lon, max_lat)   ← north-west corner (smallest x and y)
(x1, y1) = lonlatToTile(max_lon, min_lat)   ← south-east corner (largest x and y)

columns = x1 − x0 + 1
rows    = y1 − y0 + 1
tiles   = columns × rows
```

The `+1` is there because both edge tiles are included. The north-west corner uses `max_lat` because `y` counts downward.

Each tile is saved as `tiles/<z>/<x>/<y>.jpg`. This is the standard XYZ folder layout that QGIS, GDAL and most map tools understand. The Esri download URL puts the numbers in a different order (`/{z}/{y}/{x}`), but the saved files always use `z/x/y`.

### 4.8 Worked example: Delhi

`--center 28.6139 77.2090 --radius-m 200 --zoom 19`

**Bounding box**
```
dlat = 200 / 6378137 rad               = 0.001797°
dlon = 200 / (6378137 × cos 28.6139°)  = 0.002047°
west  77.206953   east  77.211047
south 28.612103   north 28.615697
```

**Corner tiles** (n = 2^19 = 524,288)

| Corner | Exact x | Exact y | Tile |
|---|---|---|---|
| North-west (77.206953, 28.615697) | 374584.78 | 218620.29 | x0 = 374584, y0 = 218620 |
| South-east (77.211047, 28.612103) | 374590.74 | 218626.25 | x1 = 374590, y1 = 218626 |

**Count**
```
columns = 374590 − 374584 + 1 = 7
rows    = 218626 − 218620 + 1 = 7
tiles   = 49
```

Each tile covers 256 px × 0.262 m ≈ 67 m. The 400 m box is about 6 tiles wide, and its edges fall partway into tiles on both sides, so 7 tiles are needed in each direction. The downloaded area is therefore slightly larger than the requested box: 7 × 67 m ≈ 470 m.

---

## 5. Stitching logic

The code lives in [`src/tile_stitcher.cpp`](src/tile_stitcher.cpp).

### 5.1 Placing the tiles

1. Read `tile_index.json` to get `zoom`, `tile_size` and `tile_range` (`x0..x1`, `y0..y1`).
2. Create a black image of size
   ```
   width  = (x1 − x0 + 1) × 256
   height = (y1 − y0 + 1) × 256
   ```
   For Delhi this is 7 × 256 = 1792 px on each side.
3. Copy each tile into place:
   ```
   pixel_col = (x − x0) × 256
   pixel_row = (y − y0) × 256
   ```
   ```
   x0=374584        374585        ...  374590
   ┌──────────────┬──────────────┬─ ─ ─┬──────────────┐  y0=218620
   │ (0, 0)       │ (256, 0)     │     │ (1536, 0)    │
   ├──────────────┼──────────────┼─ ─ ─┼──────────────┤  218621
   │ (0, 256)     │ (256, 256)   │     │              │
   ├ ─ ─ ─ ─ ─ ─ ─┼ ─ ─ ─ ─ ─ ─ ─┼─ ─ ─┼ ─ ─ ─ ─ ─ ─ ─┤
   │ (0, 1536)    │              │     │ (1536, 1536) │  y1=218626
   └──────────────┴──────────────┴─ ─ ─┴──────────────┘
   ```
4. Missing tiles stay black, and corrupt tiles are skipped with a warning. Tiles that aren't 256 × 256 are resized. `tiles_placed` / `tiles_expected` in `mosaic.json` show how complete the image is.
5. Save as JPEG (with `--quality`) or PNG.

Tiles are placed edge to edge and never overlap, so no blending or alignment is needed. Every tile is already in the same projection and at the same scale.

### 5.2 Geo-referencing: world file (`.jgw` / `.pgw`)

A world file is six lines of text that turn a pixel `(col, row)` into map coordinates:

```
line 1  A  pixel width in metres        =  mercator_m_per_px
line 2  D  rotation                     =  0
line 3  B  rotation                     =  0
line 4  E  pixel height in metres       = −mercator_m_per_px   (negative: rows go south)
line 5  C  X of the CENTRE of pixel (0,0) = mx + px/2
line 6  F  Y of the CENTRE of pixel (0,0) = my − px/2
```

where `(mx, my)` is the north-west corner of tile `(x0, y0)` from `tileToMercator`, and `px = tile_size_m / 256`. The formula is then:

```
X = C + col × A
Y = F + row × E
```

The values are offset by half a pixel because the world file format describes pixel **centres**, not corners.

### 5.3 Geo-referencing: projection file (`.prj`)

`mosaic.prj` contains the WKT (well-known text) description of EPSG:3857, stored as `kEpsg3857Wkt` in `tile_math.hpp`:

| Part | Meaning |
|---|---|
| `GEOGCS["WGS 84", DATUM["WGS_1984", SPHEROID[..., 6378137, 298.257...]]]` | The underlying lat/lon system is WGS 84 (the same one GPS uses) |
| `PROJECTION["Mercator_1SP"]` | Mercator projection with a single standard parallel |
| `central_meridian 0`, `false_easting/northing 0` | Origin (0, 0) is where the equator meets Greenwich |
| `scale_factor 1`, `UNIT["metre", 1]` | Coordinates are in metres, unscaled |
| `AUTHORITY["EPSG","3857"]` | The official code for this projection |

Web Mercator projects WGS 84 coordinates as if the Earth were a perfect sphere of radius 6,378,137 m. That is why every formula above uses `kEarthRadius` and no ellipsoid corrections.

With the image, `.jgw` and `.prj` in the same folder, QGIS or `gdalinfo mosaic.jpg` place the image at the correct location without any further setup.

### 5.4 Converting a mosaic pixel to GPS coordinates

The map matcher finds where the drone image lands in the mosaic. To turn that pixel `(col, row)` into latitude/longitude:

```cpp
#include "global_map/tile_math.hpp"
using namespace global_map;

// x0, y0, zoom: from tile_index.json → tile_range / zoom
LonLat p = tileToLonLat(x0 + col / 256.0, y0 + row / 256.0, zoom);
// p.lon, p.lat
```

The calculation works the other way as well: `lonlatToTile` without the `floor` gives the exact fractional tile, and `(fx − x0) × 256` is the pixel column.

---

## 6. Code layout

```
GlobalMap/cpp/
├── CMakeLists.txt
├── include/global_map/
│   ├── tile_math.hpp          tile/Mercator formulas (header-only, reusable)
│   └── tile_stitcher.hpp      stitch(), resolveMapDir()
└── src/
    ├── tile_stitcher.cpp      stitching + world file / .prj / .json output
    ├── tile_stitcher_main.cpp tile_stitcher command-line program
    └── map_downloader_main.cpp map_downloader command-line program
```

The downloader starts `--workers` threads. Each thread takes the next tile number from a shared atomic counter and uses its own libcurl handle. Each tile is tried up to four times, waiting 1, 2, 4 and 8 seconds after each failure. A 404 response is recorded as `missing` and is not retried. A tile is first written as `.part` and then renamed, so an interrupted download never leaves a half-written tile that would later be counted as cached.

## 7. Limits and notes

- **Area size:** the number of tiles grows ×4 with each zoom level. All of India at zoom 19 is about 690 million tiles (~13 TB), so download only the flight area plus a margin. Use `--dry-run` first.
- **Mosaic size:** `--mosaic` stops at 4096 tiles (16k × 16k px), and `tile_stitcher` at 400 million pixels. For larger areas, work directly from the tile folder using `tile_index.json`.
- **Imagery terms:** Esri World Imagery is fine for research, but its terms restrict bulk downloading and offline caching. Check the terms before commercial use, or use `--source` with a licensed or open imagery server. Google tiles are not supported, because their terms forbid bulk downloading.
- **Imagery age:** satellite images can be months or years old. Construction, seasons and shadows will differ from what the drone camera sees.
