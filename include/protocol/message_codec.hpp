#pragma once

#include <cstdint>
#include <vector>
#include <span>

#include <protocol/protocol_message.hpp>

inline constexpr int LIMIT_ORDER_MESSAGE_LEN = 31;
inline constexpr int CANCEL_ORDER_MESSAGE_LEN = 14;

class MessageCodec
{
public:
    static std::vector<std::byte> encode(const ProtocolMessage &message);
    static DecodeResult decode(std::span<const std::byte> frame);

private:
    static std::uint32_t to_uint32(std::span<const std::byte> frame, std::size_t start);
    static std::uint64_t to_uint64(std::span<const std::byte> frame, std::size_t start);
    static void write_uint64_be(std::span<std::byte> frame, std::size_t start, std::uint64_t value);
    static void write_uint32_be(std::span<std::byte> frame, std::size_t start, std::uint32_t value);
};