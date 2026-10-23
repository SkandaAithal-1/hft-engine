#include <network/frame_decoder.hpp>

FrameDecoderResult FrameDecoder::feed(std::span<const std::byte> bytes)
{
    FrameDecoderResult result{};

    switch (state)
    {
    case DecoderState::FAILED:
        result.error = FrameError::DECODER_FAILED;
        return result;
    case DecoderState::FINISHED:
        result.error = FrameError::DECODER_FINISHED;
        return result;
    }

    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        if (_buffer.size() < 4)
        {
            _buffer.push_back(bytes[i]);
            frame_len <<= 8;
            frame_len |= std::to_integer<std::uint32_t>(bytes[i]);

            if (_buffer.size() == 4)
            {
                if (frame_len > MAX_FRAME_LEN || frame_len < MIN_FRAME_LEN)
                {
                    result.error = FrameError::INVALID_FRAME_LENGTH;
                    result.frames.clear();
                    state = DecoderState::FAILED;
                    return result;
                }
            }
        }
        else
        {
            _buffer.push_back(bytes[i]);

            if (_buffer.size() == frame_len)
            {
                result.frames.push_back(_buffer);
                _buffer.clear();
                frame_len = 0;
            }
        }
    }
    return result;
}

FrameDecoderResult FrameDecoder::finish()
{
    FrameDecoderResult result;
    result.error = (state == DecoderState::FAILED)     ? FrameError::DECODER_FAILED
                   : (state == DecoderState::FINISHED) ? FrameError::DECODER_FINISHED
                   : (!_buffer.empty())                ? FrameError::INCOMPLETE_FRAME
                                                       : FrameError::NONE;
    if (state != DecoderState::FAILED)
        state = DecoderState::FINISHED;

    return result;
}

void FrameDecoder::reset()
{
    _buffer.clear();
    state = DecoderState::ACTIVE;
    frame_len = 0;
}