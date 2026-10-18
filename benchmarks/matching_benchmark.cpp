
#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>

#include <hft/matching_engine/matching_engine.hpp>
#include <hft/order_book/order_book.hpp>
#include <hft/order_book/types.hpp>

namespace
{
constexpr Price kBasePrice = 100;
constexpr Quantity kRestingQuantity = 10;
constexpr OrderID kIncomingOrderId = 1'000'000;

void BM_RestingOrder(benchmark::State& state)
{
    const auto levels =
        static_cast<std::size_t>(state.range(0));

    const auto target_price =
        static_cast<Price>(kBasePrice + levels);

    OrderBook book;
    TradeSink trades;
    MatchingEngine engine(book, trades);

    // Populate unrelated bid levels.
    for (std::size_t i = 0; i < levels; ++i)
    {
        book.add_order(
            static_cast<OrderID>(i + 1),
            Side::Buy,
            static_cast<Price>(kBasePrice + i),
            kRestingQuantity,
            kRestingQuantity);
    }

    // Keep the target price level alive.
    book.add_order(
        static_cast<OrderID>(levels + 1),
        Side::Buy,
        target_price,
        kRestingQuantity,
        kRestingQuantity);

    for (auto _ : state)
    {
        const auto result = engine.process_order(
            kIncomingOrderId,
            Side::Buy,
            target_price,
            kRestingQuantity);

        benchmark::DoNotOptimize(static_cast<int>(result));

        state.PauseTiming();
        engine.cancel_order(kIncomingOrderId);
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations());
}

void BM_PartialFill(benchmark::State& state)
{
    const auto levels =
        static_cast<std::size_t>(state.range(0));

    OrderBook book;
    TradeSink trades;
    MatchingEngine engine(book, trades);

    // Best ask that will be partially consumed.
    book.add_order(
        1, Side::Sell, kBasePrice,
        kRestingQuantity, kRestingQuantity);

    // Unrelated asks at worse prices.
    for (std::size_t i = 0; i < levels; ++i)
    {
        book.add_order(
            static_cast<OrderID>(i + 2),
            Side::Sell,
            static_cast<Price>(kBasePrice + 1 + i),
            kRestingQuantity,
            kRestingQuantity);
    }

    for (auto _ : state)
    {
        // Fills the best ask and rests the remainder of 5.
        const auto result = engine.process_order(
            kIncomingOrderId,
            Side::Buy,
            kBasePrice,
            kRestingQuantity + 5);

        benchmark::DoNotOptimize(static_cast<int>(result));

        state.PauseTiming();

        // Remove the partially filled incoming order.
        engine.cancel_order(kIncomingOrderId);

        // Restore the consumed ask.
        book.add_order(
            1, Side::Sell, kBasePrice,
            kRestingQuantity, kRestingQuantity);

        trades.clear();

        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations());
}

void BM_FullFill(benchmark::State& state)
{
    const auto levels =
        static_cast<std::size_t>(state.range(0));

    OrderBook book;
    TradeSink trades;
    MatchingEngine engine(book, trades);

    // Best ask that will be fully consumed.
    book.add_order(
        1, Side::Sell, kBasePrice,
        kRestingQuantity, kRestingQuantity);

    // Unrelated asks at worse prices.
    for (std::size_t i = 0; i < levels; ++i)
    {
        book.add_order(
            static_cast<OrderID>(i + 2),
            Side::Sell,
            static_cast<Price>(kBasePrice + 1 + i),
            kRestingQuantity,
            kRestingQuantity);
    }

    for (auto _ : state)
    {
        const auto result = engine.process_order(
            kIncomingOrderId,
            Side::Buy,
            kBasePrice,
            kRestingQuantity);

        benchmark::DoNotOptimize(static_cast<int>(result));

        state.PauseTiming();

        // Restore the consumed best ask.
        book.add_order(
            1, Side::Sell, kBasePrice,
            kRestingQuantity, kRestingQuantity);

        trades.clear();

        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations());
}

void BM_MultiLevelFill(benchmark::State& state)
{
    const auto levels =
        static_cast<std::size_t>(state.range(0));

    const auto total_quantity =
        static_cast<Quantity>(levels) * kRestingQuantity;

    const auto limit_price =
        static_cast<Price>(kBasePrice + levels);

    OrderBook book;
    TradeSink trades;
    MatchingEngine engine(book, trades);

    // One resting ask at each distinct price.
    for (std::size_t i = 0; i < levels; ++i)
    {
        book.add_order(
            static_cast<OrderID>(i + 1),
            Side::Sell,
            static_cast<Price>(kBasePrice + 1 + i),
            kRestingQuantity,
            kRestingQuantity);
    }

    for (auto _ : state)
    {
        // Consumes all configured levels.
        const auto result = engine.process_order(
            kIncomingOrderId,
            Side::Buy,
            limit_price,
            total_quantity);

        benchmark::DoNotOptimize(static_cast<int>(result));

        state.PauseTiming();

        // Restore all consumed asks.
        for (std::size_t i = 0; i < levels; ++i)
        {
            book.add_order(
                static_cast<OrderID>(i + 1),
                Side::Sell,
                static_cast<Price>(kBasePrice + 1 + i),
                kRestingQuantity,
                kRestingQuantity);
        }

        trades.clear();

        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_RestingOrder)->Arg(10)->Arg(100)->Arg(1000);

BENCHMARK(BM_PartialFill)->Arg(10)->Arg(100)->Arg(1000);

BENCHMARK(BM_FullFill)->Arg(10)->Arg(100)->Arg(1000);

BENCHMARK(BM_MultiLevelFill)->Arg(10)->Arg(100)->Arg(1000);

} // namespace

BENCHMARK_MAIN();
