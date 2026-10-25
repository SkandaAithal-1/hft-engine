#pragma once

#include <cstdint>
#include <variant>
#include <optional>
#include <string>

#include <hft/order_book/types.hpp>

enum class DecodeError
{
    NO_ERROR,
    INCOMPLETE_FRAME,
    INVALID_FRAME_LENGTH,
    FRAME_TOO_LARGE,
    UNSUPPORTED_VERSION,
    UNKNOWN_MESSAGE_TYPE,
    INVALID_PAYLOAD_LENGTH,
    INVALID_SIDE,
};

enum class Version : std::uint8_t
{
    VERSION_1 = 0x01
};

enum class MessageType : std::uint8_t
{
    LIMIT_ORDER_MESSAGE = 0x01,
    CANCEL_ORDER_MESSAGE = 0x02
};

struct LimitOrderMessage
{
    OrderID id;
    Side side;
    Price price;
    Quantity quantity;
};

struct CancelOrderMessage
{
    OrderID id;
};

using ProtocolMessage = std::variant<LimitOrderMessage, CancelOrderMessage>;

struct DecodeResult
{
    std::optional<ProtocolMessage> message;
    DecodeError error;

    bool ok() const noexcept
    {
        return error == DecodeError::NO_ERROR;
    }
};

inline std::string to_string(DecodeError error)
{
    switch (error)
    {
    case DecodeError::FRAME_TOO_LARGE:
        return std::string("FRAME TOO LARGE");
    case DecodeError::INCOMPLETE_FRAME:
        return std::string("INCOMPLETE FRAME");
    case DecodeError::INVALID_FRAME_LENGTH:
        return std::string("INVALID FRAME LENGTH");
    case DecodeError::INVALID_PAYLOAD_LENGTH:
        return std::string("INVALID PAYLOAD LENGTH");
    case DecodeError::INVALID_SIDE:
        return std::string("INVALID SIDE");
    case DecodeError::UNKNOWN_MESSAGE_TYPE:
        return std::string("UNKNOWN MESSAGE TYPE");
    case DecodeError::UNSUPPORTED_VERSION:
        return std::string("UNSUPPORTED VERSION");
    case DecodeError::NO_ERROR:
        return std::string("NO ERROR");
    }
}