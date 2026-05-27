cmake_minimum_required(VERSION 3.16)
project(doom LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_executable(doom src/main.cpp src/palette.cpp src/screen.cpp src/wad.cpp)
target_link_libraries(doom PRIVATE PkgConfig::SDL2)
