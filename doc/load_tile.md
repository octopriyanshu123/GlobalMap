# load_tile

A small standalone tool that loads one map tile (JPEG) from the GlobalMap tile cache, prints its size and shows it in a window. Its main purpose is to test how fast and how correctly tiles decode before the same code is used in the map pipeline.

- Source: [GlobalMap/src/load_tile.cpp](../GlobalMap/src/load_tile.cpp)
- Build target: `load_tile` in [GlobalMap/CMakeLists.txt](../GlobalMap/CMakeLists.txt)
- Tile layout: `GlobalMap/build/data/<map>/tiles/<zoom>/<x>/<y>.jpg`, for example `map_delhi/tiles/19/374514/218548.jpg`

## Usage

```bash
sudo apt install libturbojpeg0-dev        # one-time: TurboJPEG headers
cd GlobalMap/build
cmake .. && make load_tile
./load_tile                               # loads the default tile
./load_tile data/map_delhi/tiles/19/374513/218548.jpg
```

If `turbojpeg.h` is not installed, CMake prints a warning and skips the `load_tile` target. The rest of the project still builds.

## How it works

1. Read the whole `.jpg` file into a memory buffer.
2. `tjDecompressHeader3` reads the width and height without decoding the image.
3. Allocate a `cv::Mat` (`CV_8UC3`) of that size.
4. `tjDecompress2` decodes straight into the Mat's memory as BGR, using `TJFLAG_FASTDCT`.
5. Show the result with `cv::imshow`. This is the only OpenCV dependency besides `cv::Mat`.

## Decisions

### 1. Decode JPEGs with libjpeg-turbo (TurboJPEG API), not `cv::imread`

**Chosen:** TurboJPEG for decoding, with `cv::Mat` kept as the image container.

**Why:**
- **Speed.** TurboJPEG is the SIMD-accelerated (SSE2/AVX2/NEON) JPEG decoder. `cv::imread` often uses the same library underneath, but it adds file-format detection, codec dispatch and extra buffer handling on every call. Calling TurboJPEG directly removes that overhead, which matters when thousands of small tiles (256×256, about 16 KB each) are decoded.
- **Decoding from memory.** It decodes from a byte buffer, so the same function works for tiles read from disk, from a cache or straight from an HTTP download (`map_downloader`), without writing a temporary file.
- **Scaling while decoding.** TurboJPEG can decode at 1/2, 1/4 or 1/8 size almost for free. That is useful for zoomed-out views (see *Future work*).
- **Accuracy is good enough.** `TJFLAG_FASTDCT` trades a tiny amount of precision for speed. The difference can't be seen on satellite or map imagery.
- **No copy into OpenCV.** Decoding happens directly into `cv::Mat::data`, so the result works with the existing OpenCV-based code (`global_map`, `map_ui`, the stitcher) without any conversion.
- **Already available.** The runtime library (`libturbojpeg.so.0`) is already installed on Ubuntu as an OpenCV dependency. Only the `-dev` headers need to be added.

**Alternatives considered:**

| Option | Why it wasn't chosen |
|---|---|
| `cv::imread` (first version) | Simplest option, but it has per-call codec overhead, reads only from a path, and can't scale while decoding. It is still a good fallback. |
| `cv::imdecode` | Decodes from memory, but goes through the same OpenCV codec layer, so it isn't faster. |
| stb_image | A single header with no install needed, but its decoder is plain C without SIMD and noticeably slower than libjpeg-turbo. |
| Plain libjpeg API | The same decoder underneath, but the API is much more verbose (error handling through `setjmp`, scanline loop) for no speed gain. |
| GPU decode (nvJPEG) | Too heavy for small tiles: the upload and launch overhead is larger than the decode time, and it requires CUDA. |

### 2. Keep OpenCV for the container and display

Every other module already uses `cv::Mat`, so keeping it avoids format conversions. `highgui` was added to the OpenCV `find_package` components for `cv::imshow`.

### 3. Make TurboJPEG optional in CMake

`find_path` and `find_library` look for TurboJPEG, and `load_tile` is built only if both are found. This way a machine without the `-dev` package can still build the rest of GlobalMap. CMake prints a warning that includes the install command.

## Future work

- **Reuse one decoder.** When loading many tiles, create a single decoder handle (`tjhandle`) per thread and reuse it, instead of creating one per tile.
- **Use scaled decoding for low zoom.** Pass the scaled width and height (from `tjGetScalingFactors`) to `tjDecompress2` when the view doesn't need full resolution.
- **Move into the library.** Move `loadTile()` into the `global_map` library (for example `tile_loader.{h,cpp}`) so the stitcher and UI use the same path.
- **Benchmark.** Compare TurboJPEG with `cv::imread` on the full Delhi tile set to measure the gain on this hardware.

## Changelog

- **2026-11-02**: First version used `cv::imread` and built with OpenCV only.
- **2026-11-02**: Switched decoding to TurboJPEG (decision 1). Added `highgui` and an optional TurboJPEG lookup in CMake.
