cmake_minimum_required(VERSION 3.10)
project(Checkers)

# Если используем vcpkg, он сам найдёт пути и линковку
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

file(GLOB SOURCES "*.cpp")

add_executable(Checkers WIN32 ${SOURCES})
set(CMAKE_BUILD_TYPE Debug)

target_link_libraries(Checkers PRIVATE
    SDL2::SDL2
    SDL2_image::SDL2_image
    nlohmann_json::nlohmann_json
)

