#pragma once

#include <cstdint>
#include <variant>

using OrderID = std::uint64_t;
using Price = std::uint64_t;
using Quantity = std::uint64_t;

enum class Side
{
    Buy,
    Sell
};

enum class AddResult
{
    SUCCESS,
    DUPLICATE_ORDER_ID,
    INVALID_PRICE,
    INVALID_QUANTITY
};

enum class CancelResult
{
    SUCCESS,
    NOT_FOUND
};

enum class MatchingResult
{
    RESTING,
    PARTIALLY_FILLED,
    FULLY_FILLED,
    DUPLICATE_ORDER_ID,
    INVALID_PRICE,
    INVALID_QUANTITY
};

enum class ReduceResult
{
    REDUCED,
    NOT_FOUND,
    INVALID_QUANTITY
};

using ProcessResult = std::variant<MatchingResult, CancelResult>;