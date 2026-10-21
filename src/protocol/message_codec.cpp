#include <type_traits>

#include <protocol/message_codec.hpp>

std::vector<std::byte> MessageCodec::encode(const ProtocolMessage &message)
{
    std::vector<std::byte> encoded_message;

    std::visit([&encoded_message](const auto &msg)
               {
        using T = std::decay_t<decltype(msg)>;

        if constexpr (std::is_same_v<T, LimitOrderMessage>) {
            // Reserve the frame length 
            encoded_message.resize(LIMIT_ORDER_MESSAGE_LEN);
            // Push the length first (explicitly big endian)
            MessageCodec::write_uint32_be(encoded_message, 0, LIMIT_ORDER_MESSAGE_LEN);
            // Push the version
            encoded_message[4] = static_cast<std::byte>(Version::VERSION_1);
            // Push the message type 
            encoded_message[5] = static_cast<std::byte>(MessageType::LIMIT_ORDER_MESSAGE);
            // Now the parameters in order (OrderID, Side, Price, Quantity)
            std::size_t idx = 6;
            MessageCodec::write_uint64_be(encoded_message, idx, msg.id);
            idx += 8;

            encoded_message[idx] = static_cast<std::byte>((msg.side == Side::Buy)?0x00:0x01); 
            ++idx;

            MessageCodec::write_uint64_be(encoded_message, idx, msg.price);
            idx += 8;

            MessageCodec::write_uint64_be(encoded_message, idx, msg.quantity);

        } else if constexpr (std::is_same_v<T, CancelOrderMessage>) {
            // Reserve the length
            encoded_message.resize(CANCEL_ORDER_MESSAGE_LEN);
            // Push the length
            MessageCodec::write_uint32_be(encoded_message, 0, CANCEL_ORDER_MESSAGE_LEN);
            // Push the version
            encoded_message[4] = static_cast<std::byte>(Version::VERSION_1);
            // Push the message type
            encoded_message[5] = static_cast<std::byte>(MessageType::CANCEL_ORDER_MESSAGE);
            // Now the only parameter (OrderID)
            std::size_t idx = 6;
            MessageCodec::write_uint64_be(encoded_message, idx, msg.id);

        } }, message);

    return encoded_message;
}

DecodeResult MessageCodec::decode(std::span<const std::byte> frame)
{
    if (frame.size() < 4)
        return {std::nullopt, DecodeError::INCOMPLETE_FRAME};

    std::uint32_t len = MessageCodec::to_uint32(frame, 0);

    if (len != LIMIT_ORDER_MESSAGE_LEN && len != CANCEL_ORDER_MESSAGE_LEN)
        return {std::nullopt, DecodeError::INVALID_FRAME_LENGTH};

    if (frame.size() < len)
        return {std::nullopt, DecodeError::INCOMPLETE_FRAME};

    if (frame.size() > len)
        return {std::nullopt, DecodeError::INVALID_FRAME_LENGTH};

    std::size_t idx = 4;
    const auto version = std::to_integer<std::uint8_t>(frame[idx]);
    ++idx;
    if (version != static_cast<std::uint8_t>(Version::VERSION_1))
        return {std::nullopt, DecodeError::UNSUPPORTED_VERSION};

    const auto message_type = std::to_integer<std::uint8_t>(frame[idx]);
    std::uint8_t side_int = 0;
    ++idx;
    LimitOrderMessage limit_order_message;
    CancelOrderMessage cancel_order_message;
    switch (message_type)
    {
    case static_cast<uint8_t>(MessageType::LIMIT_ORDER_MESSAGE):
        if (len != LIMIT_ORDER_MESSAGE_LEN)
            return {std::nullopt, DecodeError::INVALID_PAYLOAD_LENGTH};

        limit_order_message.id = MessageCodec::to_uint64(frame, idx);
        idx += 8;
        side_int = std::to_integer<unsigned char>(frame[idx]);
        if (side_int == 0)
            limit_order_message.side = Side::Buy;
        else if (side_int == 1)
            limit_order_message.side = Side::Sell;
        else
            return {std::nullopt, DecodeError::INVALID_SIDE};
        ++idx;
        limit_order_message.price = MessageCodec::to_uint64(frame, idx);
        idx += 8;
        limit_order_message.quantity = MessageCodec::to_uint64(frame, idx);
        return {limit_order_message, DecodeError::NO_ERROR};

    case static_cast<std::uint8_t>(MessageType::CANCEL_ORDER_MESSAGE):
        if (len != CANCEL_ORDER_MESSAGE_LEN)
            return {std::nullopt, DecodeError::INVALID_PAYLOAD_LENGTH};
        cancel_order_message.id = MessageCodec::to_uint64(frame, idx);
        return {cancel_order_message, DecodeError::NO_ERROR};

    default:
        return {std::nullopt, DecodeError::UNKNOWN_MESSAGE_TYPE};
    }
}

std::uint32_t MessageCodec::to_uint32(std::span<const std::byte> frame, std::size_t start)
{
    std::uint32_t x = 0;
    for (std::size_t i = 0; i < 4; ++i)
        x |= (std::uint32_t(std::to_integer<unsigned char>(frame[start + i])) << (24 - 8 * i));
    return x;
}

std::uint64_t MessageCodec::to_uint64(std::span<const std::byte> frame, std::size_t start)
{
    std::uint64_t x = 0;
    for (std::size_t i = 0; i < 8; ++i)
        x |= (std::uint64_t(std::to_integer<unsigned char>(frame[start + i])) << (56 - 8 * i));
    return x;
}

void MessageCodec::write_uint64_be(std::span<std::byte> frame, std::size_t start, std::uint64_t value)
{
    for (std::size_t i = 0; i < 8; ++i)
        frame[start + i] = static_cast<std::byte>((value >> (56 - 8 * i)) & 0xFF);
}

void MessageCodec::write_uint32_be(std::span<std::byte> frame, std::size_t start, std::uint32_t value)
{
    for (std::size_t i = 0; i < 4; ++i)
        frame[start + i] = static_cast<std::byte>((value >> (24 - 8 * i)) & 0xFF);
}
