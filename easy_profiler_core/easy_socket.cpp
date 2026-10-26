/**
Lightweight profiler library for c++
Copyright(C) 2016-2019  Sergey Yagovtsev, Victor Zarubkin

Licensed under either of
* MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
* Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
at your option.

The MIT License
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.


The Apache License, Version 2.0 (the "License");
You may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
**/

#include <easy/easy_socket.h>
#include <string.h>
#include <thread>
#include <limits>
#if defined(__QNX__)
# include <sys/time.h>
#endif

#if defined(_WIN32)
# pragma comment (lib, "Ws2_32.lib")
# pragma comment (lib, "Mswsock.lib")
# pragma comment (lib, "AdvApi32.lib")
# ifdef max
#  undef max
# endif
#else
# include <errno.h>
# include <sys/ioctl.h>
#endif

/////////////////////////////////////////////////////////////////

#if defined(_WIN32)
const int SOCK_ABORTED = WSAECONNABORTED;
const int SOCK_RESET = WSAECONNRESET;
const int SOCK_REFUSED = WSAECONNREFUSED;
const int SOCK_IN_PROGRESS = WSAEINPROGRESS;
const int SOCK_ALREADY_IN_PROGRESS = WSAEALREADY;
#else
const int SOCK_ABORTED = ECONNABORTED;
const int SOCK_RESET = ECONNRESET;
const int SOCK_REFUSED = ECONNREFUSED;
const int SOCK_IN_PROGRESS = EINPROGRESS;
const int SOCK_ALREADY_IN_PROGRESS = EALREADY;
const int SOCK_IS_CONNECTED = EISCONN;
const int SOCK_BROKEN_PIPE = EPIPE;
const int SOCK_ENOENT = ENOENT;
#endif

const int SEND_BUFFER_SIZE = 64 * 1024 * 1024;

/////////////////////////////////////////////////////////////////

static int closeSocket(EasySocket::socket_t s)
{
#if defined(_WIN32)
    return ::closesocket(s);
#else
    return ::close(s);
#endif
}

/////////////////////////////////////////////////////////////////

bool EasySocket::checkSocket(socket_t s) const
{
#if defined(_WIN32)
    return s != INVALID_SOCKET;
#else
    return s >= 0;
#endif
}

void EasySocket::setBlocking(EasySocket::socket_t s, bool blocking)
{
#if defined(_WIN32)
    u_long iMode = blocking ? 0 : 1;//0 - blocking, 1 - non blocking
    ::ioctlsocket(s, FIONBIO, &iMode);
#else
    int iMode = blocking ? 0 : 1;//0 - blocking, 1 - non blocking
    ::ioctl(s, FIONBIO, (char*)&iMode);
#endif
}

int EasySocket::bind(uint16_t port)
{
    if (!checkSocket(m_socket))
        return -1;

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);
    auto res = ::bind(m_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    return res;
}

uint16_t EasySocket::boundPort() const
{
    if (!checkSocket(m_socket))
        return 0;

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
#if defined(_WIN32)
    int size = sizeof(address);
#else
    socklen_t size = sizeof(address);
#endif
    if (::getsockname(m_socket, reinterpret_cast<struct sockaddr*>(&address), &size) != 0)
        return 0;

    return ntohs(address.sin_port);
}

void EasySocket::flush()
{
    if (checkSocket(m_socket))
        closeSocket(m_socket);

    if (m_replySocket != m_socket && checkSocket(m_replySocket)) {
        closeSocket(m_replySocket);
    }

#if defined(_WIN32)
    m_socket = INVALID_SOCKET;
    m_replySocket = INVALID_SOCKET;
#else
    m_socket = -1;
    m_replySocket = -1;
    m_wsaret = 0;
#endif

    m_state = ConnectionState::Disconnected;
}

void EasySocket::checkResult(int result)
{
    if (result >= 0)
    {
        m_state = ConnectionState::Connected;
        return;
    }

    if (result == -1) // is this check necessary?
    {
#if defined(_WIN32)
        const int error_code = WSAGetLastError();
#else
        const int error_code = errno;
#endif

        switch (error_code)
        {
            case SOCK_ABORTED:
            case SOCK_RESET:
            case SOCK_REFUSED:
#if !defined(_WIN32)
            case SOCK_BROKEN_PIPE:
            case SOCK_ENOENT:
#endif
                m_state = ConnectionState::Disconnected;
                break;

            case SOCK_IN_PROGRESS:
            case SOCK_ALREADY_IN_PROGRESS:
                m_state = ConnectionState::Connecting;
                break;

            default:
                break;
        }
    }
}

void EasySocket::init()
{
    if (m_wsaret != 0)
        return;

#if !defined(_WIN32)
    const int protocol = 0;
#else
    const int protocol = IPPROTO_TCP;
#endif

    m_socket = ::socket(AF_INET, SOCK_STREAM, protocol);
    if (!checkSocket(m_socket))
        return;

    setBlocking(m_socket, true);

#if !defined(_WIN32)
    m_wsaret = 1;
#endif

    const int opt = 1;
    ::setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(int));
}

EasySocket::ConnectionState EasySocket::state() const
{
    return m_state;
}

bool EasySocket::isDisconnected() const
{
    return static_cast<int>(m_state) <= 0;
}

bool EasySocket::isConnected() const
{
    return m_state == ConnectionState::Connected;
}

EasySocket::EasySocket()
{
#if defined(_WIN32)
    WSADATA wsaData;
    m_wsaret = WSAStartup(0x101, &wsaData);
#else
    m_wsaret = 0;
#endif

    init();
}

EasySocket::~EasySocket()
{
    flush();

#if defined(_WIN32)
    if (m_wsaret == 0)
        WSACleanup();
#endif
}

void EasySocket::setReceiveTimeout(int milliseconds)
{
    if (!isConnected() || !checkSocket(m_replySocket))
        return;

    if (milliseconds <= 0)
        milliseconds = std::numeric_limits<int>::max() / 1000;

#if defined(_WIN32)
    const DWORD timeout = static_cast<DWORD>(milliseconds);
    ::setsockopt(m_replySocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(DWORD));
#else
    struct timeval tv;
    if (milliseconds >= 1000)
    {
        const int s = milliseconds / 1000;
        const int us = (milliseconds - s * 1000) * 1000;
        tv.tv_sec = s;
        tv.tv_usec = us;
    }
    else
    {
        tv.tv_sec = 0;
        tv.tv_usec = milliseconds * 1000;
    }

    ::setsockopt(m_replySocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof (struct timeval));
#endif
}

int EasySocket::send(const void* buffer, size_t nbytes)
{
    if (!checkSocket(m_replySocket))
        return -1;

    const char* data = static_cast<const char*>(buffer);
    size_t total = 0;
    while (total < nbytes)
    {
        const size_t remaining = nbytes - total;
        const int chunk = remaining > static_cast<size_t>(std::numeric_limits<int>::max())
                        ? std::numeric_limits<int>::max()
                        : static_cast<int>(remaining);
#if defined(_WIN32) || defined(__APPLE__)
        const int result = ::send(m_replySocket, data + total, chunk, 0);
#else
        const int result = static_cast<int>(::send(m_replySocket, data + total,
                                                   static_cast<size_t>(chunk), MSG_NOSIGNAL));
#endif
        checkResult(result);
        if (result <= 0)
            return total == 0 ? result : static_cast<int>(total);
        total += static_cast<size_t>(result);
    }

    return static_cast<int>(total);
}

int EasySocket::receive(void* buffer, size_t nbytes)
{
    if (!checkSocket(m_replySocket))
        return -1;

#if defined(_WIN32)
    const int res = ::recv(m_replySocket, (char*)buffer, (int)nbytes, 0);
#else
    const int res = (int)::read(m_replySocket, buffer, nbytes);
#endif

    checkResult(res);
    if (res == 0)
        m_state = ConnectionState::Disconnected;

    return res;
}

int EasySocket::listen(int count)
{
    if (!checkSocket(m_socket))
        return -1;

    const int res = ::listen(m_socket, count);
    checkResult(res);

    return res;
}

int EasySocket::accept()
{
    if (!checkSocket(m_socket))
        return -1;

    fd_set fdread;
    FD_ZERO (&fdread);
    FD_SET (m_socket, &fdread);

    fd_set fdwrite = fdread;
    fd_set fdexcl = fdread;

    struct timeval tv { 0, 500000 };
    const int rc = ::select((int)m_socket + 1, &fdread, &fdwrite, &fdexcl, &tv);
    if (rc <= 0)
        return -1; // there is no connection for accept

    m_replySocket = ::accept(m_socket, nullptr, nullptr);
    checkResult((int)m_replySocket);

    if (checkSocket(m_replySocket))
    {
        ::setsockopt(m_replySocket, SOL_SOCKET, SO_SNDBUF, (char*)&SEND_BUFFER_SIZE, sizeof(int));
        
        //const int flag = 1;
        //const int result = setsockopt(m_replySocket,IPPROTO_TCP,TCP_NODELAY,(char *)&flag,sizeof(int));

#if defined(__APPLE__)
        // Apple doesn't have MSG_NOSIGNAL, work around it
        const int value = 1;
        ::setsockopt(m_replySocket, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(value));
#endif

        //setBlocking(m_replySocket,true);
    }

    return (int)m_replySocket;
}

bool EasySocket::setAddress(const char* address, uint16_t port)
{
    m_hasServerAddress = false;
    if (address == nullptr)
        return false;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* addresses = nullptr;
    if (::getaddrinfo(address, nullptr, &hints, &addresses) != 0 || addresses == nullptr)
        return false;

    const struct addrinfo* selected = addresses;
    while (selected != nullptr &&
           (selected->ai_addr == nullptr || selected->ai_addrlen < sizeof(m_serverAddress)))
    {
        selected = selected->ai_next;
    }

    if (selected == nullptr)
    {
        ::freeaddrinfo(addresses);
        return false;
    }

    memcpy(&m_serverAddress, selected->ai_addr, sizeof(m_serverAddress));
    m_serverAddress.sin_port = htons(port);
    ::freeaddrinfo(addresses);
    m_hasServerAddress = true;

    return true;
}

int EasySocket::connect()
{
    if (!m_hasServerAddress || !checkSocket(m_socket))
        return -1;

    int res = 0;
    //TODO: more intelligence
#if !defined(_WIN32)
    setBlocking(m_socket, false);

    const int sleepMs = 20;
    const int waitSec = 1;
    const int waitMs = waitSec * 1000 / sleepMs;

    for (int counter = 0; counter++ < waitMs;)
    {
        res = ::connect(m_socket, (struct sockaddr*)&m_serverAddress, sizeof(m_serverAddress));

        // BSD sockets answer the retry of a non-blocking connect that has
        // completed in the meantime with EISCONN; Linux returns 0 instead.
        if (res == -1 && errno == SOCK_IS_CONNECTED)
            res = 0;

        checkResult(res);
        if (res == 0)
        {
            break;
        }

        if (m_state == ConnectionState::Connecting)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            continue;
        }

        if (isDisconnected())
        {
            break;
        }
    }

    if (res != 0 && m_state == ConnectionState::Connecting)
        m_state = ConnectionState::Disconnected;

    setBlocking(m_socket, true);
#else
    res = ::connect(m_socket, (struct sockaddr*)&m_serverAddress, sizeof(m_serverAddress));
    checkResult(res);
#endif

    if (res == 0)
    {
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        ::setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));

#if defined(__APPLE__)
        // Apple doesn't have MSG_NOSIGNAL, work around it
        const int value = 1;
        setsockopt(m_socket, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(value));
#endif

        m_replySocket = m_socket;
    }

    return res;
}
