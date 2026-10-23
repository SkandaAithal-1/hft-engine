
#include <network/frame_decoder.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{

    // Construct a valid cancel-order frame (14 bytes total).
    // The decoder only checks framing, not message semantics.
    std::array<std::byte, 4> make_length_prefix(std::uint32_t length)
    {
        return {
            static_cast<std::byte>((length >> 24) & 0xFF),
            static_cast<std::byte>((length >> 16) & 0xFF),
            static_cast<std::byte>((length >> 8) & 0xFF),
            static_cast<std::byte>(length & 0xFF)};
    }

    std::vector<std::byte> make_frame(std::uint32_t length = 14)
    {
        std::vector<std::byte> frame(length);

        const auto prefix = make_length_prefix(length);
        std::copy(prefix.begin(), prefix.end(), frame.begin());

        if (length >= 6)
        {
            frame[4] = std::byte{0x01};
            frame[5] = std::byte{0x02};
        }

        return frame;
    }

} // namespace

TEST_CASE("FrameDecoder extracts a complete frame")
{
    FrameDecoder decoder;
    const auto frame = make_frame();

    const auto result = decoder.feed(frame);

    REQUIRE(result.ok());
    REQUIRE(result.frames.size() == 1);
    CHECK(result.frames[0] == frame);
}

TEST_CASE("FrameDecoder buffers a fragmented length prefix")
{
    FrameDecoder decoder;
    const auto frame = make_frame();

    const auto first = decoder.feed(
        std::span<const std::byte>{frame.data(), 2});

    REQUIRE(first.ok());
    CHECK(first.frames.empty());

    const auto second = decoder.feed(
        std::span<const std::byte>{frame.data() + 2, 2});

    REQUIRE(second.ok());
    CHECK(second.frames.empty());

    const auto third = decoder.feed(
        std::span<const std::byte>{frame.data() + 4, frame.size() - 4});

    REQUIRE(third.ok());
    REQUIRE(third.frames.size() == 1);
    CHECK(third.frames[0] == frame);
}

TEST_CASE("FrameDecoder buffers a fragmented payload")
{
    FrameDecoder decoder;
    const auto frame = make_frame();

    const auto first = decoder.feed(
        std::span<const std::byte>{frame.data(), 8});

    REQUIRE(first.ok());
    CHECK(first.frames.empty());

    const auto second = decoder.feed(
        std::span<const std::byte>{frame.data() + 8, 3});

    REQUIRE(second.ok());
    CHECK(second.frames.empty());

    const auto third = decoder.feed(
        std::span<const std::byte>{frame.data() + 11, frame.size() - 11});

    REQUIRE(third.ok());
    REQUIRE(third.frames.size() == 1);
    CHECK(third.frames[0] == frame);
}

TEST_CASE("FrameDecoder extracts multiple frames from one input")
{
    FrameDecoder decoder;
    const auto first_frame = make_frame(14);
    const auto second_frame = make_frame(31);

    std::vector<std::byte> input;
    input.insert(input.end(), first_frame.begin(), first_frame.end());
    input.insert(input.end(), second_frame.begin(), second_frame.end());

    const auto result = decoder.feed(input);

    REQUIRE(result.ok());
    REQUIRE(result.frames.size() == 2);
    CHECK(result.frames[0] == first_frame);
    CHECK(result.frames[1] == second_frame);
}

TEST_CASE("FrameDecoder emits a complete frame and retains a partial next frame")
{
    FrameDecoder decoder;
    const auto first_frame = make_frame(14);
    const auto second_frame = make_frame(31);

    std::vector<std::byte> input = first_frame;
    input.insert(input.end(), second_frame.begin(), second_frame.begin() + 9);

    const auto first = decoder.feed(input);

    REQUIRE(first.ok());
    REQUIRE(first.frames.size() == 1);
    CHECK(first.frames[0] == first_frame);

    const auto second = decoder.feed(
        std::span<const std::byte>{
            second_frame.data() + 9, second_frame.size() - 9});

    REQUIRE(second.ok());
    REQUIRE(second.frames.size() == 1);
    CHECK(second.frames[0] == second_frame);
}

TEST_CASE("FrameDecoder rejects invalid frame lengths")
{
    SECTION("Lengths below the minimum")
    {
        for (const std::uint32_t length : {0, 1, 4, 5})
        {
            FrameDecoder decoder;
            const auto prefix = make_length_prefix(length);
            const auto result = decoder.feed(prefix);

            CHECK_FALSE(result.ok());
            CHECK(result.error == FrameError::INVALID_FRAME_LENGTH);

            const auto subsequent = decoder.feed(make_frame());
            CHECK(subsequent.error == FrameError::DECODER_FAILED);
        }
    }

    SECTION("Lengths above the maximum")
    {
        for (const std::uint32_t length : {65, 100, 1024})
        {
            FrameDecoder decoder;
            const auto prefix = make_length_prefix(length);
            const auto result = decoder.feed(prefix);

            CHECK_FALSE(result.ok());
            CHECK(result.error == FrameError::INVALID_FRAME_LENGTH);
        }
    }
}

TEST_CASE("FrameDecoder accepts the minimum and maximum frame lengths")
{
    SECTION("Minimum length")
    {
        FrameDecoder decoder;
        const auto frame = make_frame(MIN_FRAME_LEN);

        const auto result = decoder.feed(frame);

        REQUIRE(result.ok());
        REQUIRE(result.frames.size() == 1);
        CHECK(result.frames[0] == frame);
    }

    SECTION("Maximum length")
    {
        FrameDecoder decoder;
        const auto frame = make_frame(MAX_FRAME_LEN);

        const auto result = decoder.feed(frame);

        REQUIRE(result.ok());
        REQUIRE(result.frames.size() == 1);
        CHECK(result.frames[0] == frame);
    }
}

TEST_CASE("FrameDecoder finish handles EOF correctly")
{
    SECTION("Empty buffer")
    {
        FrameDecoder decoder;

        const auto result = decoder.finish();

        CHECK(result.error == FrameError::NONE);
        CHECK(decoder.feed(make_frame()).error ==
              FrameError::DECODER_FINISHED);
    }

    SECTION("Partial length prefix")
    {
        FrameDecoder decoder;
        const auto frame = make_frame();

        const auto feed_result = decoder.feed(
            std::span<const std::byte>{frame.data(), 2});

        REQUIRE(feed_result.ok());

        const auto finish_result = decoder.finish();

        CHECK(finish_result.error == FrameError::INCOMPLETE_FRAME);
    }

    SECTION("Partial payload")
    {
        FrameDecoder decoder;
        const auto frame = make_frame();

        const auto feed_result = decoder.feed(
            std::span<const std::byte>{frame.data(), 8});

        REQUIRE(feed_result.ok());

        const auto finish_result = decoder.finish();

        CHECK(finish_result.error == FrameError::INCOMPLETE_FRAME);
    }
}

TEST_CASE("FrameDecoder preserves terminal states")
{
    SECTION("Repeated finish")
    {
        FrameDecoder decoder;

        CHECK(decoder.finish().error == FrameError::NONE);
        CHECK(decoder.finish().error == FrameError::DECODER_FINISHED);
    }

    SECTION("Finish after an incomplete frame")
    {
        FrameDecoder decoder;
        const auto frame = make_frame();

        REQUIRE(decoder.feed(
                           std::span<const std::byte>{frame.data(), 2})
                    .ok());

        CHECK(decoder.finish().error == FrameError::INCOMPLETE_FRAME);
        CHECK(decoder.finish().error == FrameError::DECODER_FINISHED);
    }

    SECTION("Finish after a framing error")
    {
        FrameDecoder decoder;
        const auto invalid_prefix = make_frame(5);

        CHECK(decoder.feed(
                         std::span<const std::byte>{invalid_prefix.data(), 4})
                  .error ==
              FrameError::INVALID_FRAME_LENGTH);

        CHECK(decoder.finish().error == FrameError::DECODER_FAILED);
        CHECK(decoder.feed(make_frame()).error == FrameError::DECODER_FAILED);
    }

    SECTION("Feed after EOF")
    {
        FrameDecoder decoder;

        REQUIRE(decoder.finish().error == FrameError::NONE);

        const auto result = decoder.feed(make_frame());

        CHECK(result.error == FrameError::DECODER_FINISHED);
        CHECK(result.frames.empty());
    }
}

TEST_CASE("FrameDecoder discards frames from a batch containing invalid framing")
{
    FrameDecoder decoder;

    const auto valid_frame = make_frame(14);
    const auto invalid_prefix = make_frame(5);

    std::vector<std::byte> input = valid_frame;
    input.insert(input.end(), invalid_prefix.begin(), invalid_prefix.begin() + 4);

    const auto result = decoder.feed(input);

    CHECK(result.error == FrameError::INVALID_FRAME_LENGTH);
    CHECK(result.frames.empty());
    CHECK(decoder.feed(make_frame()).error == FrameError::DECODER_FAILED);
}

TEST_CASE("FrameDecoder reset restores the active state")
{
    FrameDecoder decoder;
    const auto invalid_prefix = make_frame(5);

    REQUIRE(decoder.feed(
                       std::span<const std::byte>{invalid_prefix.data(), 4})
                .error ==
            FrameError::INVALID_FRAME_LENGTH);

    decoder.reset();

    const auto frame = make_frame();
    const auto result = decoder.feed(frame);

    REQUIRE(result.ok());
    REQUIRE(result.frames.size() == 1);
    CHECK(result.frames[0] == frame);
}
