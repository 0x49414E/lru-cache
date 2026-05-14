cmake_minimum_required(VERSION 3.20...4.3.1)
project(lru_cache_sv VERSION 1.0
                     DESCRIPTION "Simple LRU cache server"
                     LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost REQUIRED)

add_subdirectory(src)
add_subdirectory(app)


