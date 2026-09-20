cmake_minimum_required(VERSION 3.20)

project(hft-engine
    VERSION 1.0
    DESCRIPTION "High-Frequency Trading Engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Library
# ------------------------------------------------------------

add_library(hft_order_book 
    src/order_book/order_book.cpp
)
add_library(hft_matching_engine
    src/matching_engine/matching_engine.cpp
)
add_library(hft_event_processor
    src/event/event_processor.cpp
)

target_include_directories(hft_order_book
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(hft_matching_engine
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(hft_event_processor
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(hft_matching_engine
    PUBLIC 
        hft_order_book
)
target_link_libraries(hft_event_processor
    PUBLIC 
        hft_matching_engine
)

# ------------------------------------------------------------
# Tests
# ------------------------------------------------------------

include(CTest)
enable_testing()

find_package(Catch2 3 REQUIRED)

add_executable(order_book_tests
    tests/order_book/order_book_tests.cpp
)
add_executable(matching_engine_tests
    tests/matching_engine/matching_engine_tests.cpp
)
add_executable(event_processor_tests
    tests/event/event_processor_tests.cpp
)

target_link_libraries(order_book_tests
    PRIVATE
        hft_order_book
        Catch2::Catch2WithMain
)
target_link_libraries(matching_engine_tests
    PRIVATE 
        hft_matching_engine
        Catch2::Catch2WithMain
)
target_link_libraries(event_processor_tests
    PRIVATE 
        hft_event_processor 
        Catch2::Catch2WithMain
)


include(Catch)

catch_discover_tests(order_book_tests)
catch_discover_tests(matching_engine_tests)
catch_discover_tests(event_processor_tests)