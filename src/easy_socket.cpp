/**
Lightweight profiler library for c++
Copyright(C) 2016  Sergey Yagovtsev

This program is free software : you can redistribute it and / or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see <http://www.gnu.org/licenses/>.
**/

#include "easy_socket.h"


#ifndef _WIN32
#include <strings.h>

EasySocket::EasySocket()
{
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct timeval tv;

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
}

EasySocket::~EasySocket()
{
}

size_t EasySocket::write(const void *buf, size_t nbyte)
{
    if(m_socket <= 0){
        return -1;
    }
    return ::write(m_socket,buf,nbyte);
}

size_t EasySocket::read(void *buf, size_t nbyte)
{
    if(m_socket <= 0){
        return -1;
    }
    return ::read(m_socket,buf,nbyte);
}

bool EasySocket::setAddress(const char *serv, uint16_t portno)
{
    server = gethostbyname(serv);
    if (server == NULL) {
        return false;
        //fprintf(stderr,"ERROR, no such host\n");
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);

    return true;
}

int EasySocket::connect()
{
    if (server == NULL || m_socket <=0 ) {
        return -1;
        //fprintf(stderr,"ERROR, no such host\n");
    }

    return ::connect(m_socket,(struct sockaddr *) &serv_addr,sizeof(serv_addr));
}
#else

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

EasySocket::EasySocket()
{
    // socket
    WSADATA wsaData;
    int wsaret = WSAStartup(0x101, &wsaData);

    if (wsaret == 0)
    {
        m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);;
        if (m_socket == INVALID_SOCKET) {
            WSACleanup();
        }
    }
    

}

EasySocket::~EasySocket()
{
    if (m_socket)
        WSACleanup();
}

size_t EasySocket::write(const void *buf, size_t nbyte)
{
    if (m_socket <= 0){
        return -1;
    }
    return send(m_socket, (const char*)buf, nbyte,0);
}

size_t EasySocket::read(void *buf, size_t nbyte)
{
    if (m_socket <= 0){
        return -1;
    }
    return recv(m_socket, (char*)buf, nbyte,0);
}

bool EasySocket::setAddress(const char *serv, uint16_t portno)
{
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    int iResult;
    char buffer[20] = {};
    _itoa(portno, buffer, 10);
    iResult = getaddrinfo(serv, buffer, &hints, &result);
    if (iResult != 0) {
        WSACleanup();
        return false;
    }

    return false;
}

int EasySocket::connect()
{
    if (!m_socket || !result){
        return -1;
    }
    
    SOCKET ConnectSocket = socket(result->ai_family, result->ai_socktype,
        result->ai_protocol);
    if (ConnectSocket == INVALID_SOCKET) {
        return -1;
    }

    // Connect to server.
    auto iResult = ::connect(ConnectSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        closesocket(ConnectSocket);
        ConnectSocket = INVALID_SOCKET;
    }
    
    return iResult;
}



#endif
