cmake_minimum_required(VERSION 3.16)
project(global_map LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenCV REQUIRED COMPONENTS core imgcodecs imgproc highgui)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(nlohmann_json 3 QUIET)

add_library(global_map src/tile_stitcher.cpp src/map_image.cpp)
target_include_directories(global_map PUBLIC include)
target_link_libraries(global_map PUBLIC ${OpenCV_LIBS})
if(nlohmann_json_FOUND)
  target_link_libraries(global_map PUBLIC nlohmann_json::nlohmann_json)
endif()
target_compile_options(global_map PRIVATE -Wall -Wextra)

add_executable(tile_stitcher src/tile_stitcher_main.cpp)
target_link_libraries(tile_stitcher PRIVATE global_map)

add_executable(map_info src/map_info_main.cpp)
target_link_libraries(map_info PRIVATE global_map)
target_compile_options(map_info PRIVATE -Wall -Wextra)

add_executable(map_downloader src/map_downloader_main.cpp)
target_link_libraries(map_downloader PRIVATE global_map CURL::libcurl Threads::Threads)
target_compile_options(map_downloader PRIVATE -Wall -Wextra)

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
add_executable(map_ui src/map_ui.cpp)
target_link_libraries(map_ui PRIVATE GLUT::GLUT OpenGL::GL OpenGL::GLU ${OpenCV_LIBS})
target_compile_options(map_ui PRIVATE -Wall -Wextra)


add_executable(drone_view src/drone_view.cpp)
target_link_libraries(drone_view PRIVATE GLUT::GLUT OpenGL::GL OpenGL::GLU ${OpenCV_LIBS})
target_compile_options(drone_view PRIVATE -Wall -Wextra)


# add_executable(map_map_image src/map_image.cpp)
# target_link_libraries(map_map_image PRIVATE global_map CURL::libcurl Threads::Threads)
# target_compile_options(map_map_image PRIVATE -Wall -Wextra)

find_path(TURBOJPEG_INCLUDE_DIR turbojpeg.h)
find_library(TURBOJPEG_LIBRARY turbojpeg)
if(TURBOJPEG_INCLUDE_DIR AND TURBOJPEG_LIBRARY)
  add_executable(load_tile src/load_tile.cpp)
  target_include_directories(load_tile PRIVATE ${TURBOJPEG_INCLUDE_DIR})
  target_link_libraries(load_tile PRIVATE ${OpenCV_LIBS} ${TURBOJPEG_LIBRARY})
  target_compile_options(load_tile PRIVATE -Wall -Wextra)



add_executable(load_gray_tile src/load_gray_tile.cpp)
  target_include_directories(load_gray_tile PRIVATE ${TURBOJPEG_INCLUDE_DIR})
  target_link_libraries(load_gray_tile PRIVATE ${OpenCV_LIBS} ${TURBOJPEG_LIBRARY})
  target_compile_options(load_gray_tile PRIVATE -Wall -Wextra)



else()
  message(WARNING "libturbojpeg not found, skipping load_tile (sudo apt install libturbojpeg0-dev)")
endif()





add_executable(load_random src/load_random.cpp)
  target_include_directories(load_random PRIVATE ${TURBOJPEG_INCLUDE_DIR})
  target_link_libraries(load_random PRIVATE ${OpenCV_LIBS} ${TURBOJPEG_LIBRARY})
  target_compile_options(load_random PRIVATE -Wall -Wextra)
