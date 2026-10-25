#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unistd.h>
#include <atomic>

#include <protocol/protocol_message.hpp>
#include <network/frame_decoder.hpp>

enum class ClientSessionStatus
{
    CLEAN_DISCONNECT,
    PROTOCOL_ERROR,
    TRANSPORT_ERROR,
    SHUTDOWN_REQUESTED,
};

struct ClientSessionResult
{
    ClientSessionStatus status;

    std::optional<FrameError> frame_error;
    std::optional<DecodeError> decode_error;
    std::error_code transport_error;
};

class TcpFdHandler
{
public:
    TcpFdHandler(int fd = -1) : _fd(fd) {}

    TcpFdHandler(const TcpFdHandler &) = delete;
    TcpFdHandler &operator=(const TcpFdHandler &) = delete;

    TcpFdHandler(TcpFdHandler &&other) : _fd(other.release()) {}

    TcpFdHandler &operator=(TcpFdHandler &&other) noexcept
    {
        if (&other != this)
            reset(other.release());

        return *this;
    }

    [[nodiscard]] int release() noexcept
    {
        int fd = _fd;
        _fd = -1;
        return fd;
    }

    [[nodiscard]] int get() const noexcept
    {
        return _fd;
    }

    void reset(int fd = -1) noexcept
    {
        if (_fd != -1)
            close(_fd);

        _fd = fd;
    }

    ~TcpFdHandler()
    {
        reset();
    }

private:
    int _fd{-1};
};

class TcpServer
{
public:
    using MessageHandler = std::function<void(const ProtocolMessage &)>;

    TcpServer(const std::string &address, std::uint16_t port);
    ~TcpServer() = default;

    TcpServer(const TcpServer &) = delete;
    TcpServer &operator=(const TcpServer &) = delete;

    void run(const MessageHandler &handler);

    void stop();

    void start_listening(int backlog = 16);

    [[nodiscard]] TcpFdHandler accept_client();

    [[nodiscard]] std::uint16_t local_port() const noexcept;

private:
    std::uint16_t actual_port{};
    TcpFdHandler _listen_fd;
    TcpFdHandler _shutdown_fd;
    std::atomic<bool> _stop_requested{false};

    ClientSessionResult receive_client(TcpFdHandler &client, const MessageHandler &handler);
};
