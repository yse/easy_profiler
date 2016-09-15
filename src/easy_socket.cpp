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

#include "profiler/easy_socket.h"


#ifndef _WIN32
#include <strings.h>

int EasySocket::bind(uint16_t portno)
{
    if(m_socket < 0 ) return -1;
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    return ::bind(m_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
}

EasySocket::EasySocket()
{
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct timeval tv;

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    //setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
}

EasySocket::~EasySocket()
{
}

int EasySocket::send(const void *buf, size_t nbyte)
{
    if(m_replySocket <= 0)  return -1;
    return ::write(m_replySocket,buf,nbyte);
}

int EasySocket::receive(void *buf, size_t nbyte)
{
    if(m_replySocket <= 0) return -1;
    return ::read(m_replySocket,buf,nbyte);
}

int EasySocket::listen(int count)
{
    if(m_socket < 0 ) return -1;
    return ::listen(m_socket,count);
}

int EasySocket::accept()
{
    if(m_socket < 0 ) return -1;
    m_replySocket = ::accept(m_socket,nullptr,nullptr);
    return m_replySocket;
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
    

    u_long iMode = 0;//0 - blocking, 1 - non blocking
    ioctlsocket(m_socket, FIONBIO, &iMode);

    int opt = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

}

EasySocket::~EasySocket()
{
    if (m_socket)
        WSACleanup();
}

int EasySocket::send(const void *buf, size_t nbyte)
{
    if (m_replySocket <= 0){
        return -1;
    }
    return ::send(m_replySocket, (const char*)buf, nbyte, 0);
}

#include <stdio.h>

int EasySocket::receive(void *buf, size_t nbyte)
{
    if (m_replySocket <= 0){
        return -1;
    }

    int res = ::recv(m_replySocket, (char*)buf, nbyte, 0);

    if (res == SOCKET_ERROR)
    {
        LPWSTR *s = NULL;
        int err = WSAGetLastError();
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, err,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&s, 0, NULL);
        printf("%S\n", s);
        LocalFree(s);
    }
    return res;
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

    return true;
}

int EasySocket::connect()
{
    if (!m_socket || !result){
        return -1;
    }
    /**
    SOCKET  ConnectSocket = socket(result->ai_family, result->ai_socktype,
        result->ai_protocol);
    if (ConnectSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return -1;
    }

    // Connect to server.
    auto iResult = ::connect(ConnectSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        closesocket(ConnectSocket);
        ConnectSocket = INVALID_SOCKET;
        return -1;
    }
    m_socket = ConnectSocket;
    /**/
    // Connect to server.
    auto iResult = ::connect(m_socket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    /**/
    m_replySocket = m_socket;
    return iResult;
}


int EasySocket::listen(int count)
{
    if (m_socket < 0) return -1;
    return ::listen(m_socket, count);
}

int EasySocket::accept()
{
    if (m_socket < 0) return -1;
    m_replySocket = ::accept(m_socket, nullptr, nullptr);

    int send_buffer = 64 * 1024*1024;    // 64 MB
    int send_buffer_sizeof = sizeof(int);
    setsockopt(m_replySocket, SOL_SOCKET, SO_SNDBUF, (char*)&send_buffer, send_buffer_sizeof);

    //int flag = 1;
    //int result = setsockopt(m_replySocket,IPPROTO_TCP,TCP_NODELAY,(char *)&flag,sizeof(int));

    u_long iMode = 0;//0 - blocking, 1 - non blocking
    ioctlsocket(m_replySocket, FIONBIO, &iMode);

    return (int)m_replySocket;
}

int EasySocket::bind(uint16_t portno)
{
    if (m_socket < 0) return -1;
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    auto res = ::bind(m_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (res == SOCKET_ERROR)
    {
        printf("bind failed with error %u\n", WSAGetLastError());
        return -1;
    }
    return res;
}


#endif
