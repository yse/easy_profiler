/** ProfileManager::listen — driving the profiler over the network.

This is the second way to get data out: the GUI connects to a running
application, sends commands and streams the blocks back. listen() is the largest
function in the core and used to be covered by nothing — the tests started and
stopped the listening thread without sending it a single message.

Here the tests connect with a real socket and walk the actual GUI scenario:
handshake, start capture, stop, receive blocks and descriptions.

The tests intentionally fragment and coalesce commands because TCP is a byte
stream. Both the production listener and this client retain partial frames.
*/

#include <catch2/catch_test_macros.hpp>

#include "support/prof_fixture.h"

#include <easy/easy_net.h>
#include <easy/easy_socket.h>
#include <easy/profiler.h>

#include <chrono>
#include <cstring>
#include <future>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace {

uint16_t freePort()
{
    EasySocket socket;
    if (socket.bind(0) != 0)
        return 0;
    return socket.boundPort();
}

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
        return sendRaw(&message, sizeof(T));
    }

    bool sendRaw(const void* data, size_t size)
    {
        return m_socket.send(data, size) == static_cast<int>(size);
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
    // TSan startup can be considerably slower on a loaded CI worker.
    for (int attempt = 0; attempt < 500 && !profiler::isListening(); ++attempt)
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
    ListenGuard() : port(freePort()), ok(port != 0 && startListening(port)) {}
    ~ListenGuard() { stopListening(); }
    uint16_t port;
    bool ok;
};

} // namespace

TEST_CASE("Connecting yields the profiler status", "[net][commands]")
{
    easy_test::drainPending();

    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));

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

TEST_CASE("Stopping the listener does not wait for an idle client", "[net][listen][threads]")
{
    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));

    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    std::future<void> stopper = std::async(std::launch::async, [] {
        profiler::stopListen();
    });
    const std::future_status initialStatus =
        stopper.wait_for(std::chrono::milliseconds(750));

    // Always unblock the old implementation before asserting, so a RED test
    // exits normally instead of leaving std::future's destructor waiting.
    if (initialStatus != std::future_status::ready)
        client.socket().flush();

    REQUIRE(stopper.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    stopper.get();
    REQUIRE(initialStatus == std::future_status::ready);
    REQUIRE_FALSE(profiler::isListening());
}

TEST_CASE("Ping does not break the connection", "[net][commands]")
{
    easy_test::drainPending();

    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));

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

    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));

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

    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));

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

TEST_CASE("Starting an active network capture is idempotent", "[net][commands]")
{
    easy_test::drainPending();
    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));
    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    const profiler::net::Message start(profiler::net::MessageType::Request_Start_Capture);
    REQUIRE(client.send(start));
    REQUIRE(client.receive(buffer) >= static_cast<int>(sizeof(profiler::net::Message)));
    REQUIRE(profiler::isEnabled());

    REQUIRE(client.send(start));
    REQUIRE(client.receive(buffer) >= static_cast<int>(sizeof(profiler::net::Message)));
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

    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));

    std::vector<char> buffer(4096, 0);
    std::vector<char> pending;
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
        if (received <= 0)
            continue;

        pending.insert(pending.end(), buffer.begin(), buffer.begin() + received);
        while (pending.size() >= sizeof(profiler::net::Message))
        {
            const profiler::net::Message* message =
                reinterpret_cast<const profiler::net::Message*>(pending.data());
            REQUIRE(message->isEasyNetMessage());

            size_t frameSize = sizeof(profiler::net::Message);
            if (message->type == profiler::net::MessageType::Reply_Blocks_Description)
            {
                if (pending.size() < sizeof(profiler::net::DataMessage))
                    break;
                const auto data = reinterpret_cast<const profiler::net::DataMessage*>(pending.data());
                frameSize = sizeof(*data) + data->size;
                if (pending.size() < frameSize)
                    break;
                sawDescription = true;
            }
            else if (message->type == profiler::net::MessageType::Reply_Blocks_Description_End)
            {
                sawEnd = true;
            }

            pending.erase(pending.begin(), pending.begin() + frameSize);
        }
    }

    REQUIRE(sawDescription);
    REQUIRE(sawEnd);
}

TEST_CASE("A fragmented command is reassembled", "[net][commands][framing]")
{
    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));

    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    const profiler::net::Message request(profiler::net::MessageType::Request_MainThread_FPS);
    const char* bytes = reinterpret_cast<const char*>(&request);
    REQUIRE(client.sendRaw(bytes, 2));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    REQUIRE(client.sendRaw(bytes + 2, sizeof(request) - 2));

    const int received = client.receive(buffer);
    REQUIRE(received >= static_cast<int>(sizeof(profiler::net::TimestampMessage)));
    const auto reply = reinterpret_cast<const profiler::net::TimestampMessage*>(buffer.data());
    REQUIRE(reply->type == profiler::net::MessageType::Reply_MainThread_FPS);
}

TEST_CASE("A command header waits for the rest of its payload", "[net][commands][framing]")
{
    easy_test::drainPending();
    const profiler::BaseBlockDescriptor* descriptor = profiler::registerDescription(
        profiler::ON, "net_fragmented_status_id", "net_fragmented_status", __FILE__, __LINE__,
        profiler::BlockType::Block, profiler::colors::Default, false);
    REQUIRE(descriptor != nullptr);

    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));
    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    const profiler::net::BlockStatusMessage disable(
        descriptor->id(), static_cast<uint8_t>(profiler::OFF));
    const char* bytes = reinterpret_cast<const char*>(&disable);
    REQUIRE(client.sendRaw(bytes, sizeof(profiler::net::Message)));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE(descriptor->status() == profiler::ON);

    REQUIRE(client.sendRaw(bytes + sizeof(profiler::net::Message),
                           sizeof(disable) - sizeof(profiler::net::Message)));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE(descriptor->status() == profiler::OFF);

    const profiler::net::BlockStatusMessage restore(
        descriptor->id(), static_cast<uint8_t>(profiler::ON));
    REQUIRE(client.send(restore));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE(descriptor->status() == profiler::ON);
}

TEST_CASE("Coalesced commands are processed independently", "[net][commands][framing]")
{
    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));

    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    const profiler::net::Message ping(profiler::net::MessageType::Ping);
    const profiler::net::Message fps(profiler::net::MessageType::Request_MainThread_FPS);
    std::vector<char> commands(sizeof(ping) + sizeof(fps));
    std::memcpy(commands.data(), &ping, sizeof(ping));
    std::memcpy(commands.data() + sizeof(ping), &fps, sizeof(fps));
    REQUIRE(client.sendRaw(commands.data(), commands.size()));

    const int received = client.receive(buffer);
    REQUIRE(received >= static_cast<int>(sizeof(profiler::net::TimestampMessage)));
    const auto reply = reinterpret_cast<const profiler::net::TimestampMessage*>(buffer.data());
    REQUIRE(reply->type == profiler::net::MessageType::Reply_MainThread_FPS);
}

TEST_CASE("Full cycle: start, work, stop, receive blocks", "[net][commands]")
{
    easy_test::drainPending();

    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));

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
    std::vector<char> pending;

    for (int attempt = 0; attempt < 100 && !sawEnd; ++attempt)
    {
        const int received = client.receive(buffer, 500);
        if (received <= 0)
            continue;

        totalBytes += static_cast<size_t>(received);
        pending.insert(pending.end(), buffer.begin(), buffer.begin() + received);
        while (pending.size() >= sizeof(profiler::net::Message))
        {
            const auto message = reinterpret_cast<const profiler::net::Message*>(pending.data());
            REQUIRE(message->isEasyNetMessage());

            size_t frameSize = sizeof(profiler::net::Message);
            if (message->type == profiler::net::MessageType::Reply_Blocks)
            {
                if (pending.size() < sizeof(profiler::net::DataMessage))
                    break;
                const auto data = reinterpret_cast<const profiler::net::DataMessage*>(pending.data());
                frameSize = sizeof(*data) + data->size;
                if (pending.size() < frameSize)
                    break;
                sawBlocks = true;
            }
            else if (message->type == profiler::net::MessageType::Reply_Blocks_End)
            {
                sawEnd = true;
            }

            pending.erase(pending.begin(), pending.begin() + frameSize);
        }
    }

    REQUIRE(sawBlocks);
    REQUIRE(sawEnd);
    REQUIRE(totalBytes > 0);

    REQUIRE_FALSE(profiler::isEnabled());

    easy_test::drainPending();
}

TEST_CASE("A description request cancels an unfinished block dump", "[net][commands][cancel]")
{
    easy_test::drainPending();
    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));
    std::vector<char> buffer(65536, 0);
    REQUIRE(client.receive(buffer) > 0);

    const profiler::net::Message start(profiler::net::MessageType::Request_Start_Capture);
    REQUIRE(client.send(start));
    REQUIRE(client.receive(buffer) > 0);

    for (int i = 0; i < 50000; ++i)
    {
        EASY_BLOCK("cancelled_network_dump_block");
    }

    // Coalescing guarantees that the description request is already pending
    // when the listener starts its asynchronous block dump.
    const profiler::net::Message stop(profiler::net::MessageType::Request_Stop_Capture);
    const profiler::net::Message descriptions(
        profiler::net::MessageType::Request_Blocks_Description);
    std::vector<char> commands(sizeof(stop) + sizeof(descriptions));
    std::memcpy(commands.data(), &stop, sizeof(stop));
    std::memcpy(commands.data() + sizeof(stop), &descriptions, sizeof(descriptions));
    REQUIRE(client.sendRaw(commands.data(), commands.size()));

    bool sawDescription = false;
    bool sawDescriptionEnd = false;
    std::vector<char> pending;
    for (int attempt = 0; attempt < 100 && !sawDescriptionEnd; ++attempt)
    {
        const int received = client.receive(buffer, 500);
        if (received <= 0)
            continue;

        pending.insert(pending.end(), buffer.begin(), buffer.begin() + received);
        while (pending.size() >= sizeof(profiler::net::Message))
        {
            const auto message =
                reinterpret_cast<const profiler::net::Message*>(pending.data());
            REQUIRE(message->isEasyNetMessage());

            size_t frameSize = sizeof(profiler::net::Message);
            if (message->type == profiler::net::MessageType::Reply_Blocks ||
                message->type == profiler::net::MessageType::Reply_Blocks_Description)
            {
                if (pending.size() < sizeof(profiler::net::DataMessage))
                    break;
                const auto data =
                    reinterpret_cast<const profiler::net::DataMessage*>(pending.data());
                frameSize = sizeof(*data) + data->size;
                if (pending.size() < frameSize)
                    break;
                if (message->type == profiler::net::MessageType::Reply_Blocks_Description)
                    sawDescription = true;
            }
            else if (message->type == profiler::net::MessageType::Reply_Blocks_Description_End)
            {
                sawDescriptionEnd = true;
            }

            pending.erase(pending.begin(), pending.begin() + frameSize);
        }
    }

    REQUIRE(sawDescription);
    REQUIRE(sawDescriptionEnd);
    REQUIRE_FALSE(profiler::isEnabled());
}

TEST_CASE("Disconnecting during a block dump leaves the listener reusable", "[net][commands][cancel]")
{
    easy_test::drainPending();
    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));
    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    const profiler::net::Message start(profiler::net::MessageType::Request_Start_Capture);
    REQUIRE(client.send(start));
    REQUIRE(client.receive(buffer) > 0);
    for (int i = 0; i < 100; ++i)
    {
        EASY_BLOCK("disconnected_network_dump_block");
    }

    const profiler::net::Message stop(profiler::net::MessageType::Request_Stop_Capture);
    REQUIRE(client.send(stop));
    client.socket().flush();

    // The listener cancels and joins the asynchronous dump before accepting
    // another GUI client.
    TestClient replacement;
    REQUIRE(replacement.connectTo(guard.port));
    REQUIRE(replacement.receive(buffer) >=
            static_cast<int>(sizeof(profiler::net::EasyProfilerStatus)));
    REQUIRE_FALSE(profiler::isEnabled());
}

TEST_CASE("Stopping the listener cancels an unfinished block dump", "[net][listen][cancel]")
{
    easy_test::drainPending();
    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));
    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    const profiler::net::Message start(profiler::net::MessageType::Request_Start_Capture);
    REQUIRE(client.send(start));
    REQUIRE(client.receive(buffer) > 0);
    for (int i = 0; i < 100; ++i)
    {
        EASY_BLOCK("stopped_listener_dump_block");
    }

    const profiler::net::Message stop(profiler::net::MessageType::Request_Stop_Capture);
    REQUIRE(client.send(stop));

    for (int attempt = 0; attempt < 100 && profiler::isEnabled(); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    REQUIRE_FALSE(profiler::isEnabled());

    std::future<void> stopper = std::async(std::launch::async, [] {
        profiler::stopListen();
    });
    REQUIRE(stopper.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    stopper.get();
    REQUIRE_FALSE(profiler::isListening());
}

TEST_CASE("A repeated stop command is ignored while a block dump is active", "[net][commands]")
{
    easy_test::drainPending();
    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));
    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    const profiler::net::Message start(profiler::net::MessageType::Request_Start_Capture);
    REQUIRE(client.send(start));
    REQUIRE(client.receive(buffer) > 0);
    { EASY_BLOCK("repeated_stop_dump_block"); }

    const profiler::net::Message stop(profiler::net::MessageType::Request_Stop_Capture);
    std::vector<char> commands(sizeof(stop) * 2U);
    std::memcpy(commands.data(), &stop, sizeof(stop));
    std::memcpy(commands.data() + sizeof(stop), &stop, sizeof(stop));
    REQUIRE(client.sendRaw(commands.data(), commands.size()));

    for (int attempt = 0; attempt < 100 && profiler::isEnabled(); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    REQUIRE_FALSE(profiler::isEnabled());
}

TEST_CASE("Stopping an inactive capture leaves it disabled", "[net][commands]")
{
    easy_test::drainPending();
    REQUIRE_FALSE(profiler::isEnabled());
    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));
    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    const profiler::net::Message stop(profiler::net::MessageType::Request_Stop_Capture);
    REQUIRE(client.send(stop));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE_FALSE(profiler::isEnabled());
}

TEST_CASE("Changing a block status over the network", "[net][commands]")
{
    easy_test::drainPending();

    const profiler::BaseBlockDescriptor* descriptor = profiler::registerDescription(
        profiler::ON, "net_status_probe_id", "net_status_probe", __FILE__, __LINE__,
        profiler::BlockType::Block, profiler::colors::Default, false);
    REQUIRE(descriptor != nullptr);
    REQUIRE(descriptor->status() == profiler::ON);

    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));

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

TEST_CASE("An unknown block id does not disrupt network commands", "[net][commands][robustness]")
{
    easy_test::drainPending();

    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));

    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    const profiler::net::BlockStatusMessage unknown(
        std::numeric_limits<uint32_t>::max(), static_cast<uint8_t>(profiler::OFF));
    REQUIRE(client.send(unknown));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const profiler::net::Message fps(profiler::net::MessageType::Request_MainThread_FPS);
    REQUIRE(client.send(fps));
    const int received = client.receive(buffer);
    REQUIRE(received >= static_cast<int>(sizeof(profiler::net::TimestampMessage)));
}

TEST_CASE("Block status cannot change during an active capture", "[net][commands][status]")
{
    easy_test::drainPending();
    const profiler::BaseBlockDescriptor* descriptor = profiler::registerDescription(
        profiler::ON, "net_active_status_id", "net_active_status", __FILE__, __LINE__,
        profiler::BlockType::Block, profiler::colors::Default, false);
    REQUIRE(descriptor != nullptr);

    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));
    std::vector<char> buffer(256, 0);
    REQUIRE(client.receive(buffer) > 0);

    profiler::setEnabled(true);
    const profiler::net::BlockStatusMessage disable(
        descriptor->id(), static_cast<uint8_t>(profiler::OFF));
    REQUIRE(client.send(disable));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const bool stayedEnabled = descriptor->status() == profiler::ON;
    profiler::setEnabled(false);

    REQUIRE(stayedEnabled);

    REQUIRE(client.send(disable));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE(descriptor->status() == profiler::OFF);

    const profiler::net::BlockStatusMessage restore(
        descriptor->id(), static_cast<uint8_t>(profiler::ON));
    REQUIRE(client.send(restore));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE(descriptor->status() == profiler::ON);
    easy_test::drainPending();
}

TEST_CASE("Changing the event tracing status over the network", "[net][commands]")
{
    easy_test::drainPending();

    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));

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

    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));

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

    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));

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

    ListenGuard guard;
    REQUIRE(guard.ok);

    TestClient client;
    REQUIRE(client.connectTo(guard.port));

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
