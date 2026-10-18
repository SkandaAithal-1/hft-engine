add_executable(lob_benchmark
    lob_benchmark.cpp
)

add_executable(matching_benchmark
    matching_benchmark.cpp)

add_executable(simulation_benchmark
    simulation_benchmark.cpp)

target_link_libraries(lob_benchmark
    PRIVATE 
        hft_order_book
        benchmark::benchmark
)

target_link_libraries(matching_benchmark
    PRIVATE 
        hft_order_book
        hft_matching_engine
        benchmark::benchmark
)

target_link_libraries(simulation_benchmark
    PRIVATE 
        hft_order_book
        hft_matching_engine
        hft_event_processor
        hft_simulation
        hft_synthetic_market
        hft_order_gateway
        hft_strategy
        benchmark::benchmark
)

target_compile_features(lob_benchmark PRIVATE cxx_std_20)
