#include <catch2/catch_test_macros.hpp>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include <network/tcp_server.hpp>
#include <protocol/message_codec.hpp>
#include <protocol/protocol_message.hpp>

namespace
{
using namespace std::chrono_literals;

class ClientSocket
{
public:
    ClientSocket() : fd_(::socket(AF_INET, SOCK_STREAM, 0))
    {
        if (fd_ == -1)
            throw std::runtime_error("socket() failed");
    }

    explicit ClientSocket(int fd) : fd_(fd) {}

    ClientSocket(const ClientSocket&) = delete;
    ClientSocket& operator=(const ClientSocket&) = delete;

    ClientSocket(ClientSocket&& other) noexcept : fd_(other.fd_)
    {
        other.fd_ = -1;
    }

    ClientSocket& operator=(ClientSocket&& other) noexcept
    {
        if (this == &other)
            return *this;
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
        return *this;
    }

    ~ClientSocket() { close(); }

    int get() const noexcept { return fd_; }

    void close() noexcept
    {
        if (fd_ != -1)
        {
            ::close(fd_);
            fd_ = -1;
        }
    }

private:
    int fd_{-1};
};

class RunningServer
{
public:
    RunningServer() : server_("127.0.0.1", 0) {}

    RunningServer(const RunningServer&) = delete;
    RunningServer& operator=(const RunningServer&) = delete;

    ~RunningServer()
    {
        if (worker_.joinable())
        {
            try
            {
                server_.stop();
            }
            catch (...)
            {
                // Do not throw from a test fixture destructor.
            }
            worker_.join();
        }
    }

    TcpServer& server() noexcept { return server_; }

    void start(TcpServer::MessageHandler handler)
    {
        worker_ = std::thread([this, handler = std::move(handler)]() mutable
        {
            try
            {
                server_.run(handler);
            }
            catch (...)
            {
                server_exception_ = std::current_exception();
            }
        });
    }

    void stop_and_join()
    {
        server_.stop();
        if (worker_.joinable())
            worker_.join();
        rethrow_server_exception();
    }

    void join()
    {
        if (worker_.joinable())
            worker_.join();
        rethrow_server_exception();
    }

private:
    void rethrow_server_exception()
    {
        if (server_exception_)
            std::rethrow_exception(server_exception_);
    }

    TcpServer server_;
    std::thread worker_;
    std::exception_ptr server_exception_;
};

ClientSocket connect_with_retry(std::uint16_t port)
{
    // The socket is already bound in TcpServer's constructor, but run()
    // still has to call listen(). Retry briefly until the server is listening.
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    int last_error = 0;

    while (std::chrono::steady_clock::now() < deadline)
    {
        ClientSocket client;
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        ::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

        if (::connect(client.get(), reinterpret_cast<const sockaddr*>(&address),
                      sizeof(address)) == 0)
            return client;

        last_error = errno;
        std::this_thread::sleep_for(10ms);
    }

    throw std::runtime_error("connect() did not succeed; errno=" +
                             std::to_string(last_error));
}

void send_all(int fd, std::span<const std::byte> bytes)
{
    std::size_t sent = 0;
    while (sent < bytes.size())
    {
        const auto n = ::send(fd, bytes.data() + sent, bytes.size() - sent,
#ifdef MSG_NOSIGNAL
                              MSG_NOSIGNAL
#else
                              0
#endif
        );

        if (n > 0)
        {
            sent += static_cast<std::size_t>(n);
            continue;
        }

        if (n == -1 && errno == EINTR)
            continue;

        throw std::runtime_error("send() failed");
    }
}

std::vector<std::byte> make_invalid_length_frame()
{
    // FrameDecoder's minimum valid frame length is 6. Length 5 is invalid.
    return {
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x05}
    };
}

bool wait_for_count(std::condition_variable& cv, std::unique_lock<std::mutex>& lock,
                    const std::vector<ProtocolMessage>& messages,
                    std::size_t count)
{
    return cv.wait_for(lock, 2s, [&messages, count]
    {
        return messages.size() >= count;
    });
}
} // namespace

TEST_CASE("TCP server can stop while waiting for a connection", "[network][tcp]")
{
    RunningServer server;
    server.start([](const ProtocolMessage&) {});

    // Give run() a chance to enter its listening poll. stop() must wake it.
    std::this_thread::sleep_for(20ms);
    REQUIRE_NOTHROW(server.stop_and_join());
}

TEST_CASE("TCP server delivers a limit-order message", "[network][tcp]")
{
    RunningServer server;
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<ProtocolMessage> received;

    server.start([&](const ProtocolMessage& message)
    {
        {
            std::lock_guard lock(mutex);
            received.push_back(message);
        }
        cv.notify_one();
    });

    auto client = connect_with_retry(server.server().local_port());
    const ProtocolMessage expected = LimitOrderMessage{101, Side::Buy, 12345, 7};
    const auto bytes = MessageCodec::encode(expected);
    send_all(client.get(), bytes);

    {
        std::unique_lock lock(mutex);
        REQUIRE(wait_for_count(cv, lock, received, 1));
        REQUIRE(received.size() == 1);
        REQUIRE(std::holds_alternative<LimitOrderMessage>(received[0]));
        const auto& actual = std::get<LimitOrderMessage>(received[0]);
        REQUIRE(actual.id == 101);
        REQUIRE(actual.side == Side::Buy);
        REQUIRE(actual.price == 12345);
        REQUIRE(actual.quantity == 7);
    }

    client.close();
    REQUIRE_NOTHROW(server.stop_and_join());
}

TEST_CASE("TCP server handles fragmented and consecutive frames", "[network][tcp]")
{
    RunningServer server;
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<ProtocolMessage> received;

    server.start([&](const ProtocolMessage& message)
    {
        {
            std::lock_guard lock(mutex);
            received.push_back(message);
        }
        cv.notify_one();
    });

    auto client = connect_with_retry(server.server().local_port());
    const auto first = MessageCodec::encode(
        ProtocolMessage{LimitOrderMessage{201, Side::Sell, 20000, 3}});
    const auto second = MessageCodec::encode(
        ProtocolMessage{CancelOrderMessage{201}});

    // Split the first frame across writes, then append a second complete frame.
    REQUIRE(first.size() > 4);
    send_all(client.get(), std::span<const std::byte>(first.data(), 3));
    std::this_thread::sleep_for(10ms);

    std::vector<std::byte> rest;
    rest.insert(rest.end(), first.begin() + 3, first.end());
    rest.insert(rest.end(), second.begin(), second.end());
    send_all(client.get(), rest);

    {
        std::unique_lock lock(mutex);
        REQUIRE(wait_for_count(cv, lock, received, 2));
        REQUIRE(received.size() == 2);
        REQUIRE(std::holds_alternative<LimitOrderMessage>(received[0]));
        REQUIRE(std::holds_alternative<CancelOrderMessage>(received[1]));
        REQUIRE(std::get<LimitOrderMessage>(received[0]).id == 201);
        REQUIRE(std::get<CancelOrderMessage>(received[1]).id == 201);
    }

    client.close();
    REQUIRE_NOTHROW(server.stop_and_join());
}

TEST_CASE("TCP server continues accepting clients after a malformed frame", "[network][tcp]")
{
    RunningServer server;
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<ProtocolMessage> received;

    server.start([&](const ProtocolMessage& message)
    {
        {
            std::lock_guard lock(mutex);
            received.push_back(message);
        }
        cv.notify_one();
    });

    {
        auto bad_client = connect_with_retry(server.server().local_port());
        const auto bad_frame = make_invalid_length_frame();
        send_all(bad_client.get(), bad_frame);
        bad_client.close();
    }

    // The server should close the malformed session and accept this client.
    auto good_client = connect_with_retry(server.server().local_port());
    const ProtocolMessage expected = CancelOrderMessage{303};
    const auto bytes = MessageCodec::encode(expected);
    send_all(good_client.get(), bytes);

    {
        std::unique_lock lock(mutex);
        REQUIRE(wait_for_count(cv, lock, received, 1));
        REQUIRE(received.size() == 1);
        REQUIRE(std::holds_alternative<CancelOrderMessage>(received[0]));
        REQUIRE(std::get<CancelOrderMessage>(received[0]).id == 303);
    }

    good_client.close();
    REQUIRE_NOTHROW(server.stop_and_join());
}

TEST_CASE("TCP server shutdown interrupts an idle client session", "[network][tcp]")
{
    RunningServer server;
    server.start([](const ProtocolMessage&) {});

    auto client = connect_with_retry(server.server().local_port());

    // No data is sent. stop() must wake the server even though a client is active.
    REQUIRE_NOTHROW(server.stop_and_join());
    client.close();
}

TEST_CASE("TCP server can stop when shutdown is requested before run starts", "[network][tcp]")
{
    TcpServer server("127.0.0.1", 0);
    server.stop();

    std::exception_ptr server_exception;
    std::thread worker([&]
    {
        try
        {
            server.run([](const ProtocolMessage&) {});
        }
        catch (...)
        {
            server_exception = std::current_exception();
        }
    });
    worker.join();

    if (server_exception)
        std::rethrow_exception(server_exception);
}
