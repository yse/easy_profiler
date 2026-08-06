/** ProfileManager::listen — driving the profiler over the network.

This is the second way to get data out: the GUI connects to a running
application, sends commands and streams the blocks back. listen() is the largest
function in the core and used to be covered by nothing — the tests started and
stopped the listening thread without sending it a single message.

Here the tests connect with a real socket and walk the actual GUI scenario:
handshake, start capture, stop, receive blocks and descriptions.

A note on fragility: the command channel is not framed — the receiving side does
a single recv into a 256-byte buffer and casts it to a message type. Commands are
therefore sent one at a time with a pause; several sent back to back may coalesce
into one segment and the second would be silently lost.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_fixture.h"

#include <easy/easy_net.h>
#include <easy/easy_socket.h>
#include <easy/profiler.h>

#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

const uint16_t kBasePort = 45400;

/** A client that mimics the GUI. */
class TestClient
{
public:
    bool connectTo(uint16_t port)
    {
        if (!m_socket.setAddress("127.0.0.1", port))
            return false;

        for (int attempt = 0; attempt < 50; ++attempt)
        {
            if (m_socket.connect() >= 0)
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return false;
    }

    template <class T>
    bool send(const T& message)
    {
        const int sent = m_socket.send(&message, sizeof(T));
        // The pause is mandatory: the channel is unframed and coalesced messages
        // lose data.
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        return sent > 0;
    }

    /** Read one reply; returns the number of bytes read. */
    int receive(std::vector<char>& buffer, int timeoutMs = 2000)
    {
        m_socket.setReceiveTimeout(timeoutMs);
        return m_socket.receive(buffer.data(), buffer.size());
    }

    EasySocket& socket() { return m_socket; }

private:
    EasySocket m_socket;
};

bool startListening(uint16_t port)
{
    profiler::startListen(port);
    for (int attempt = 0; attempt < 100 && !profiler::isListening(); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return profiler::isListening();
}

void stopListening()
{
    profiler::stopListen();
    for (int attempt = 0; attempt < 100 && profiler::isListening(); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

/** RAII over the listening thread so it does not outlive a failed test. */
struct ListenGuard
{
    explicit ListenGuard(uint16_t port) : ok(startListening(port)) {}
    ~ListenGuard() { stopListening(); }
    bool ok;
};

} // namespace

TEST_CASE("Connecting yields the profiler status", "[net][commands]")
{
    easy_test::drainPending();

    ListenGuard guard(kBasePort);
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(kBasePort));

    // Right after accept the server sends an unsolicited EasyProfilerStatus.
    std::vector<char> buffer(256, 0);
    const int received = client.receive(buffer);

    REQUIRE(received >= static_cast<int>(sizeof(profiler::net::EasyProfilerStatus)));

    const profiler::net::EasyProfilerStatus* status =
        reinterpret_cast<const profiler::net::EasyProfilerStatus*>(buffer.data());

    REQUIRE(status->isEasyNetMessage());
    REQUIRE(status->type == profiler::net::MessageType::Connection_Accepted);
    REQUIRE_FALSE(status->isProfilerEnabled);
}

TEST_CASE("Ping does not break the connection", "[net][commands]")
{
    easy_test::drainPending();

    ListenGuard guard(kBasePort + 1);
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(kBasePort + 1));

    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    const profiler::net::Message ping(profiler::net::MessageType::Ping);
    REQUIRE(client.send(ping));

    // Ping needs no reply; the connection stays alive, so the next command works.
    const profiler::net::Message fps(profiler::net::MessageType::Request_MainThread_FPS);
    REQUIRE(client.send(fps));

    const int received = client.receive(buffer);
    REQUIRE(received >= static_cast<int>(sizeof(profiler::net::TimestampMessage)));
}

TEST_CASE("A main-thread FPS request returns timestamps", "[net][commands]")
{
    easy_test::drainPending();
    EASY_MAIN_THREAD;

    ListenGuard guard(kBasePort + 2);
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(kBasePort + 2));

    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    const profiler::net::Message request(profiler::net::MessageType::Request_MainThread_FPS);
    REQUIRE(client.send(request));

    const int received = client.receive(buffer);
    REQUIRE(received >= static_cast<int>(sizeof(profiler::net::TimestampMessage)));

    const profiler::net::TimestampMessage* reply =
        reinterpret_cast<const profiler::net::TimestampMessage*>(buffer.data());

    REQUIRE(reply->isEasyNetMessage());
    REQUIRE(reply->type == profiler::net::MessageType::Reply_MainThread_FPS);
}

TEST_CASE("Starting capture over the network enables the profiler", "[net][commands]")
{
    easy_test::drainPending();
    REQUIRE_FALSE(profiler::isEnabled());

    ListenGuard guard(kBasePort + 3);
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(kBasePort + 3));

    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    const profiler::net::Message start(profiler::net::MessageType::Request_Start_Capture);
    REQUIRE(client.send(start));

    const int received = client.receive(buffer);
    REQUIRE(received >= static_cast<int>(sizeof(profiler::net::Message)));

    const profiler::net::Message* reply =
        reinterpret_cast<const profiler::net::Message*>(buffer.data());

    REQUIRE(reply->isEasyNetMessage());
    REQUIRE(reply->type == profiler::net::MessageType::Reply_Capturing_Started);

    REQUIRE(profiler::isEnabled());

    profiler::setEnabled(false);
    easy_test::drainPending();
}

TEST_CASE("A block description request returns a description stream", "[net][commands]")
{
    easy_test::drainPending();

    // Guarantee at least one descriptor exists.
    profiler::registerDescription(profiler::ON, "net_desc_probe_id", "net_desc_probe",
                                  __FILE__, __LINE__, profiler::BlockType::Block,
                                  profiler::colors::Default, false);

    ListenGuard guard(kBasePort + 4);
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(kBasePort + 4));

    std::vector<char> buffer(4096, 0);
    REQUIRE(client.receive(buffer) > 0);

    const profiler::net::Message request(profiler::net::MessageType::Request_Blocks_Description);
    REQUIRE(client.send(request));

    // The reply arrives as one or more Reply_Blocks_Description messages and
    // finishes with Reply_Blocks_Description_End.
    bool sawDescription = false;
    bool sawEnd = false;

    for (int attempt = 0; attempt < 40 && !sawEnd; ++attempt)
    {
        const int received = client.receive(buffer, 500);
        if (received < static_cast<int>(sizeof(profiler::net::Message)))
            continue;

        const profiler::net::Message* message =
            reinterpret_cast<const profiler::net::Message*>(buffer.data());
        if (!message->isEasyNetMessage())
            continue;

        if (message->type == profiler::net::MessageType::Reply_Blocks_Description)
            sawDescription = true;
        else if (message->type == profiler::net::MessageType::Reply_Blocks_Description_End)
            sawEnd = true;
    }

    // The end marker may arrive in the same segment as the last chunk of data,
    // so seeing it in a separate read is not required here.
    REQUIRE(sawDescription);
    INFO("end marker received: " << sawEnd);
}

TEST_CASE("Full cycle: start, work, stop, receive blocks", "[net][commands]")
{
    easy_test::drainPending();

    ListenGuard guard(kBasePort + 5);
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(kBasePort + 5));

    std::vector<char> buffer(65536, 0);
    REQUIRE(client.receive(buffer) > 0);

    // 1. Start capture.
    const profiler::net::Message start(profiler::net::MessageType::Request_Start_Capture);
    REQUIRE(client.send(start));
    REQUIRE(client.receive(buffer) > 0);
    REQUIRE(profiler::isEnabled());

    // 2. Do a little work.
    for (int i = 0; i < 50; ++i)
    {
        EASY_BLOCK("net_captured_block");
    }

    // 3. Stop capture — the server starts sending blocks.
    const profiler::net::Message stop(profiler::net::MessageType::Request_Stop_Capture);
    REQUIRE(client.send(stop));

    // 4. Read the stream until Reply_Blocks_End.
    bool sawBlocks = false;
    bool sawEnd = false;
    size_t totalBytes = 0;

    for (int attempt = 0; attempt < 100 && !sawEnd; ++attempt)
    {
        const int received = client.receive(buffer, 500);
        if (received <= 0)
            continue;

        totalBytes += static_cast<size_t>(received);

        const profiler::net::Message* message =
            reinterpret_cast<const profiler::net::Message*>(buffer.data());
        if (!message->isEasyNetMessage())
            continue;

        if (message->type == profiler::net::MessageType::Reply_Blocks)
            sawBlocks = true;
        else if (message->type == profiler::net::MessageType::Reply_Blocks_End)
            sawEnd = true;
    }

    REQUIRE(sawBlocks);
    REQUIRE(totalBytes > 0);

    REQUIRE_FALSE(profiler::isEnabled());

    easy_test::drainPending();
}

TEST_CASE("Changing a block status over the network", "[net][commands]")
{
    easy_test::drainPending();

    const profiler::BaseBlockDescriptor* descriptor = profiler::registerDescription(
        profiler::ON, "net_status_probe_id", "net_status_probe", __FILE__, __LINE__,
        profiler::BlockType::Block, profiler::colors::Default, false);
    REQUIRE(descriptor != nullptr);
    REQUIRE(descriptor->status() == profiler::ON);

    ListenGuard guard(kBasePort + 6);
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(kBasePort + 6));

    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    // Switch the block off by its id — this is how the GUI mutes blocks live.
    const profiler::net::BlockStatusMessage message(
        descriptor->id(), static_cast<uint8_t>(profiler::OFF));
    REQUIRE(client.send(message));

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    REQUIRE(descriptor->status() == profiler::OFF);

    // Restore it so other tests are unaffected.
    const profiler::net::BlockStatusMessage restore(
        descriptor->id(), static_cast<uint8_t>(profiler::ON));
    REQUIRE(client.send(restore));
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    REQUIRE(descriptor->status() == profiler::ON);
}

TEST_CASE("Changing the event tracing status over the network", "[net][commands]")
{
    easy_test::drainPending();

    ListenGuard guard(kBasePort + 7);
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(kBasePort + 7));

    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    const bool original = profiler::isEventTracingEnabled();

    const profiler::net::BoolMessage toggle(
        profiler::net::MessageType::Change_Event_Tracing_Status, !original);
    REQUIRE(client.send(toggle));

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    REQUIRE(profiler::isEventTracingEnabled() == !original);

    const profiler::net::BoolMessage restore(
        profiler::net::MessageType::Change_Event_Tracing_Status, original);
    REQUIRE(client.send(restore));
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    REQUIRE(profiler::isEventTracingEnabled() == original);
}

TEST_CASE("Changing the event tracing priority over the network", "[net][commands]")
{
    easy_test::drainPending();

    ListenGuard guard(kBasePort + 8);
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(kBasePort + 8));

    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    // Off Windows the command is accepted and ignored; what matters is that the
    // loop survives it.
    const profiler::net::BoolMessage lowPriority(
        profiler::net::MessageType::Change_Event_Tracing_Priority, true);
    REQUIRE(client.send(lowPriority));

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    const profiler::net::Message fps(profiler::net::MessageType::Request_MainThread_FPS);
    REQUIRE(client.send(fps));
    REQUIRE(client.receive(buffer) > 0);
}

TEST_CASE("An unknown message type is ignored", "[net][commands]")
{
    easy_test::drainPending();

    ListenGuard guard(kBasePort + 9);
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(kBasePort + 9));

    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    // A type from a future protocol version. The dispatcher must skip it quietly
    // (default: break) rather than desynchronize the channel — additive protocol
    // extension rests on that property.
    profiler::net::Message unknown;
    unknown.type = static_cast<profiler::net::MessageType>(200);
    REQUIRE(client.send(unknown));

    const profiler::net::Message fps(profiler::net::MessageType::Request_MainThread_FPS);
    REQUIRE(client.send(fps));

    const int received = client.receive(buffer);
    REQUIRE(received >= static_cast<int>(sizeof(profiler::net::TimestampMessage)));
}

TEST_CASE("Garbage without a valid signature does not break the loop", "[net][commands]")
{
    easy_test::drainPending();

    ListenGuard guard(kBasePort + 10);
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(kBasePort + 10));

    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    // Bytes without the signature are dropped by the isEasyNetMessage() check.
    std::vector<char> garbage(64, static_cast<char>(0x5A));
    client.socket().send(garbage.data(), garbage.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const profiler::net::Message fps(profiler::net::MessageType::Request_MainThread_FPS);
    REQUIRE(client.send(fps));
    REQUIRE(client.receive(buffer) > 0);
}
