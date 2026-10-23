#pragma once

#include <cstdint>
#include <vector>
#include <span>

inline constexpr std::size_t MIN_FRAME_LEN = 6;
inline constexpr std::size_t MAX_FRAME_LEN = 64;

enum class FrameError
{
    NONE,
    INVALID_FRAME_LENGTH,
    INCOMPLETE_FRAME,
    DECODER_FAILED,
    DECODER_FINISHED,
};

enum class DecoderState
{
    ACTIVE,
    FAILED,
    FINISHED,
};

struct FrameDecoderResult
{
    std::vector<std::vector<std::byte>> frames;
    FrameError error = FrameError::NONE;

    bool ok() const noexcept
    {
        return error == FrameError::NONE;
    }
};

class FrameDecoder
{
public:
    [[nodiscard]] FrameDecoderResult feed(std::span<const std::byte> bytes);
    [[nodiscard]] FrameDecoderResult finish();
    void reset();

private:
    std::vector<std::byte> _buffer;
    std::uint32_t frame_len{0};
    DecoderState state{DecoderState::ACTIVE};
};