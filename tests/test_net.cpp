/** The network layer: protocol structures and EasySocket over loopback.

Streaming over the network is the second way to get data out, alongside dumping
to a file. The protocol structures are packed to 1 byte and read by casting a
pointer onto the received buffer, which makes their layout as critical as the
file format's: a mismatch between the GUI's and the application's versions leads
to silently parsing garbage.
*/

#include <catch2/catch_test_macros.hpp>

#include <easy/easy_net.h>
#include <easy/easy_socket.h>
#include <easy/profiler.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

//////////////////////////////////////////////////////////////////////////
// Protocol structures

TEST_CASE("The network message signature is pinned", "[net][protocol]")
{
    REQUIRE(profiler::net::EASY_MESSAGE_SIGN == 20160909);
}

TEST_CASE("Message sizes match 1-byte packing", "[net][protocol]")
{
    // magic(4) + type(1)
    REQUIRE(sizeof(profiler::net::Message) == 5);

    // Message(5) + size(4)
    REQUIRE(sizeof(profiler::net::DataMessage) == 9);

    // Message(5) + id(4) + status(1)
    REQUIRE(sizeof(profiler::net::BlockStatusMessage) == 10);

    // Message(5) + three bools
    REQUIRE(sizeof(profiler::net::EasyProfilerStatus) == 8);

    // Message(5) + bool
    REQUIRE(sizeof(profiler::net::BoolMessage) == 6);

    // Message(5) + maxValue(4) + avgValue(4)
    REQUIRE(sizeof(profiler::net::TimestampMessage) == 13);
}

TEST_CASE("A default message carries the right signature", "[net][protocol]")
{
    profiler::net::Message message;
    REQUIRE(message.isEasyNetMessage());
    REQUIRE(message.magic_number == profiler::net::EASY_MESSAGE_SIGN);
    REQUIRE(message.type == profiler::net::MessageType::Undefined);
}

TEST_CASE("The message type survives construction", "[net][protocol]")
{
    profiler::net::Message ping(profiler::net::MessageType::Ping);
    REQUIRE(ping.type == profiler::net::MessageType::Ping);
    REQUIRE(ping.isEasyNetMessage());

    profiler::net::Message stop(profiler::net::MessageType::Request_Stop_Capture);
    REQUIRE(stop.type == profiler::net::MessageType::Request_Stop_Capture);
}

TEST_CASE("A garbage buffer is not recognized as a message", "[net][protocol]")
{
    // This check is what keeps the listen loop from parsing random bytes
    // (profile_manager.cpp: if (!message->isEasyNetMessage()) continue;).
    std::vector<char> garbage(sizeof(profiler::net::Message), static_cast<char>(0xAB));

    const profiler::net::Message* asMessage =
        reinterpret_cast<const profiler::net::Message*>(garbage.data());

    REQUIRE_FALSE(asMessage->isEasyNetMessage());
}

TEST_CASE("A zeroed buffer is not recognized as a message", "[net][protocol]")
{
    std::vector<char> zeros(sizeof(profiler::net::Message), 0);
    const profiler::net::Message* asMessage =
        reinterpret_cast<const profiler::net::Message*>(zeros.data());

    REQUIRE_FALSE(asMessage->isEasyNetMessage());
}

TEST_CASE("DataMessage points at the data right behind it", "[net][protocol]")
{
    // data() returns this + sizeof(DataMessage): the payload follows the header
    // immediately, with no separate pointer.
    std::vector<char> buffer(sizeof(profiler::net::DataMessage) + 4);

    profiler::net::DataMessage* message =
        new (buffer.data()) profiler::net::DataMessage(4, profiler::net::MessageType::Reply_Blocks);

    std::memcpy(buffer.data() + sizeof(profiler::net::DataMessage), "abcd", 4);

    REQUIRE(message->size == 4);
    REQUIRE(message->isEasyNetMessage());
    REQUIRE(std::strncmp(message->data(), "abcd", 4) == 0);
    REQUIRE(message->data() == buffer.data() + sizeof(profiler::net::DataMessage));
}

TEST_CASE("BlockStatusMessage carries an id and a status", "[net][protocol]")
{
    profiler::net::BlockStatusMessage message(1234u, static_cast<uint8_t>(profiler::OFF));

    REQUIRE(message.type == profiler::net::MessageType::Change_Block_Status);
    REQUIRE(message.id == 1234u);
    REQUIRE(message.status == static_cast<uint8_t>(profiler::OFF));
    REQUIRE(message.isEasyNetMessage());
}

TEST_CASE("EasyProfilerStatus carries three flags", "[net][protocol]")
{
    profiler::net::EasyProfilerStatus status(true, false, true);

    REQUIRE(status.type == profiler::net::MessageType::Connection_Accepted);
    REQUIRE(status.isProfilerEnabled);
    REQUIRE_FALSE(status.isEventTracingEnabled);
    REQUIRE(status.isLowPriorityEventTracing);
}

TEST_CASE("TimestampMessage carries max and avg", "[net][protocol]")
{
    profiler::net::TimestampMessage message(
        profiler::net::MessageType::Reply_MainThread_FPS, 1000u, 500u);

    REQUIRE(message.type == profiler::net::MessageType::Reply_MainThread_FPS);
    REQUIRE(message.maxValue == 1000u);
    REQUIRE(message.avgValue == 500u);
}

TEST_CASE("MessageType values are stable", "[net][protocol]")
{
    // The numeric values are part of the protocol: the GUI and the application
    // may be built from different revisions. Reordering the enum breaks compatibility.
    using MT = profiler::net::MessageType;

    REQUIRE(static_cast<int>(MT::Undefined) == 0);
    REQUIRE(static_cast<int>(MT::Request_Start_Capture) == 1);
    REQUIRE(static_cast<int>(MT::Reply_Capturing_Started) == 2);
    REQUIRE(static_cast<int>(MT::Request_Stop_Capture) == 3);
    REQUIRE(static_cast<int>(MT::Reply_Blocks) == 4);
    REQUIRE(static_cast<int>(MT::Reply_Blocks_End) == 5);
    REQUIRE(static_cast<int>(MT::Connection_Accepted) == 6);
    REQUIRE(static_cast<int>(MT::Request_Blocks_Description) == 7);
    REQUIRE(static_cast<int>(MT::Reply_Blocks_Description) == 8);
    REQUIRE(static_cast<int>(MT::Reply_Blocks_Description_End) == 9);
    REQUIRE(static_cast<int>(MT::Change_Block_Status) == 10);
    REQUIRE(static_cast<int>(MT::Change_Event_Tracing_Status) == 11);
    REQUIRE(static_cast<int>(MT::Change_Event_Tracing_Priority) == 12);
    REQUIRE(static_cast<int>(MT::Ping) == 13);
    REQUIRE(static_cast<int>(MT::Request_MainThread_FPS) == 14);
    REQUIRE(static_cast<int>(MT::Reply_MainThread_FPS) == 15);
}

//////////////////////////////////////////////////////////////////////////
// EasySocket over loopback

namespace {

/** Take a free port from the high range; returns 0 on failure. */
uint16_t bindToFreePort(EasySocket& socket)
{
    for (uint16_t port = 45100; port < 45200; ++port)
    {
        if (socket.bind(port) == 0)
            return port;
    }
    return 0;
}

} // namespace

TEST_CASE("A fresh socket is not connected", "[net][socket]")
{
    EasySocket socket;
    REQUIRE_FALSE(socket.isConnected());
}

TEST_CASE("bind and listen on a free port", "[net][socket]")
{
    EasySocket server;
    const uint16_t port = bindToFreePort(server);

    REQUIRE(port != 0);
    REQUIRE(server.listen() == 0);
}

TEST_CASE("accept with no client fails on timeout", "[net][socket]")
{
    EasySocket server;
    const uint16_t port = bindToFreePort(server);
    REQUIRE(port != 0);
    REQUIRE(server.listen() == 0);

    // accept() runs a select with a 500 ms timeout and returns -1 when there is
    // nobody to accept.
    REQUIRE(server.accept() < 0);
}

TEST_CASE("Data exchange over loopback", "[net][socket]")
{
    EasySocket server;
    const uint16_t port = bindToFreePort(server);
    REQUIRE(port != 0);
    REQUIRE(server.listen() == 0);

    const std::string payload = "easy_profiler loopback payload";
    std::atomic<bool> clientDone(false);
    std::atomic<bool> clientConnected(false);

    std::thread client([port, &payload, &clientDone, &clientConnected] {
        EasySocket socket;
        if (!socket.setAddress("127.0.0.1", port))
        {
            clientDone.store(true);
            return;
        }

        if (socket.connect() < 0)
        {
            clientDone.store(true);
            return;
        }

        clientConnected.store(true);
        socket.send(payload.data(), payload.size());

        // Keep the socket open until the server has read.
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        clientDone.store(true);
    });

    // accept() polls with a timeout, so retry a few times.
    int accepted = -1;
    for (int attempt = 0; attempt < 10 && accepted < 0; ++attempt)
        accepted = server.accept();

    REQUIRE(accepted >= 0);

    std::vector<char> buffer(256, 0);
    server.setReceiveTimeout(1000);
    const int received = server.receive(buffer.data(), buffer.size());

    client.join();

    REQUIRE(clientConnected.load());
    REQUIRE(received == static_cast<int>(payload.size()));
    REQUIRE(std::string(buffer.data(), static_cast<size_t>(received)) == payload);
}

//////////////////////////////////////////////////////////////////////////
// Listening thread lifecycle

TEST_CASE("startListen and stopListen toggle the state", "[net][listen]")
{
    REQUIRE_FALSE(profiler::isListening());

    // A high port so as not to collide with a real profiler.
    profiler::startListen(45250);

    // The listening thread starts asynchronously; give it time to come up.
    for (int attempt = 0; attempt < 50 && !profiler::isListening(); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    REQUIRE(profiler::isListening());

    profiler::stopListen();

    for (int attempt = 0; attempt < 50 && profiler::isListening(); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    REQUIRE_FALSE(profiler::isListening());
}

TEST_CASE("A repeated stopListen is safe", "[net][listen]")
{
    profiler::stopListen();
    profiler::stopListen();

    REQUIRE_FALSE(profiler::isListening());
}
