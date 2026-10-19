/*
 * The LAN code's sockets (lan.cpp, lan_libnet.cpp, lan_lobby.cpp): Winsock on
 * Windows; elsewhere (the Mac, the iPad, Linux) the BSD sockets Winsock was
 * made after, under Winsock's names - SOCKET, closesocket, WSAGetLastError -
 * so one text serves both.
 */

#ifndef BURNOUT3_LAN_SOCKETS_H
#define BURNOUT3_LAN_SOCKETS_H

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

using SockLen = int;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using SOCKET = int;
using SockLen = socklen_t;
constexpr SOCKET INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
constexpr int WSAEWOULDBLOCK = EWOULDBLOCK;
constexpr int WSAEINPROGRESS = EINPROGRESS;
constexpr int WSAEALREADY = EALREADY;
constexpr int WSAECONNRESET = ECONNRESET;
#ifndef MAKEWORD
#define MAKEWORD(low, high) ((unsigned short)(((low) & 0xFF) | (((high) & 0xFF) << 8)))
#endif
struct WSADATA {};
inline int WSAStartup(unsigned short, WSADATA*) { return 0; }
inline int WSAGetLastError() { return errno; }
inline int closesocket(SOCKET s) { return close(s); }
inline int ioctlsocket(SOCKET s, unsigned long command, unsigned long* value) {
    int on = int(*value);
    return ioctl(s, command, &on);
}
#endif

// select()'s first argument: ignored by Winsock, one past the highest
// descriptor elsewhere.
inline int SelectWidth(SOCKET highest) { return int(highest) + 1; }

#endif
