cmake_minimum_required(VERSION 3.10)
project(ffmpeg_converter)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(nlohmann_json QUIET)

# Add the executable
add_executable(ffmpeg_converter ffmpeg_converter.cpp)

# If nlohmann_json was found as a package, use it
if(nlohmann_json_FOUND)
    target_link_libraries(ffmpeg_converter PRIVATE nlohmann_json::nlohmann_json)
else()
    # If not found, download it at configure time
    include(FetchContent)
    FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
    )
    FetchContent_MakeAvailable(json)
    target_link_libraries(ffmpeg_converter PRIVATE nlohmann_json::nlohmann_json)
endif()

# Install
install(TARGETS ffmpeg_converter DESTINATION bin)
