#pragma once

#include <cstdint>
#include <variant>
#include <optional>

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
};