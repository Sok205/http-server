cmake_minimum_required(VERSION 3.13)
project(http-server-starter-cpp)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)

add_executable(server
        ${SOURCE_FILES}
        external/tracy/public/TracyClient.cpp
)

target_include_directories(server
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external/tracy/public
)

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

target_link_libraries(server
        PRIVATE Threads::Threads ZLIB::ZLIB
)

target_compile_definitions(server PRIVATE TRACY_ENABLE)
