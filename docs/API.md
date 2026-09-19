# M1 Design Choices

## Summary

- `std::map<Price, PriceLevel>` for price levels
- `std::list<Order>` inside each price level for FIFO queues
- `std::unordered_map<OrderID, OrderHandle>` for direct order lookup and cancellation

This is the **M1 baseline**. This is intentionally simple and easy to implement, test and benchmark and later optimize.

## Core Data Model

### Order structure

```
using OrderID = uint64_t;
using Price = uint64_t;
using Quantity = uint64_t;
enum class Side {
    Buy, Sell
}

struct Order {
    OrderID id;
    Side side;
    Price price;
    Quantity quantity;
    Quantity remaining;
    uint64_t sequence;
}
```

`sequence` represents deterministic arrival order and is useful for audit/debugging. The actual queue structure already preserves FIFO ordering.

### PriceLevel structure

```
struct PriceLevel {
    std::list<Order> orders;
    Quantity total_quantity;
}
```

### Book-Side structure

```
bids: std::map<Price, PriceLevel>
asks: std::map<Price, PriceLevel>
```

### Order-ID index

```
struct OrderHandle {
    Price price;
    std::list<Order>::iterator iterator;
}

std::unordered_map<OrderID, OrderHandle> order_index;
```

## Justification for `std::list` instead of `std::deque`

A `std::deque` has efficient append and front removal and has better locality than a linked list. However, cancellation can remove an arbitrary order from the middle of a price-level queue.  
With a `std::deque`, arbitrary removal would be **O(n)**.
With `std::list`, we can achieve **O(1)** removal.

## Expected Complexity

Let `P` be the number of active price levels.

| Operation                  | Expected Complexity |
| -------------------------- | ------------------: |
| Insert order               |            O(log P) |
| Find order by ID           |        Average O(1) |
| Cancel order               |        Average O(1) |
| Append to price-level FIFO |                O(1) |
| Remove front order         |                O(1) |
| Remove arbitrary order     |                O(1) |
| Best bid                   |                O(1) |
| Best ask                   |                O(1) |
| Quantity at price          |            O(log P) |
| Create/remove price level  |            O(log P) |

## LOB API

Initially it should be small.

```
AddResult add_order(
    OrderID id,
    Side side,
    Price price,
    Quantity quantity
);

CancelResult cancel_order(OrderID id);

std::optional<Price> best_bid() const;

std::optional<Price> best_ask() const;

Quantity quantity_at(
    Side side,
    Price price
) const;

std::size_t order_count_at(
    Side side,
    Price price
) const;

const Order* get_order(OrderID id) const;
```

**Execution API**
```
ReduceResult reduce_order(OrderID id, Quantity quantity);
```

**Possible result types:**

```
enum class AddResult {
    ACCEPTED, DUPLICATE_ORDER_ID, INVALID_PRICE, INVALID_QUANTITY
}

enum class CancelResult {
    CANCELLED, NOT_FOUND
}

enum class ReduceResult {
    REDUCED, NOT_FOUND, INVALID_QUANTITY
}
```

## Important API Boundary

The LOB **does not** perform matching. The matching engine does this separately.

## Design Summary

```text
                         Limit Order Book
                                │
                 ┌──────────────┴──────────────┐
                 │                             │
               Bids                           Asks
                 │                             │
          std::map<Price,               std::map<Price,
                    PriceLevel>                  PriceLevel>
                 │                             │
            PriceLevel                    PriceLevel
                 │                             │
           std::list<Order>              std::list<Order>


                   Order ID Index
                         │
                 std::unordered_map
                         │
                    OrderHandle
                         │
                 ┌───────┴───────┐
                 │               │
               Price        list iterator
```

## Matching Engine API

Initially this should be small

```
struct Trade {
    OrderID aggressor_id;
    OrderID resting_id;
    Price price;
    Quantity quantity;
};

using TradeSink = std::vector<Trade>;
```

```
MatchingResult process_order(OrderID id, Side side, Price price, Quantity quantity);
```

**Possible result types**
```
enum class MatchingResult {
    RESTING, PARTIALLY_FILLED, FULLY_FILLED, INVALID_PRICE, INVALID_QUANTITY, DUPLICATE_ORDER_ID
}
```

## Event Loop 
```
        incoming_order
              |
        process_order()
              |
         trade_queue
              |
        consume/publish
              |
          next_order
```
