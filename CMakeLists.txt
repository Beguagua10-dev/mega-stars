cmake_minimum_required(VERSION 3.16)
project(MegaStars VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# macOS: 10.13 High Sierra is the last release for the 2011 iMac, so that is
# the deployment target for the Mac build of the PC Edition.
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

option(MEGA_BUILD_PC "Build the PC Edition" ON)
option(MEGA_BUILD_MOBILE "Build the Mobile Edition" OFF)
option(MEGA_BUILD_POCKET "Build the Pocket Edition" OFF)
option(MEGA_BUILD_TESTS "Build the core tests" ON)
set(MEGA_EOS_SDK "" CACHE PATH "Path to the extracted Epic Online Services SDK")

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------- core -------
add_library(mega_core STATIC
    core/src/Arena.cpp
    core/src/Brawlers.cpp
    core/src/BotBrain.cpp
    core/src/World.cpp
)
target_include_directories(mega_core PUBLIC core/include)

# ----------------------------------------------------------------- net -------
add_library(mega_net STATIC
    platform/net/src/Json.cpp
    platform/net/src/NetClient.cpp
    platform/eos/src/EosNetClient.cpp
)
target_include_directories(mega_net PUBLIC platform/net/include)
target_link_libraries(mega_net PUBLIC mega_core)
if(WIN32)
    target_link_libraries(mega_net PUBLIC ws2_32)
endif()

if(MEGA_EOS_SDK)
    if(NOT EXISTS "${MEGA_EOS_SDK}/SDK/Include/eos_sdk.h")
        message(FATAL_ERROR "MEGA_EOS_SDK=${MEGA_EOS_SDK} does not contain SDK/Include/eos_sdk.h")
    endif()
    target_compile_definitions(mega_net PUBLIC MEGA_WITH_EOS)
    target_include_directories(mega_net PUBLIC "${MEGA_EOS_SDK}/SDK/Include")
    find_library(EOS_LIBRARY NAMES EOSSDK-Win64-Shipping EOSSDK-Mac-Shipping libEOSSDK-Linux-Shipping.so
                 HINTS "${MEGA_EOS_SDK}/SDK/Lib" "${MEGA_EOS_SDK}/SDK/Bin")
    if(EOS_LIBRARY)
        target_link_libraries(mega_net PUBLIC ${EOS_LIBRARY})
    else()
        message(WARNING "EOS SDK headers found but no import library; link it manually")
    endif()
endif()

# ----------------------------------------------------------------- app -------
add_library(mega_app STATIC
    app/src/AsciiRenderer.cpp
    app/src/Game.cpp
    app/src/Profile.cpp
    app/src/SdlRenderer.cpp
)
target_include_directories(mega_app PUBLIC app/include)
target_link_libraries(mega_app PUBLIC mega_core mega_net)

find_package(SDL2 QUIET)
if(SDL2_FOUND)
    target_compile_definitions(mega_app PUBLIC MEGA_WITH_SDL)
    # SDL2main is deliberately not linked: the editions are console programs
    # with a plain main(), so they also work from a terminal/PowerShell.
    if(TARGET SDL2::SDL2)
        target_link_libraries(mega_app PUBLIC SDL2::SDL2)
    else()
        target_include_directories(mega_app PUBLIC ${SDL2_INCLUDE_DIRS})
        target_link_libraries(mega_app PUBLIC ${SDL2_LIBRARIES})
    endif()
else()
    message(STATUS "SDL2 not found: only the ASCII frontend will be available")
endif()

# ------------------------------------------------------------- editions ------
if(MEGA_BUILD_PC)
    add_executable(mega-stars editions/pc/src/main.cpp)
    target_link_libraries(mega-stars PRIVATE mega_app)
endif()

if(MEGA_BUILD_MOBILE)
    add_executable(mega-stars-mobile editions/mobile/src/main.cpp)
    target_link_libraries(mega-stars-mobile PRIVATE mega_app)
endif()

if(MEGA_BUILD_POCKET)
    add_executable(mega-stars-pocket editions/pocket/src/main.cpp)
    target_link_libraries(mega-stars-pocket PRIVATE mega_app)
endif()

if(MEGA_BUILD_TESTS)
    enable_testing()
    add_executable(mega_core_tests tests/core_tests.cpp)
    target_link_libraries(mega_core_tests PRIVATE mega_core mega_net)
    add_test(NAME core COMMAND mega_core_tests)
endif()
