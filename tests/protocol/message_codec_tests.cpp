#include <catch2/catch_test_macros.hpp>

#include <protocol/message_codec.hpp>

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace
{

using Bytes = std::vector<std::byte>;

Bytes make_frame(std::uint32_t length)
{
    Bytes frame(length, std::byte{0});

    if (length >= 4)
    {
        frame[0] = static_cast<std::byte>((length >> 24) & 0xFF);
        frame[1] = static_cast<std::byte>((length >> 16) & 0xFF);
        frame[2] = static_cast<std::byte>((length >> 8) & 0xFF);
        frame[3] = static_cast<std::byte>(length & 0xFF);
    }

    if (length >= 6)
    {
        frame[4] = static_cast<std::byte>(Version::VERSION_1);
        frame[5] =
            static_cast<std::byte>(MessageType::CANCEL_ORDER_MESSAGE);
    }

    return frame;
}

} // namespace

// -----------------------------------------------------------------------------
// Encoding
// -----------------------------------------------------------------------------

TEST_CASE("MessageCodec encodes a limit order exactly",
          "[protocol][message_codec][encode]")
{
    const ProtocolMessage message = LimitOrderMessage{
        0x0102030405060708ULL,
        Side::Buy,
        0x1112131415161718ULL,
        0x2122232425262728ULL
    };

    const Bytes encoded = MessageCodec::encode(message);

    const Bytes expected{
        std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x1F}, // Total length: 31
        std::byte{0x01},                   // Version 1
        std::byte{0x01},                   // Limit order
        std::byte{0x01}, std::byte{0x02},
        std::byte{0x03}, std::byte{0x04},
        std::byte{0x05}, std::byte{0x06},
        std::byte{0x07}, std::byte{0x08}, // Order ID
        std::byte{0x00},                   // Buy
        std::byte{0x11}, std::byte{0x12},
        std::byte{0x13}, std::byte{0x14},
        std::byte{0x15}, std::byte{0x16},
        std::byte{0x17}, std::byte{0x18}, // Price
        std::byte{0x21}, std::byte{0x22},
        std::byte{0x23}, std::byte{0x24},
        std::byte{0x25}, std::byte{0x26},
        std::byte{0x27}, std::byte{0x28}  // Quantity
    };

    REQUIRE(encoded.size() == LIMIT_ORDER_MESSAGE_LEN);
    CHECK(encoded == expected);
}

TEST_CASE("MessageCodec encodes a sell order with the correct side byte",
          "[protocol][message_codec][encode]")
{
    const ProtocolMessage message =
        LimitOrderMessage{42, Side::Sell, 100'500, 250};

    const Bytes encoded = MessageCodec::encode(message);

    REQUIRE(encoded.size() == LIMIT_ORDER_MESSAGE_LEN);
    CHECK(encoded[14] == std::byte{0x01});
}

TEST_CASE("MessageCodec encodes a cancel order exactly",
          "[protocol][message_codec][encode]")
{
    const ProtocolMessage message =
        CancelOrderMessage{0x0102030405060708ULL};

    const Bytes encoded = MessageCodec::encode(message);

    const Bytes expected{
        std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x0E}, // Total length: 14
        std::byte{0x01},                   // Version 1
        std::byte{0x02},                   // Cancel order
        std::byte{0x01}, std::byte{0x02},
        std::byte{0x03}, std::byte{0x04},
        std::byte{0x05}, std::byte{0x06},
        std::byte{0x07}, std::byte{0x08}  // Order ID
    };

    REQUIRE(encoded.size() == CANCEL_ORDER_MESSAGE_LEN);
    CHECK(encoded == expected);
}

// -----------------------------------------------------------------------------
// Decoding valid messages
// -----------------------------------------------------------------------------

TEST_CASE("MessageCodec decodes a limit order",
          "[protocol][message_codec][decode]")
{
    const ProtocolMessage original = LimitOrderMessage{
        0x0102030405060708ULL,
        Side::Buy,
        0x1112131415161718ULL,
        0x2122232425262728ULL
    };

    const Bytes encoded = MessageCodec::encode(original);
    const DecodeResult result = MessageCodec::decode(encoded);

    REQUIRE(result.error == DecodeError::NO_ERROR);
    REQUIRE(result.message.has_value());
    REQUIRE(std::holds_alternative<LimitOrderMessage>(*result.message));

    const auto& decoded = std::get<LimitOrderMessage>(*result.message);

    CHECK(decoded.id == 0x0102030405060708ULL);
    CHECK(decoded.side == Side::Buy);
    CHECK(decoded.price == 0x1112131415161718ULL);
    CHECK(decoded.quantity == 0x2122232425262728ULL);
}

TEST_CASE("MessageCodec decodes a sell order",
          "[protocol][message_codec][decode]")
{
    const ProtocolMessage original =
        LimitOrderMessage{42, Side::Sell, 100'500, 250};

    const Bytes encoded = MessageCodec::encode(original);
    const DecodeResult result = MessageCodec::decode(encoded);

    REQUIRE(result.error == DecodeError::NO_ERROR);
    REQUIRE(result.message.has_value());
    REQUIRE(std::holds_alternative<LimitOrderMessage>(*result.message));

    const auto& decoded = std::get<LimitOrderMessage>(*result.message);

    CHECK(decoded.id == 42);
    CHECK(decoded.side == Side::Sell);
    CHECK(decoded.price == 100'500);
    CHECK(decoded.quantity == 250);
}

TEST_CASE("MessageCodec decodes a cancel order",
          "[protocol][message_codec][decode]")
{
    const ProtocolMessage original =
        CancelOrderMessage{0xFEDCBA9876543210ULL};

    const Bytes encoded = MessageCodec::encode(original);
    const DecodeResult result = MessageCodec::decode(encoded);

    REQUIRE(result.error == DecodeError::NO_ERROR);
    REQUIRE(result.message.has_value());
    REQUIRE(std::holds_alternative<CancelOrderMessage>(*result.message));

    const auto& decoded = std::get<CancelOrderMessage>(*result.message);

    CHECK(decoded.id == 0xFEDCBA9876543210ULL);
}

// -----------------------------------------------------------------------------
// Malformed frames
// -----------------------------------------------------------------------------

TEST_CASE("MessageCodec rejects a prefix shorter than four bytes",
          "[protocol][message_codec][decode][invalid]")
{
    const Bytes frame(3, std::byte{0});

    const DecodeResult result = MessageCodec::decode(frame);

    CHECK(result.error == DecodeError::INCOMPLETE_FRAME);
    CHECK_FALSE(result.message.has_value());
}

TEST_CASE("MessageCodec rejects invalid declared frame lengths",
          "[protocol][message_codec][decode][invalid]")
{
    for (const std::uint32_t length :
         {0U, 4U, 5U, 6U, 13U, 15U, 30U, 32U, 65U})
    {
        DYNAMIC_SECTION("Declared length: " << length)
        {
            Bytes frame(4, std::byte{0});

            frame[0] = static_cast<std::byte>((length >> 24) & 0xFF);
            frame[1] = static_cast<std::byte>((length >> 16) & 0xFF);
            frame[2] = static_cast<std::byte>((length >> 8) & 0xFF);
            frame[3] = static_cast<std::byte>(length & 0xFF);

            const DecodeResult result = MessageCodec::decode(frame);

            CHECK(result.error == DecodeError::INVALID_FRAME_LENGTH);
            CHECK_FALSE(result.message.has_value());
        }
    }
}

TEST_CASE("MessageCodec rejects an incomplete frame",
          "[protocol][message_codec][decode][invalid]")
{
    SECTION("Limit order")
    {
        Bytes frame = make_frame(LIMIT_ORDER_MESSAGE_LEN);
        frame.resize(10);

        const DecodeResult result = MessageCodec::decode(frame);

        CHECK(result.error == DecodeError::INCOMPLETE_FRAME);
        CHECK_FALSE(result.message.has_value());
    }

    SECTION("Cancel order")
    {
        Bytes frame = make_frame(CANCEL_ORDER_MESSAGE_LEN);
        frame.resize(10);

        const DecodeResult result = MessageCodec::decode(frame);

        CHECK(result.error == DecodeError::INCOMPLETE_FRAME);
        CHECK_FALSE(result.message.has_value());
    }
}

TEST_CASE("MessageCodec rejects trailing bytes",
          "[protocol][message_codec][decode][invalid]")
{
    Bytes frame =
        MessageCodec::encode(ProtocolMessage{CancelOrderMessage{42}});

    frame.push_back(std::byte{0xAA});

    const DecodeResult result = MessageCodec::decode(frame);

    CHECK(result.error == DecodeError::INVALID_FRAME_LENGTH);
    CHECK_FALSE(result.message.has_value());
}

TEST_CASE("MessageCodec rejects an unsupported version",
          "[protocol][message_codec][decode][invalid]")
{
    Bytes frame = make_frame(CANCEL_ORDER_MESSAGE_LEN);
    frame[4] = std::byte{0x02};

    const DecodeResult result = MessageCodec::decode(frame);

    CHECK(result.error == DecodeError::UNSUPPORTED_VERSION);
    CHECK_FALSE(result.message.has_value());
}

TEST_CASE("MessageCodec rejects an unknown message type",
          "[protocol][message_codec][decode][invalid]")
{
    Bytes frame = make_frame(CANCEL_ORDER_MESSAGE_LEN);
    frame[5] = std::byte{0x7F};

    const DecodeResult result = MessageCodec::decode(frame);

    CHECK(result.error == DecodeError::UNKNOWN_MESSAGE_TYPE);
    CHECK_FALSE(result.message.has_value());
}

TEST_CASE("MessageCodec rejects a limit order with cancel frame length",
          "[protocol][message_codec][decode][invalid]")
{
    Bytes frame = make_frame(CANCEL_ORDER_MESSAGE_LEN);
    frame[5] =
        static_cast<std::byte>(MessageType::LIMIT_ORDER_MESSAGE);

    const DecodeResult result = MessageCodec::decode(frame);

    CHECK(result.error == DecodeError::INVALID_PAYLOAD_LENGTH);
    CHECK_FALSE(result.message.has_value());
}

TEST_CASE("MessageCodec rejects a cancel order with limit frame length",
          "[protocol][message_codec][decode][invalid]")
{
    Bytes frame = make_frame(LIMIT_ORDER_MESSAGE_LEN);
    frame[5] =
        static_cast<std::byte>(MessageType::CANCEL_ORDER_MESSAGE);

    const DecodeResult result = MessageCodec::decode(frame);

    CHECK(result.error == DecodeError::INVALID_PAYLOAD_LENGTH);
    CHECK_FALSE(result.message.has_value());
}

TEST_CASE("MessageCodec rejects an invalid side value",
          "[protocol][message_codec][decode][invalid]")
{
    Bytes frame = make_frame(LIMIT_ORDER_MESSAGE_LEN);
    frame[5] =
        static_cast<std::byte>(MessageType::LIMIT_ORDER_MESSAGE);
    frame[14] = std::byte{0x02};

    const DecodeResult result = MessageCodec::decode(frame);

    CHECK(result.error == DecodeError::INVALID_SIDE);
    CHECK_FALSE(result.message.has_value());
}