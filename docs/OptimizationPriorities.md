# M6 --- Low-Latency Optimization Plan

**Project:** HFT order-book and matching-engine simulator\
**Status:** M5 benchmarking and profiling complete; M6 optimization
candidates prioritized\
**Baseline profile:** `passive-1m-callgraph.txt`\
**Hardware-counter run:** `perf-stat-counters.txt`, captured 2026-10-18\
**Primary objective:** Improve end-to-end simulation throughput and
reduce operation cost without sacrificing correctness, determinism, or
maintainability.

------------------------------------------------------------------------

## 1. Scope and operating principles

M5 established the microbenchmarks, end-to-end simulation benchmarks,
and initial profiling baseline. M6 begins changing the implementation.
The M5 results should remain the reference point for every optimization.

The current limit order book (LOB) uses:

-   `std::map` for price levels;
-   `std::list` for FIFO orders within each price level;
-   `std::unordered_map` for order-ID lookup, with handles that identify
    the price level and order-list iterator.

This is a reasonable correctness-oriented baseline. M6 should not
replace these containers wholesale just because they appear in the
profile. Each change must test a specific hypothesis.

### Rules for each experiment

1.  Preserve the M5 baseline and benchmark configuration.
2.  Change one implementation detail at a time.
3.  Run the relevant microbenchmark and the end-to-end simulation
    benchmark.
4.  Run correctness tests and deterministic replay checks.
5.  Record the result, including regressions and memory trade-offs.
6.  Keep a change only if its benefit is reproducible and justifies its
    complexity.

Do not add lock-free structures, concurrency, custom allocators, or a
new container layout without evidence that the simpler experiments are
insufficient.

------------------------------------------------------------------------

## 2. What the existing profile says

The passive 1-million-event call graph attributes the following costs:

  -----------------------------------------------------------------------------------------------------
  Function                                Inclusive cost            Self cost Interpretation
  --------------------------------- -------------------- -------------------- -------------------------
  `Simulation::run`                               88.17%                1.83% Large execution path, but
                                                                              most cost is in its
                                                                              callees

  `EventProcessor::process`                       48.69%                3.06% Significant
                                                                              event-processing path

  `MatchingEngine::process_order`                 31.49%                2.30% Significant matching path

  `OrderBook::add_order`                          19.59%               13.49% High direct cost; first
                                                                              optimization target

  `SyntheticMarket::next`                         12.36%                6.78% Source generation has
                                                                              measurable direct cost

  `publish_market_data`                           11.29%                4.65% Publication/bookkeeping
                                                                              is worth investigating

  `OrderBook::cancel_order`                        8.78%                7.55% High direct cost

  `OrderBook::reduce_order`                        6.65%                4.92% Material direct cost

  `OrderBook::quantity_at`                         5.89%                5.70% Almost all attributed
                                                                              cost is self cost;
                                                                              inspect implementation
                                                                              and call frequency
  -----------------------------------------------------------------------------------------------------

**Important:** inclusive percentages overlap and must not be summed.
Self cost is the more useful starting point for ranking direct
implementation targets. These are sampled profile attributions, not
exact per-call latency measurements.

The call graph also shows tree insertion/rebalancing, unordered-map
internals, and allocation-related work beneath `add_order`. This
supports investigating container and allocation costs, but does not by
itself prove which individual change will be fastest.

------------------------------------------------------------------------

## 3. Hardware-counter evidence

The attached `perf stat` output measures:

``` text
./build-prof/benchmarks/simulation_benchmark
  --benchmark_filter=BM_Simulation_Passive/1000000
  --benchmark_min_time=3s
```

The report aggregates three runs.

### Recorded counters

  ------------------------------------------------------------------------
  Counter                            Reported result Initial
                                                     interpretation
  --------------------- ---------------------------- ---------------------
  Task-clock                                 7.496 s About 1.000 CPU
                                                     utilized; consistent
                                                     with a
                                                     single-threaded
                                                     workload

  Context switches                                 0 No context switches
                                                     were recorded for
                                                     this run

  CPU migrations                                   0 No CPU migrations
                                                     were recorded

  Page faults                                332,953 High enough to
                                                     investigate; this
                                                     counter alone does
                                                     not distinguish minor
                                                     from major faults

  Cycles                               28.98 billion About 3.866 GHz
                                                     average over
                                                     task-clock

  Instructions                         51.60 billion 1.78 instructions per
                                                     cycle (IPC)

  Branches                             10.00 billion Large dynamic branch
                                                     count

  Branch misses                        325.4 million 3.25% branch-miss
                                                     rate; branch behavior
                                                     is worth
                                                     investigating

  L1 data-cache loads                  16.01 billion Large number of data
                                                     accesses

  L1 data-cache load                   69.66 million 0.44% reported miss
  misses                                             rate

  LLC loads                                  551,250 Reported count is
                                                     small relative to L1
                                                     accesses

  LLC load misses                            391,818 71.08% of reported
                                                     LLC loads; the
                                                     absolute count is
                                                     comparatively small

  Topdown backend bound                         2.1% Does not indicate a
                                                     dominant
                                                     backend-bound
                                                     bottleneck in this
                                                     report

  Topdown frontend                              6.3% Not the leading
  bound                                              Topdown category
                                                     reported

  Topdown retiring                              5.8% Low reported retiring
                                                     fraction

  Topdown bad                                  85.8% Extremely high
  speculation                                        reported value; must
                                                     be validated before
                                                     treating it as a
                                                     reliable diagnosis
  ------------------------------------------------------------------------

Counter coverage varied by event (roughly 81.8--90.9%), so events were
not all measured with identical coverage. The output also warns that CPU
frequency scaling is enabled. The three reported runs have low variance,
but repeatability alone does not remove systematic counter or frequency
effects.

### What this changes---and what it does not

**It does not justify changing the main order-book optimization order
yet.** The call graph provides the function-level attribution that
`perf stat` cannot. `perf stat` aggregates the whole process and cannot
tell us which function caused a particular counter value.

It does, however, add three diagnostic tasks to M6:

1.  **Validate the Topdown result.** The reported 85.8% bad-speculation
    value is unusually high relative to the 3.25% branch-miss rate. Do
    not conclude that branch misprediction is responsible for 85.8% of
    execution cost without validating the metric, event support, and
    measurement setup.
2.  **Separate page faults by type.** Re-run with `minor-faults` and
    `major-faults` to determine what contributes to the 332,953 total.
    Do not equate this number directly with heap allocation count.
3.  **Avoid premature cache-layout work.** The reported L1 data-cache
    miss rate is low, and backend-bound is reported at 2.1%. These
    results do not currently support making cache locality or memory
    latency the first optimization target. The LLC figures should also
    be interpreted cautiously because the absolute counts are small and
    counter coverage is multiplexed.

A useful follow-up diagnostic run is to collect `cycles`,
`instructions`, `branches`, `branch-misses`, `minor-faults`, and
`major-faults` in a controlled run, then validate Topdown metrics
separately using events supported by the CPU and installed `perf`
version. Keep the same workload and compare against the baseline. Avoid
combining too many events in one group if that causes substantial
multiplexing.

------------------------------------------------------------------------

## 4. Prioritized M6 candidates

### P1 --- `OrderBook::add_order`

**Profile evidence:** 19.59% inclusive, 13.49% self. The call graph
includes red-black-tree insertion/rebalancing, unordered-map insertion
internals, and allocation-related work.

**Hypotheses to test:**

-   Runtime hash-map rehashes contribute to insertion cost.
-   Creating a new price level costs more than inserting into an
    existing level.
-   Allocations for list nodes, tree nodes, or hash-table nodes
    contribute materially.

**First experiment: capacity reservation**

Reserve a realistic expected active-order capacity in the order-ID map,
where that capacity is known from the workload. Compare the current
implementation against the reserved variant.

-   Measure both existing-price-level and new-price-level insertion.
-   Measure end-to-end throughput as well as the microbenchmark.
-   Record memory usage or capacity overhead where possible.
-   Do not reserve an arbitrarily huge capacity: reservation trades
    memory for fewer rehashes and may not help if the workload rarely
    reaches that size.

**Follow-up only if justified:** investigate allocation counts and
consider an order-node pool or allocator strategy. Do not implement a
pool merely because allocation functions appear in the call graph.

### P2 --- `OrderBook::cancel_order`

**Profile evidence:** 8.78% inclusive, 7.55% self.

**Hypotheses to test:**

-   The order-ID handle may already provide the information needed for
    removal, so repeated lookup or validation work may be avoidable.
-   Removing the last order at a price level requires an additional
    price-level tree erase.
-   Cancellation cost may differ significantly between nonempty and
    emptied price levels.

**First experiment:** benchmark cancellation separately for (a) an order
whose price level remains populated and (b) the last order at a price
level. Inspect the implementation for redundant lookup or bookkeeping
before changing it.

**Correctness constraints:** preserve FIFO ordering for remaining
orders, update aggregate quantity exactly once, remove empty levels, and
ensure the ID index never retains an invalid handle.

### P3 --- `OrderBook::reduce_order`

**Profile evidence:** 6.65% inclusive, 4.92% self.

Inspect whether the order handle is reused throughout the operation and
whether the price-level aggregate quantity is updated once. Benchmark
reductions that leave positive remaining quantity separately from
reductions that remove the order entirely, if both paths exist in the
API.

Validate that reduction cannot create negative quantities, and that full
reduction has the same book and index effects as the intended removal
semantics.

### P4 --- `OrderBook::quantity_at`

**Profile evidence:** 5.89% inclusive, 5.70% self.

The high self-cost fraction makes this worth inspecting, but the profile
alone does not reveal the algorithm.

-   If it scans the orders at a level despite an existing aggregate
    quantity, use the aggregate if the API semantics permit it.
-   If it performs a tree lookup, determine whether repeated calls can
    be avoided or whether the call site already has the level.
-   If it is rarely called in the end-to-end workload, a microbenchmark
    speedup may have little total effect.

Do not change the implementation until the code and call sites establish
which case applies.

### P5 --- Allocation overhead (cross-cutting investigation)

Allocation-related functions appear beneath insertion, but the attached
counters do not directly measure allocation count. The high total
page-fault count also does not prove that allocations are the cause.

First measure allocation frequency or distinguish allocation-heavy
benchmark cases. If allocation cost is confirmed, consider a pool for
order nodes or another bounded allocation strategy as a separate
experiment.

Keep object lifetime, iterator stability, cancellation, and reuse
semantics explicit. Reuse must not leave stale order handles or allow
one order's state to leak into another.

### P6 --- `publish_market_data`

**Profile evidence:** 11.29% inclusive, 4.65% self.

Inspect the implementation for:

-   repeated order-ID lookups during trade processing;
-   repeated best-bid/best-ask recomputation for one incoming order that
    produces multiple trades;
-   publication of unchanged BBO state;
-   unnecessary copying or redundant strategy/source bookkeeping.

The existing last-published-update tracking should already suppress
unchanged updates if correctly applied; verify its effectiveness before
adding new state.

**Semantic constraint:** batching publication can change what the
strategy observes. Preserve trade order, callback order, and current
market-data semantics unless a deliberate simulation-model change is
intended.

### P7 --- `SyntheticMarket::next` and RNG

**Profile evidence:** `SyntheticMarket::next` is 12.36% inclusive and
6.78% self. Mersenne Twister accounts for 4.37% self cost in the call
graph.

Inspect active-order selection, source-side bookkeeping, and event
construction before replacing the generator. If RNG cost remains
significant after those checks, benchmark an alternative generator
separately.

**Reproducibility constraint:** changing the RNG or the number/order of
random draws changes the generated event sequence and potentially the
workload distribution. Keep a fixed seed and treat a generator change as
a distinct experiment, not a transparent implementation detail.

------------------------------------------------------------------------

## 5. Execution order

  --------------------------------------------------------------------------------------------
                  Order Change or investigation Why now             Required evaluation
  --------------------- ----------------------- ------------------- --------------------------
                      0 Freeze M5 benchmark     Establish a         Save commands, build
                        results and             trustworthy         configuration, workload
                        configuration           comparison point    parameters, seed, and
                                                                    results

                      1 Reserve a realistic     Small, reversible   Existing-level/new-level
                        order-ID map capacity   test of rehashing   insertion benchmarks;
                                                overhead            end-to-end throughput;
                                                                    memory impact

                      2 Separate cancellation   Cancellation has    Both cancellation
                        paths and inspect       high self cost      microbenchmarks;
                        redundant work                              correctness tests

                      3 Inspect and optimize    Material direct     Reduction microbenchmarks;
                        reduction               cost                quantity/index invariants

                      4 Inspect `quantity_at`   High self-cost      Query microbenchmark and
                        and call sites          fraction            end-to-end impact

                      5 Diagnose                May explain costs   Allocation measurement;
                        allocation/page-fault   across multiple     minor/major fault
                        contribution            operations          counters; targeted
                                                                    benchmarks

                      6 Inspect market-data     Meaningful          Trade-heavy and
                        publication             end-to-end path     multi-level-fill tests;
                                                                    event/callback equivalence

                      7 Inspect source          Meaningful source   Fixed-seed throughput and
                        generation and RNG      overhead, but       output-distribution checks
                                                workload-specific   

                      8 Consider pools or       Higher complexity   Before/after benchmarks,
                        container-layout        and larger          memory measurements, full
                        changes only if         correctness surface correctness suite
                        justified                                   
  --------------------------------------------------------------------------------------------

This ordering is a starting plan, not a commitment to implement every
candidate. If an experiment shows negligible end-to-end improvement,
document the result and move on.

------------------------------------------------------------------------

## 6. Benchmarking and validation protocol

For each change, capture:

-   **Microbenchmark:** operation-specific latency or throughput.
-   **End-to-end benchmark:** events per second on the same passive
    1-million-event workload, plus any relevant active-strategy
    workload.
-   **Correctness:** existing unit tests, invariants, and deterministic
    replay.
-   **Memory:** capacity or allocation impact when relevant.
-   **Environment:** compiler/build type, benchmark command, CPU scaling
    status, and whether the machine was otherwise idle.
-   **Repeatability:** multiple runs and the distribution or variation,
    not only the best run.

Use the same build configuration and benchmark parameters before and
after each change. CPU frequency scaling was enabled in the supplied
run, so avoid over-interpreting small gains. Prefer controlled repeated
runs and compare medians or the benchmark framework's aggregate
statistics.

For deterministic replay, verify not only that the final book matches
but also that emitted trades and observable event ordering remain
equivalent.

### Suggested result log

  ---------------------------------------------------------------------------------------
  Experiment          Microbenchmark   End-to-end   Correctness   Memory      Decision
                      result           result                     impact      
  ------------------- ---------------- ------------ ------------- ----------- -----------
  Baseline            M5 value         M5 value     Pass          Baseline    Frozen

  Reserve order-ID    To measure       To measure   To verify     To measure  Pending
  map                                                                         

  Cancellation-path   To measure       To measure   To verify     To measure  Pending
  change                                                                      

  Further experiments To measure       To measure   To verify     To measure  Pending
  ---------------------------------------------------------------------------------------

------------------------------------------------------------------------

## 7. Explicitly deferred work

Do not begin these solely on the basis of the current profile:

-   replacing `std::map` or `std::list` wholesale;
-   introducing custom allocators or pools before confirming allocation
    cost;
-   lock-free data structures or concurrent matching;
-   cache-line alignment or broad data-layout redesign;
-   replacing the RNG without a controlled performance and
    reproducibility comparison.

These are possible later directions, not requirements for M6.

------------------------------------------------------------------------

## 8. Exit criteria for M6

M6 is complete when:

1.  Each retained optimization has a reproducible before/after result.
2.  The end-to-end benchmark shows the cumulative throughput improvement
    over the frozen M5 baseline.
3.  Correctness and deterministic replay remain intact, or any
    deliberate semantic change is separately documented.
4.  Rejected experiments and their results are recorded.
5.  The final implementation remains understandable and maintainable;
    complexity is introduced only when justified by measured gains.

**Current first action:** run the order-ID map reservation experiment on
the frozen M5 build/configuration, then compare both the relevant
insertion microbenchmarks and the passive end-to-end simulation. In
parallel, validate the anomalous Topdown bad-speculation result before
using it to prioritize branch-oriented changes.

# M6 Optimization Log

## Experiment 01 — Reserve Order ID Index Capacity

**Status:** Retained  
**Target:** `OrderBook::_order_index`  
**Optimization branch:** `m6/01-map-reservation`

### 1. Motivation

Profiling identified `OrderBook::add_order()` as a major cost center, accounting for 19.59% inclusive overhead and 13.49% self time in the recorded passive 1M-event profile. The function inserts order IDs into the `std::unordered_map` order index, which may incur rehashing as the number of active orders grows.

### 2. Hypothesis

Reserving sufficient capacity in the order ID index ahead of insertion should reduce or eliminate rehashing during the expected workload, improving insertion throughput.

### 3. Implementation

Added an optional expected order count to the `OrderBook` constructor:

```cpp
explicit OrderBook(std::size_t expected_order_count = 0);
```

When a nonzero expected count is provided, the constructor reserves capacity in `_order_index`. Existing default construction remains supported.

The LOB insertion benchmarks provide an expected active-order count, including the timed insertion batch. The end-to-end simulation benchmark passes its input event count as the expected count.

### 4. Validation

- Existing LOB and simulation benchmarks completed successfully.
- Isolated LOB insertion benchmarks showed a substantial improvement.
- End-to-end simulation benchmarks did not show a consistent improvement across workload sizes.
- No end-to-end speedup is claimed from the current measurements.

### 5. Decision

**Retain the optimization.** The isolated insertion results justify keeping the low-complexity change. The end-to-end results are inconclusive, so the benefit should be described as an insertion-path improvement rather than a demonstrated overall simulation speedup.

### 6. Trade-offs and limitations

- Reserving capacity may allocate more memory than the workload ultimately requires.
- The expected order count is an estimate of peak simultaneous active orders, not the total number of events processed.
- End-to-end performance depends on other costs, including matching, cancellation, market-data publication, and synthetic event generation.

### 7. Benchmark artifacts

Record the exact baseline and optimized benchmark JSON paths, compiler/build configuration, and commit hashes here.

### 8. Follow-up

Proceed to `cancel_order()` as the next optimization candidate. Benchmark cancellation both when the price level remains populated and when cancellation removes its final order.