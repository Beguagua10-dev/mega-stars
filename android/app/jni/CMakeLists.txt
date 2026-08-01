cmake_minimum_required(VERSION 3.18)
project(mega_stars_android)

include(FetchContent)

# SDL2 is fetched instead of vendored to keep the repository small.
FetchContent_Declare(
    SDL2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz
    URL_HASH SHA256=24b574f71c87a763f50704bbb630cbe38298d544a1f890f099a4696b1d6beba4
)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../.. mega-stars)
