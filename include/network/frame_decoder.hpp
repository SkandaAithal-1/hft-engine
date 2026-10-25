#pragma once

#include <cstdint>
#include <vector>
#include <span>
#include <string>

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

inline std::string to_string(FrameError error)
{
    switch (error)
    {
    case FrameError::DECODER_FAILED:
        return std::string("DECODER FAILED");
    case FrameError::DECODER_FINISHED:
        return std::string("DECODER FINISHED");
    case FrameError::INCOMPLETE_FRAME:
        return std::string("INCOMPLETE FRAME");
    case FrameError::INVALID_FRAME_LENGTH:
        return std::string("INVALID FRAME LENGTH");
    case FrameError::NONE:
        return std::string("NONE");
    }
}

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