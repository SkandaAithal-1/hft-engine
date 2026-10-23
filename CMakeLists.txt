cmake_minimum_required(VERSION 3.20)

project(hft-engine
    VERSION 1.0
    DESCRIPTION "High-Frequency Trading Engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
add_library(hft_order_gateway
    src/order_gateway/order_gateway.cpp
)
add_library(hft_strategy
    src/strategy/strategy.cpp
)
add_library(hft_simulation
    src/simulation/simulation.cpp
)
add_library(hft_synthetic_market
    src/synthetic_market/synthetic_market.cpp
)
add_library(hft_message_codec
    src/protocol/message_codec.cpp
)
add_library(hft_frame_decoder
    src/network/frame_decoder.cpp
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
target_include_directories(hft_order_gateway
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(hft_strategy
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(hft_simulation
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(hft_synthetic_market
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(hft_message_codec
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(hft_frame_decoder
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
target_link_libraries(hft_simulation
    PUBLIC 
        hft_order_book
        hft_matching_engine
        hft_event_processor
        hft_order_gateway
)
target_link_libraries(hft_message_codec
    PUBLIC 
        hft_order_book
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
add_executable(simulation_tests
    tests/simulation/simulation_tests.cpp
)
add_executable(synthetic_market_tests
    tests/synthetic_market/synthetic_market_tests.cpp
)
add_executable(simulation_synthetic_market_tests
    tests/synthetic_market/simulation_synthetic_market_tests.cpp
)
add_executable(message_codec_tests
    tests/protocol/message_codec_tests.cpp
)
add_executable(frame_decoder_tests
    tests/network/frame_decoder_tests.cpp 
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
target_link_libraries(simulation_tests
    PRIVATE 
        hft_simulation
        hft_strategy
        Catch2::Catch2WithMain
)
target_link_libraries(synthetic_market_tests
    PRIVATE 
        hft_synthetic_market
        Catch2::Catch2WithMain
)
target_link_libraries(simulation_synthetic_market_tests
    PRIVATE 
        hft_synthetic_market
        hft_simulation
        hft_strategy
        Catch2::Catch2WithMain
)
target_link_libraries(message_codec_tests
    PRIVATE 
        hft_message_codec
        Catch2::Catch2WithMain
)
target_link_libraries(frame_decoder_tests
    PRIVATE 
        hft_frame_decoder
        Catch2::Catch2WithMain
)


include(Catch)

catch_discover_tests(order_book_tests)
catch_discover_tests(matching_engine_tests)
catch_discover_tests(event_processor_tests)
catch_discover_tests(simulation_tests)
catch_discover_tests(synthetic_market_tests)
catch_discover_tests(simulation_synthetic_market_tests)
catch_discover_tests(message_codec_tests)
catch_discover_tests(frame_decoder_tests)

include(FetchContent)

option(HFT_BUILD_BENCHMARKS
    "Build HFT project benchmarks"
    ON
)

if (HFT_BUILD_BENCHMARKS)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        google_benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.4
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(google_benchmark)

    add_subdirectory(benchmarks)
endif()