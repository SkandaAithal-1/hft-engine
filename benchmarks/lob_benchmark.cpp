#include <benchmark/benchmark.h>

#include <hft/order_book/order_book.hpp>
#include <hft/order_book/types.hpp>

#include "benchmark_utils.hpp"

namespace
{
    void BM_InsertExistingLevel(benchmark::State &state)
    {
        const auto initial_orders = static_cast<std::size_t>(state.range(0));
        constexpr std::size_t kBatchSize = 1000;
        constexpr Price kPrice = 100;
        constexpr OrderID kFirstInsertedId = 1'000'000;

        for (auto _ : state)
        {
            state.PauseTiming();
            {
                OrderBook book(initial_orders + kBatchSize);

                // Setup which is excluded from the timed region
                benchmark_utils::populate_same_level(book, initial_orders, Side::Buy, kPrice);

                state.ResumeTiming();
                for (std::size_t i = 0; i < kBatchSize; ++i)
                {
                    const auto id = kFirstInsertedId + static_cast<OrderID>(i);

                    book.add_order(id, Side::Buy, kPrice, benchmark_utils::kOrderQuantity, benchmark_utils::kOrderQuantity);
                }

                benchmark::DoNotOptimize(book);
                benchmark::ClobberMemory();

                state.PauseTiming();
            }
            state.ResumeTiming();
        }

        state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(kBatchSize));
    }

    void BM_InsertNewLevel(benchmark::State &state)
    {
        const auto initial_levels = static_cast<std::size_t>(state.range(0));

        constexpr std::size_t kBatchSize = 1000;
        constexpr Price kFirstPrice = 100;
        constexpr OrderID kFirstInsertedId = 1'000'000;

        for (auto _ : state)
        {
            state.PauseTiming();
            {
                OrderBook book(initial_levels + kBatchSize);

                benchmark_utils::populate_distinct_levels(
                    book,
                    initial_levels,
                    Side::Buy,
                    kFirstPrice);

                // Start beyond every preloaded price.
                const Price first_inserted_price =
                    kFirstPrice + static_cast<Price>(initial_levels);

                state.ResumeTiming();

                for (std::size_t i = 0; i < kBatchSize; ++i)
                {
                    const auto id =
                        kFirstInsertedId + static_cast<OrderID>(i);
                    const auto price =
                        first_inserted_price + static_cast<Price>(i);

                    book.add_order(
                        id,
                        Side::Buy,
                        price,
                        benchmark_utils::kOrderQuantity,
                        benchmark_utils::kOrderQuantity);
                }

                benchmark::DoNotOptimize(book);
                benchmark::ClobberMemory();
                state.PauseTiming();
            }
            state.ResumeTiming();
        }

        state.SetItemsProcessed(
            state.iterations() * static_cast<std::int64_t>(kBatchSize));
    }

    void BM_CancelById(benchmark::State &state)
    {
        const auto order_count =
            static_cast<std::size_t>(state.range(0));

        for (auto _ : state)
        {
            state.PauseTiming();
            {
                OrderBook book;

                benchmark_utils::populate_same_level(
                    book,
                    order_count,
                    Side::Buy,
                    100);

                const auto ids =
                    benchmark_utils::make_order_ids(order_count);

                state.ResumeTiming();

                for (const auto id : ids)
                {
                    const auto result = book.cancel_order(id);
                    benchmark::DoNotOptimize(result);
                }

                benchmark::DoNotOptimize(book);
                benchmark::ClobberMemory();

                state.PauseTiming();
            }
            state.ResumeTiming();
        }

        state.SetItemsProcessed(
            state.iterations() * static_cast<std::int64_t>(order_count));
    }

    void BM_CancelWithinLevel(benchmark::State &state)
    {
        const auto order_count =
            static_cast<std::size_t>(state.range(0));

        // Leave one order alive so the price level never becomes empty
        // during the timed cancellation batch.
        const auto timed_cancel_count = order_count - 1;

        constexpr Side kSide = Side::Buy;
        constexpr Price kPrice = 100;

        for (auto _ : state)
        {
            state.PauseTiming();
            {
                OrderBook book(order_count);

                benchmark_utils::populate_same_level(
                    book,
                    order_count,
                    kSide,
                    kPrice);

                const auto ids =
                    benchmark_utils::make_order_ids(order_count);

                state.ResumeTiming();

                for (std::size_t i = 0; i < timed_cancel_count; ++i)
                {
                    const auto result = book.cancel_order(ids[i]);
                    benchmark::DoNotOptimize(result);
                }

                benchmark::DoNotOptimize(book);
                benchmark::ClobberMemory();

                state.PauseTiming();
            }
            state.ResumeTiming();
        }

        state.SetItemsProcessed(
            state.iterations() *
            static_cast<std::int64_t>(timed_cancel_count));
    }

    void BM_CancelFinalOrderAtLevel(benchmark::State &state)
    {
        const auto order_count =
            static_cast<std::size_t>(state.range(0));

        constexpr Side kSide = Side::Buy;
        constexpr Price kFirstPrice = 100;

        for (auto _ : state)
        {
            state.PauseTiming();
            {
                OrderBook book(order_count);

                // Each order occupies its own price level.
                benchmark_utils::populate_distinct_levels(
                    book,
                    order_count,
                    kSide,
                    kFirstPrice);

                const auto ids =
                    benchmark_utils::make_order_ids(order_count);

                state.ResumeTiming();

                for (const auto id : ids)
                {
                    const auto result = book.cancel_order(id);
                    benchmark::DoNotOptimize(result);
                }

                benchmark::DoNotOptimize(book);
                benchmark::ClobberMemory();

                state.PauseTiming();
            }
            state.ResumeTiming();
        }

        state.SetItemsProcessed(
            state.iterations() *
            static_cast<std::int64_t>(order_count));
    }

    static void BM_BestBid(benchmark::State &state)
    {
        const std::size_t kLevels = static_cast<std::size_t>(state.range(0));
        constexpr Price kFirstPrice = 100;

        state.PauseTiming();
        {
            OrderBook book;
            benchmark_utils::populate_distinct_levels(
                book, kLevels, Side::Buy, kFirstPrice);

            state.ResumeTiming();

            for (auto _ : state)
            {
                auto best = book.best_bid();
                benchmark::DoNotOptimize(best);
            }
        }

        state.SetItemsProcessed(state.iterations());
        state.ResumeTiming();
    }

    static void BM_BestAsk(benchmark::State &state)
    {
        const std::size_t kLevels = static_cast<std::size_t>(state.range(0));
        constexpr Price kFirstPrice = 100;

        state.PauseTiming();

        {
            OrderBook book;

            benchmark_utils::populate_distinct_levels(
                book, kLevels, Side::Sell, kFirstPrice);

            state.ResumeTiming();

            for (auto _ : state)
            {
                auto best = book.best_ask();
                benchmark::DoNotOptimize(best);
            }

            state.PauseTiming();
        }

        state.SetItemsProcessed(state.iterations());
        state.ResumeTiming();
    }

    static void BM_MixedInsertCancel(benchmark::State &state)
    {
        const auto count = static_cast<std::size_t>(state.range(0));

        constexpr Price kPrice = 100;
        constexpr OrderID kNewOrderId = 1'000'000;

        for (auto _ : state)
        {
            state.PauseTiming();
            {
                OrderBook book(2*count);

                benchmark_utils::populate_same_level(
                    book, count, Side::Buy, kPrice);

                state.ResumeTiming();

                for (std::size_t i = 0; i < count; ++i)
                {
                    book.add_order(
                        kNewOrderId + i,
                        Side::Buy,
                        kPrice,
                        benchmark_utils::kOrderQuantity,
                        benchmark_utils::kOrderQuantity);

                    auto result = book.cancel_order(
                        static_cast<OrderID>(i + 1));

                    benchmark::DoNotOptimize(
                        static_cast<int>(result));
                }

                benchmark::DoNotOptimize(book);
                benchmark::ClobberMemory();

                state.PauseTiming();
            }
            state.ResumeTiming();
        }

        state.SetItemsProcessed(
            state.iterations() * static_cast<int64_t>(count) * 2);
    }

    BENCHMARK(BM_InsertExistingLevel)->Arg(10)->Arg(100)->Arg(1000);

    BENCHMARK(BM_InsertNewLevel)->Arg(10)->Arg(100)->Arg(1000);

    BENCHMARK(BM_CancelById)->Arg(10)->Arg(100)->Arg(1000);

    BENCHMARK(BM_CancelWithinLevel)->Arg(10)->Arg(100)->Arg(1000);

    BENCHMARK(BM_CancelFinalOrderAtLevel)->Arg(10)->Arg(100)->Arg(1000);

    BENCHMARK(BM_BestBid)->Arg(10)->Arg(100)->Arg(1000);

    BENCHMARK(BM_BestAsk)->Arg(10)->Arg(100)->Arg(1000);

    BENCHMARK(BM_MixedInsertCancel)->Arg(100)->Arg(1000)->Arg(10000);
} // namespace

BENCHMARK_MAIN();