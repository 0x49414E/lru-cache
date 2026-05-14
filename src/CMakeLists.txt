# src/CMakeLists.txt

add_library(lru_lib
        net/server.cpp
        net/lru_server.cpp
        core/lru_cache_service.cpp
)

target_include_directories(lru_lib
        PUBLIC  ${PROJECT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(lru_lib
        PUBLIC Boost::headers
)