/**
Lightweight profiler library for c++
Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin


Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


GNU General Public License Usage
Alternatively, this file may be used under the terms of the GNU
General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see <http://www.gnu.org/licenses/>.
**/
#ifndef EASY________SOCKET_________H
#define EASY________SOCKET_________H

#include <stdint.h>
#include "easy/profiler.h"
#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#else

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#endif

class PROFILER_API EasySocket
{
public:

#ifdef _WIN32
    typedef SOCKET socket_t;
#else
    typedef int socket_t;
#endif

    enum ConnectionState
    {
        CONNECTION_STATE_UNKNOWN,
        CONNECTION_STATE_SUCCESS,

        CONNECTION_STATE_DISCONNECTED,
        CONNECTION_STATE_IN_PROGRESS
    };

private:
    
    void checkResult(int result);
    bool checkSocket(socket_t s) const;
    static int _close(socket_t s);
    void setBlocking(socket_t s, bool blocking);
    
    socket_t m_socket = 0;
    socket_t m_replySocket = 0;

    int wsaret = -1;

    struct hostent * server;
    struct sockaddr_in serv_addr;

    ConnectionState m_state = CONNECTION_STATE_UNKNOWN;

public:

    EasySocket();
    ~EasySocket();

    int send(const void *buf, size_t nbyte);
    int receive(void *buf, size_t nbyte);
    int listen(int count=5);
    int accept();
    int bind(uint16_t portno);

    bool setAddress(const char* serv, uint16_t port);
    int connect();

    void flush();
    void init();

    void setState(ConnectionState state){m_state=state;}
    ConnectionState state() const{return m_state;}

    bool isDisconnected() const
    {
        return  m_state == CONNECTION_STATE_UNKNOWN ||
                m_state == CONNECTION_STATE_DISCONNECTED;
    }
};

#endif // EASY________SOCKET_________H
