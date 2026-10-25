#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <system_error>
#include <stdexcept>
#include <iostream>
#include <span>
#include <array>
#include <optional>
#include <cstddef>
#include <poll.h>
#include <sys/eventfd.h>
#include <cstdint>

#include <network/tcp_server.hpp>
#include <network/frame_decoder.hpp>
#include <protocol/message_codec.hpp>

std::error_code get_socket_error(int fd)
{
    int error = 0;
    socklen_t len = sizeof(error);

    if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1)
        return {errno, std::generic_category()};

    if (error == 0)
        return {}; // No pending socket error.

    return {error, std::generic_category()};
}

TcpServer::TcpServer(const std::string &address, std::uint16_t port) : _listen_fd(socket(AF_INET, SOCK_STREAM, 0)), _shutdown_fd(eventfd(0, EFD_CLOEXEC))
{
    if (_listen_fd.get() == -1)
        throw std::system_error(errno, std::generic_category(), "socket");

    if (_shutdown_fd.get() == -1)
        throw std::system_error(errno, std::generic_category(), "eventfd");

    int enabled = 1;
    if (setsockopt(_listen_fd.get(), SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "setsockopt");
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    int result = inet_pton(AF_INET, address.c_str(), &addr.sin_addr);
    if (result == 0)
        throw std::invalid_argument("Invalid IPv4 address: " + address);

    if (result == -1)
        throw std::system_error(errno, std::generic_category(), "inet_pton");

    if (bind(_listen_fd.get(),
             reinterpret_cast<const sockaddr *>(&addr),
             sizeof(addr)) == -1)
    {
        // Handle error
        throw std::system_error(errno, std::generic_category(), "bind");
    }

    sockaddr_in actual_address{};
    socklen_t length = sizeof(actual_address);

    if (getsockname(_listen_fd.get(), reinterpret_cast<sockaddr *>(&actual_address), &length) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "getsockname");
    }

    actual_port = ntohs(actual_address.sin_port);
}

std::uint16_t TcpServer::local_port() const noexcept
{
    return actual_port;
}

void TcpServer::start_listening(int backlog)
{
    if (listen(_listen_fd.get(), backlog) == -1)
        throw std::system_error(errno, std::generic_category(), "listen");
}

TcpFdHandler TcpServer::accept_client()
{
    int client_fd;

    do
    {
        client_fd = accept(_listen_fd.get(), nullptr, nullptr);
    } while (client_fd == -1 && errno == EINTR);

    if (client_fd == -1)
        throw std::system_error(errno, std::generic_category(), "accept");

    return TcpFdHandler(client_fd);
}

ClientSessionResult TcpServer::receive_client(TcpFdHandler &client, const MessageHandler &handler)
{
    std::array<std::byte, 4096> buffer{};
    FrameDecoder decoder;
    MessageCodec codec;

    std::array<pollfd, 2> fds{{{client.get(), POLLIN, 0},
                               {_shutdown_fd.get(), POLLIN, 0}}};

    while (!_stop_requested.load())
    {
        int poll_result = poll(fds.data(), fds.size(), -1);

        if (poll_result < 0)
        {
            if (errno == EINTR)
                continue;
            else
                throw std::system_error(errno, std::generic_category(), "poll");
        }

        if (_stop_requested.load() || fds[1].revents & POLLIN)
            return {
                ClientSessionStatus::SHUTDOWN_REQUESTED,
                std::nullopt,
                std::nullopt,
                {}};

        if (fds[0].revents & POLLNVAL)
            return {
                ClientSessionStatus::TRANSPORT_ERROR,
                std::nullopt,
                std::nullopt,
                std::make_error_code(std::errc::bad_file_descriptor)};

        if (fds[0].revents & (POLLIN | POLLHUP))
        {
            const ssize_t n = recv(client.get(), buffer.data(), buffer.size(), 0);

            if (n > 0)
            {
                // process them
                auto result = decoder.feed(std::span<std::byte>(buffer.data(), static_cast<std::size_t>(n)));

                if (!result.ok())
                    return {
                        ClientSessionStatus::PROTOCOL_ERROR,
                        result.error,
                        std::nullopt,
                        {}};

                for (auto &frame : result.frames)
                {
                    auto codec_result = codec.decode(frame);

                    if (codec_result.ok())
                        handler(codec_result.message.value());
                    else
                        return {
                            ClientSessionStatus::PROTOCOL_ERROR,
                            std::nullopt,
                            codec_result.error,
                            {}};
                }

                if (fds[0].revents & POLLERR)
                {
                    auto error = get_socket_error(client.get());

                    if (!error)
                        error = std::make_error_code(std::errc::io_error);

                    return {
                        ClientSessionStatus::TRANSPORT_ERROR,
                        std::nullopt,
                        std::nullopt,
                        error};
                }

                continue;
            }

            if (n == 0)
            {
                // Proper closure
                if (fds[0].revents & POLLERR)
                {
                    auto error = get_socket_error(client.get());

                    if (error)
                        return {
                            ClientSessionStatus::TRANSPORT_ERROR,
                            std::nullopt,
                            std::nullopt,
                            error};
                }
                auto result = decoder.finish();

                if (!result.ok())
                    return {
                        ClientSessionStatus::PROTOCOL_ERROR,
                        result.error,
                        std::nullopt,
                        {}};

                return {
                    ClientSessionStatus::CLEAN_DISCONNECT,
                    std::nullopt,
                    std::nullopt,
                    {}};
            }

            const int error = errno;

            if (error == EINTR || error == EAGAIN || error == EWOULDBLOCK)
                continue;

            return {
                ClientSessionStatus::TRANSPORT_ERROR,
                std::nullopt,
                std::nullopt,
                std::error_code(error, std::generic_category())};
        }

        if (fds[0].revents & POLLERR)
        {
            auto error = get_socket_error(client.get());

            if (!error)
                error = std::make_error_code(std::errc::io_error);

            return {
                ClientSessionStatus::TRANSPORT_ERROR,
                std::nullopt,
                std::nullopt,
                error};
        }
    }
    return {
        ClientSessionStatus::SHUTDOWN_REQUESTED,
        std::nullopt,
        std::nullopt,
        {}};
}

void TcpServer::run(const MessageHandler &handler)
{
    start_listening();

    std::array<pollfd, 2> fds{{{_listen_fd.get(), POLLIN, 0},
                               {_shutdown_fd.get(), POLLIN, 0}}};

    while (!_stop_requested.load())
    {
        int poll_result = poll(fds.data(), fds.size(), -1);

        if (poll_result < 0)
        {
            if (errno == EINTR)
                continue;
            else
                throw std::system_error(errno, std::generic_category(), "poll");
        }

        if (_stop_requested.load() || fds[1].revents & POLLIN)
            break;

        if (fds[0].revents & POLLNVAL)
            throw std::system_error(std::make_error_code(std::errc::bad_file_descriptor), "poll listening socket");

        if (fds[0].revents & (POLLHUP | POLLERR))
            throw std::system_error(std::make_error_code(std::errc::io_error), "poll listening socket");

        if (fds[0].revents & POLLIN)
        {
            auto client = accept_client();
            auto result = receive_client(client, handler);

            switch (result.status)
            {
            case ClientSessionStatus::CLEAN_DISCONNECT:
                break;

            case ClientSessionStatus::PROTOCOL_ERROR:
                if (result.decode_error != std::nullopt)
                    std::cerr << "Protocol error : " << to_string(result.decode_error.value()) << "\n";
                else if (result.frame_error != std::nullopt)
                    std::cerr << "Protocol error : " << to_string(result.frame_error.value()) << "\n";
                break;

            case ClientSessionStatus::TRANSPORT_ERROR:
                std::cerr << "Transport error : " << result.transport_error.message() << "\n";
                break;

            case ClientSessionStatus::SHUTDOWN_REQUESTED:
                break;
            }
        }
    }
}

void TcpServer::stop()
{
    _stop_requested.store(true);

    const std::uint64_t signal = 1;
    while (true)
    {
        const ssize_t n = write(_shutdown_fd.get(), &signal, sizeof(signal));

        if (n == static_cast<ssize_t>(sizeof(signal)))
            return;

        if (n == -1)
        {
            const int error = errno;

            if (error == EINTR)
                continue;

            if (error == EAGAIN || error == EWOULDBLOCK)
                return;

            throw std::system_error(error, std::generic_category(), "eventfd write");
        }

        throw std::system_error(std::make_error_code(std::errc::io_error), "short eventfd write");
    }
}