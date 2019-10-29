//
// Created by vzarubkin on 29.10.19.
//

#ifndef EASY_PROFILER_SOCKET_LISTENER_H
#define EASY_PROFILER_SOCKET_LISTENER_H

#include <atomic>
#include <sstream>
#include <string>
#include <thread>

#include <QObject>

#include <easy/easy_socket.h>

namespace profiler { namespace net { struct EasyProfilerStatus; } }

//////////////////////////////////////////////////////////////////////////

enum class ListenerRegime : uint8_t
{
    Idle = 0,
    Capture,
    Capture_Receive,
    Descriptors
};

class SocketListener Q_DECL_FINAL
{
    EasySocket            m_easySocket; ///<
    std::string              m_address; ///<
    std::stringstream   m_receivedData; ///<
    std::thread               m_thread; ///<
    uint64_t            m_receivedSize; ///<
    uint16_t                    m_port; ///<
    std::atomic<uint32_t>   m_frameMax; ///<
    std::atomic<uint32_t>   m_frameAvg; ///<
    std::atomic_bool      m_bInterrupt; ///<
    std::atomic_bool      m_bConnected; ///<
    std::atomic_bool    m_bStopReceive; ///<
    std::atomic_bool   m_bCaptureReady; ///<
    std::atomic_bool m_bFrameTimeReady; ///<
    ListenerRegime            m_regime; ///<

public:

    SocketListener();
    ~SocketListener();

    bool connected() const;
    bool captured() const;
    ListenerRegime regime() const;
    uint64_t size() const;
    const std::string& address() const;
    uint16_t port() const;

    std::stringstream& data();
    void clearData();

    void disconnect();
    void closeSocket();
    bool connect(const char* _ipaddress, uint16_t _port, profiler::net::EasyProfilerStatus& _reply, bool _disconnectFirst = false);
    bool reconnect(const char* _ipaddress, uint16_t _port, profiler::net::EasyProfilerStatus& _reply);

    bool startCapture();
    void stopCapture();
    void finalizeCapture();
    void requestBlocksDescription();

    bool frameTime(uint32_t& _maxTime, uint32_t& _avgTime);
    bool requestFrameTime();

    template <class T>
    void send(const T& _message) {
        m_easySocket.send(&_message, sizeof(T));
    }

private:

    void listenCapture();
    void listenDescription();
    void listenFrameTime();

}; // END of class SocketListener.

#endif //EASY_PROFILER_SOCKET_LISTENER_H
