/*
MIT License

Copyright (c) 2026 Coreware Limited

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once

#ifdef __linux__
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#elif _WIN32
#pragma comment(lib,"Ws2_32.lib")
// Include order matters in Windows // VOLTRON_EXCLUDE
// If using Socket class on Windows, ws2tcpip.h should be included before any windows.h inclusion in the entire project or do "#define WIN32_LEAN_AND_MEAN" // VOLTRON_EXCLUDE
#include <Ws2tcpip.h>
#include <Windows.h>
#pragma warning(disable:4996)
#endif

#include <atomic>
#include <string>
#include <string_view>
#include <cstring>
#include <cstddef>
#include <cassert>
#include <limits>
#include <new>

#include "../compiler/builtin_functions.h"

namespace llfix
{

enum class SocketOptionLevel
{
    SOCKET,
    TCP,
    IP
};

enum class SocketOption
{
    GET_ERROR_AND_CLEAR,
    REUSE_ADDRESS,
    REUSE_PORT,
    KEEP_ALIVE,
    EXCLUSIVE_ADDRESS,
    RECEIVE_BUFFER_SIZE,
    RECEIVE_BUFFER_TIMEOUT,
    SEND_BUFFER_SIZE,
    SEND_BUFFER_TIMEOUT,
    TCP_ENABLE_CORK,
    TCP_ENABLE_QUICKACK,        // Applies only to Linux , even Nagle is turned off , delayed can cause time loss due in case of lost packages
    TCP_DISABLE_NAGLE,          // Send packets as soon as possible , no need to wait for ACKs or to reach a certain amount of buffer size
    BUSY_POLL_MICROSECONDS,     // SO_BUSY_POLL , specifies time to wait for async io to query kernel to know if new data received
    SOCKET_PRIORITY,
    TIME_TO_LIVE,
    ZERO_COPY,                  // https://www.kernel.org/doc/html/v4.15/networking/msg_zerocopy.html
};

enum class SocketType
{
    TCP,
    UDP
};

enum class SocketState
{
    DISCONNECTED,
    BOUND,
    CONNECTED,
    LISTENING,
    ACCEPTED
};

class SocketAddress
{
    public:

        void initialise(const std::string_view& address, int port)
        {
            m_port = port;
            m_address = address;

            auto addr = &m_socket_address_struct;

            llfix_builtin_memset(addr, 0, sizeof(sockaddr_in));
            addr->sin_family = PF_INET;
            addr->sin_port = htons(port);

            if (m_address.size() > 0)
            {
                if (get_address_info(m_address.c_str(), &(addr->sin_addr)) != 0)
                {
                    inet_pton(PF_INET, m_address.c_str(), &(addr->sin_addr));
                }
            }
            else
            {
                addr->sin_addr.s_addr = INADDR_ANY;
            }
        }

        void initialise(struct sockaddr_in* socket_address_struct)
        {
            char ip[64];
            #ifdef __linux__
            inet_ntop(PF_INET, (struct in_addr*)&(socket_address_struct->sin_addr.s_addr), ip, sizeof(ip) - 1);
            #elif _WIN32
            InetNtopA(PF_INET, (struct in_addr*)&(socket_address_struct->sin_addr.s_addr), ip, sizeof(ip) - 1);
            #endif
            m_address = ip;
            m_port = ntohs(socket_address_struct->sin_port);
        }

        const std::string& get_address() const
        {
            return m_address;
        }

        int get_port() const
        {
            return m_port;
        }

        struct sockaddr_in* get_socket_address_struct()
        {
            return &m_socket_address_struct;
        }

    private:
        int m_port = 0;
        std::string m_address;
        struct sockaddr_in m_socket_address_struct;

        static int get_address_info(const char* hostname, struct in_addr* socket_address)
        {
            struct addrinfo hints{};
            hints.ai_family = AF_INET;

            struct addrinfo *res{ nullptr };

            int result = getaddrinfo(hostname, nullptr, &hints, &res);

            if (result == 0)
            {
                llfix_builtin_memcpy(socket_address, &((struct sockaddr_in *) res->ai_addr)->sin_addr, sizeof(struct in_addr));
                freeaddrinfo(res);
            }

            return result;
        }
};

#ifdef __linux__
using SocketFDType = int;
#elif defined(_WIN32)
using SocketFDType = SOCKET;
#endif

template <SocketType socket_type = SocketType::TCP>
class Socket
{
    public :

        #ifdef __linux__
        constexpr static inline SocketFDType INVALID_SOCKET_DESCRIPTOR = -1;
        #elif defined(_WIN32)
        constexpr static inline SocketFDType INVALID_SOCKET_DESCRIPTOR = INVALID_SOCKET;
        #endif

        static inline std::atomic<bool> socket_library_initialised = false;

        // 2 static methods needs to be called at the beginning and end of a program
        static void socket_library_initialise()
        {
            if (socket_library_initialised.load() == false)
            {
                #ifdef _WIN32
                WORD version = MAKEWORD(2, 2);
                WSADATA data;
                WSAStartup(version, &data);
                #endif
                socket_library_initialised.store(true);
            }
        }

        static void socket_library_uninitialise()
        {
            if (socket_library_initialised.load() == true)
            {
                #ifdef _WIN32
                WSACleanup();
                #endif
                socket_library_initialised.store(false);
            }
        }

        Socket()
        {
            m_socket_descriptor = INVALID_SOCKET_DESCRIPTOR;
            m_state = SocketState::DISCONNECTED;
            m_pending_connections_queue_size = 0;
            socket_library_initialise();
        }

        ~Socket()
        {
            close();
        }

        bool create()
        {
            if constexpr(socket_type == SocketType::TCP)
            {
                m_socket_descriptor = socket(PF_INET, SOCK_STREAM, 0);
            }
            else if constexpr (socket_type == SocketType::UDP)
            {
                m_socket_descriptor = socket(PF_INET, SOCK_DGRAM, 0);
            }

            if (m_socket_descriptor == INVALID_SOCKET_DESCRIPTOR)
            {
                return false;
            }

            return true;
        }

        void close()
        {
            if (m_socket_descriptor != INVALID_SOCKET_DESCRIPTOR)
            {
                #ifdef __linux__
                ::close(m_socket_descriptor);
                #elif _WIN32
                ::closesocket(m_socket_descriptor);
                #endif
            }

            m_socket_descriptor = INVALID_SOCKET_DESCRIPTOR;
            m_state = SocketState::DISCONNECTED;
        }

        // For acceptors :  it is listening address and port
        // For connectors : it is the NIC that the application wants to use for outgoing connection.
        //                  in connector case port can be specified as 0
        bool bind(const std::string_view& address, int port)
        {
            m_bind_address.initialise(address, port);

            int result = ::bind(m_socket_descriptor, (struct sockaddr*)m_bind_address.get_socket_address_struct(), sizeof(struct sockaddr_in));

            if (result != 0)
            {
                return false;
            }

            m_state = SocketState::BOUND;

            return true;
        }

        bool connect(const std::string_view& address, int port)
        {
            set_endpoint(address, port);

            auto ret = ::connect(m_socket_descriptor, (struct sockaddr*)m_endpoint_address.get_socket_address_struct(), sizeof(struct sockaddr_in));

            if (ret != 0)
            {
                return false;
            }

            m_state = SocketState::CONNECTED;
            return true;
        }

        Socket* accept(int timeout_seconds)
        {
            if (m_state != SocketState::LISTENING && m_state != SocketState::ACCEPTED)
            {
                return nullptr;
            }

            auto blocking_mode_before_the_call = is_in_blocking_mode();
            set_blocking_mode(false);

            bool success{ true };
            ///////////////////////////////////////////////////
            fd_set read_set;
            FD_ZERO(&read_set);
            FD_SET(m_socket_descriptor, &read_set);

            struct timeval tv;
            tv.tv_sec = timeout_seconds;
            tv.tv_usec = 0;

            #ifdef __linux__
            success = (::select(m_socket_descriptor + 1, &read_set, nullptr, nullptr, &tv) <= 0) ? false : true;
            #elif _WIN32
            success = (::select(0, &read_set, nullptr, nullptr, &tv) <= 0) ? false : true;
            #endif
            ///////////////////////////////////////////////////
            SocketFDType connector_socket_desc = INVALID_SOCKET_DESCRIPTOR;

            struct sockaddr_in address;
            socklen_t len = sizeof(address);
            llfix_builtin_memset(&address, 0, sizeof(address));

            if (success)
            {
                connector_socket_desc = ::accept(m_socket_descriptor, (struct sockaddr*)&address, &len);

                if (connector_socket_desc == INVALID_SOCKET_DESCRIPTOR)
                {
                    success = false;
                }
            }

            set_blocking_mode(blocking_mode_before_the_call);

            if (!success)
            {
                return nullptr;
            }

            // It is caller`s responsibility to delete it
            Socket* connector_socket{ nullptr };
            connector_socket = new (std::nothrow) Socket;

            if(connector_socket == nullptr)
            {
                return nullptr;
            }

            connector_socket->initialise(connector_socket_desc, &address);
            connector_socket->m_state = SocketState::CONNECTED;

            m_state = SocketState::ACCEPTED;

            return connector_socket;
        }

        void set_pending_connections_queue_size(int value)
        {
            m_pending_connections_queue_size = value;
        }

        void set_blocking_mode(bool blocking_mode)
        {
            #if __linux__
            long arg = fcntl(m_socket_descriptor, F_GETFL, NULL);

            if (blocking_mode)
            {
                arg &= (~O_NONBLOCK);
            }
            else
            {
                arg |= O_NONBLOCK;
            }

            fcntl(m_socket_descriptor, F_SETFL, arg);
            #elif _WIN32
            u_long mode{ 0 };

            if (!blocking_mode)
            {
                mode = 1;
            }

            ioctlsocket(m_socket_descriptor, FIONBIO, &mode);
            #endif
            m_in_blocking_mode = blocking_mode;
        }

        bool set_socket_option(SocketOptionLevel level, SocketOption option, int value)
        {
            int actual_option = get_socket_option_value(option);

            if (actual_option == -1)
            {
                // Even though called ,not supported on this system, for ex QUICK_ACK for Windows
                return false;
            }

            int actual_level = get_socket_option_level_value(level);

            if (actual_level == -1)
            {
                return false;
            }

            int actual_value = value;
            int ret = -1;
            #ifdef __linux__
            ret = setsockopt(m_socket_descriptor, actual_level, actual_option, &actual_value, sizeof(actual_value));
            #elif _WIN32
            ret = setsockopt(m_socket_descriptor, actual_level, actual_option, (char*)&actual_value, sizeof(actual_value));
            #endif

            return (ret == 0) ? true : false;
        }

        bool listen()
        {
            int result = ::listen(m_socket_descriptor, m_pending_connections_queue_size);

            if (result != 0)
            {
                return false;
            }

            m_state = SocketState::LISTENING;
            return true;
        }

        int get_socket_option(SocketOptionLevel level, SocketOption option)
        {
            int actual_level = get_socket_option_level_value(level);
            int actual_option = get_socket_option_value(option);

            int ret{ 0 };
            socklen_t len = sizeof(ret);

            #ifdef __linux__
            getsockopt(m_socket_descriptor, actual_level, actual_option, (void*)(&ret), &len);
            #elif _WIN32
            getsockopt(m_socket_descriptor, actual_level, actual_option, (char*)(&ret), &len);
            #endif

            return ret;
        }

        static int get_current_thread_last_socket_error()
        {
            int ret{ -1 };
            #ifdef __linux__
            ret = errno;
            #elif _WIN32
            ret = WSAGetLastError();
            #endif
            return ret;
        }

        static std::string get_socket_error_as_string(int error_code)
        {
            std::string ret;
            #ifdef __linux__
            ret = strerror(error_code);
            #elif _WIN32
            char* temp_string = nullptr;
            HMODULE lib = ::LoadLibraryA("WSock32.dll");

            if(lib)
            {
                FormatMessageA(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
                    (LPCVOID)lib, error_code,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPSTR)&temp_string, 0, nullptr);

                if (temp_string)
                {
                    ret = temp_string;
                    LocalFree(temp_string);
                }

                ::FreeLibrary(lib);
            }
            #endif
            return ret;
        }

        SocketState get_state() const
        {
            return m_state;
        }

        SocketFDType get_socket_descriptor() const
        {
            return m_socket_descriptor;
        }

        int receive(char* buffer, std::size_t len)
        {
            assert(len <= static_cast<std::size_t>((std::numeric_limits<int>::max)()));
            return ::recv(m_socket_descriptor, buffer, static_cast<int>(len), static_cast<int>(0));
        }

        int send(const char* buffer, std::size_t len)
        {
            assert(len <= static_cast<std::size_t>((std::numeric_limits<int>::max)()));
            #ifdef __linux__
            /*
                BY DEFAULT , LINUX APPS GET SIGPIPE SIGNALS WHEN THEY WRITE TO WRITE/SEND
                ON CLOSED SOCKETS WHICH MAKES IT IMPOSSIBLE TO DETECT CONNECTION LOSS DURING SENDS.
                BY IGNORING THE SIGNAL , CONNECTION LOSS DETECTION CAN BE DONE INSIDE THE CALLER APP
            */
            return ::send(m_socket_descriptor, buffer, static_cast<int>(len), MSG_NOSIGNAL);
            #elif _WIN32
            return ::send(m_socket_descriptor, buffer, static_cast<int>(len), static_cast<int>(0));
            #endif
        }

        int send_zero_copy(const char* buffer, std::size_t len)
        {
            assert(len <= static_cast<std::size_t>((std::numeric_limits<int>::max)()));
            #ifdef MSG_ZEROCOPY
            return ::send(m_socket_descriptor, buffer, static_cast<int>(len), MSG_ZEROCOPY);
            #else
            return ::send(m_socket_descriptor, buffer, static_cast<int>(len), static_cast<int>(0));
            #endif
        }

        void set_endpoint(const std::string_view& address , int port)
        {
            m_endpoint_address.initialise(address, port);
        }

        bool is_in_blocking_mode() const { return m_in_blocking_mode; }

        /*
            Note for non-blocking/asycn-io sockets : recv will result with return code 0
            therefore you also need to check recv result
        */
        bool is_connection_lost_during_receive(int error_code)
        {
            bool ret{ false };

            #ifdef __linux__
            switch (error_code)
            {
                case ECONNRESET:
                    ret = true;
                    m_state = SocketState::DISCONNECTED;
                    break;
                case ECONNREFUSED:
                    ret = true;
                    m_state = SocketState::DISCONNECTED;
                    break;
                case ENOTCONN:
                    ret = true;
                    m_state = SocketState::DISCONNECTED;
                    break;
                default:
                    break;
            }
            #elif _WIN32
            switch (error_code)
            {
                case WSAENOTCONN:
                    ret = true;
                    m_state = SocketState::DISCONNECTED;
                    break;
                case WSAENETRESET:
                    ret = true;
                    m_state = SocketState::DISCONNECTED;
                    break;
                case WSAESHUTDOWN:
                    ret = true;
                    m_state = SocketState::DISCONNECTED;
                    break;
                case WSAECONNABORTED:
                    ret = true;
                    m_state = SocketState::DISCONNECTED;
                    break;
                case WSAECONNRESET:
                    ret = true;
                    m_state = SocketState::DISCONNECTED;
                    break;
                default:
                    break;
            }
            #endif

            return ret;
        }

        bool is_connection_lost_during_send(int error_code)
        {
            bool ret{ false };

            #ifdef __linux__
            switch (error_code)
            {
                case ECONNRESET:
                    ret = true;
                    m_state = SocketState::DISCONNECTED;
                    break;
                case ENOTCONN:
                    ret = true;
                    m_state = SocketState::DISCONNECTED;
                    break;
                case EPIPE:
                    ret = true;
                    m_state = SocketState::DISCONNECTED;
                    break;
                default:
                    break;
            }
            #elif _WIN32
            switch (error_code)
            {
                case WSAENOTCONN:
                    ret = true;
                    m_state = SocketState::DISCONNECTED;
                    break;
                case WSAENETRESET:
                    ret = true;
                    m_state = SocketState::DISCONNECTED;
                    break;
                case WSAESHUTDOWN:
                    ret = true;
                    m_state = SocketState::DISCONNECTED;
                    break;
                case WSAECONNABORTED:
                    ret = true;
                    m_state = SocketState::DISCONNECTED;
                    break;
                case WSAECONNRESET:
                    ret = true;
                    m_state = SocketState::DISCONNECTED;
                    break;
                default:
                    break;
            }
            #endif

            return ret;
        }

        static bool get_peer_details(SocketFDType socket_fd, std::string& address, int& port)
        {
            sockaddr_storage addr;
            socklen_t addr_len = sizeof(addr);

            if (getpeername(socket_fd, reinterpret_cast<sockaddr*>(&addr), &addr_len) != 0)
            {
                return false;
            }

            char host[NI_MAXHOST];
            char serv[NI_MAXSERV];

            int rc = getnameinfo(reinterpret_cast<sockaddr*>(&addr), addr_len, host, sizeof(host), serv, sizeof(serv), NI_NUMERICHOST | NI_NUMERICSERV);

            if (rc != 0)
            {
                return false;
            }

            address = host;

            try
            {
                port = std::stoi(serv);
            }
            catch(...)
            {
                return false;
            }

            return true;
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // UDP Only
        bool join_multicast_group(const std::string_view& multicast_address)
        {
            static_assert(socket_type == SocketType::UDP, "join_multicast_group can be called only on UDP sockets");

            struct ip_mreq mreq;
            mreq.imr_multiaddr.s_addr = inet_addr(multicast_address.data());
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);

            #ifdef __linux__
            if (setsockopt(m_socket_descriptor, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
            #elif _WIN32
            if (setsockopt(m_socket_descriptor, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq)) < 0)
            #endif
            {
                return false;
            }

            return true;
        }

        bool leave_multicast_group(const std::string_view& multicast_address)
        {
            static_assert(socket_type == SocketType::UDP, "leave_multicast_group can be called only on UDP sockets");

            struct ip_mreq mreq;
            mreq.imr_multiaddr.s_addr = inet_addr(multicast_address.data());
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);

            #ifdef __linux__
            if (setsockopt(m_socket_descriptor, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
            #elif _WIN32
            if (setsockopt(m_socket_descriptor, IPPROTO_IP, IP_DROP_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq)) < 0)
            #endif
            {
                return false;
            }

            return true;
        }

        int receive_from(char* buffer, std::size_t len)
        {
            static_assert(socket_type == SocketType::UDP, "receive_from can be called only on UDP sockets");
            assert(len <= static_cast<std::size_t>((std::numeric_limits<int>::max)()));
            return ::recvfrom(m_socket_descriptor, buffer, static_cast<int>(len), 0, nullptr, nullptr);
        }

        int send_to(const char* buffer, std::size_t len)
        {
            static_assert(socket_type == SocketType::UDP, "send_to can be called only on UDP sockets");
            assert(len <= static_cast<std::size_t>((std::numeric_limits<int>::max)()));
            return ::sendto(m_socket_descriptor, buffer, static_cast<int>(len), 0, (struct sockaddr*)m_endpoint_address.get_socket_address_struct(), sizeof(struct sockaddr_in)) ;
        }
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

private:
    SocketFDType m_socket_descriptor;
    SocketState m_state;
    bool m_in_blocking_mode = true;
    int m_pending_connections_queue_size;

    SocketAddress m_bind_address;        // TCP Acceptors -> listening address  , TCP Connectors -> NIC address , UDP Multicast listeners -> NIC address
    SocketAddress m_endpoint_address;    // Used by only TCP connectors & UDP multicast publishers

    Socket(const Socket& other) = delete;
    Socket& operator= (const Socket& other) = delete;
    Socket(Socket&& other) = delete;
    Socket& operator=(Socket&& other) = delete;

    void initialise(SocketFDType socket_descriptor, struct sockaddr_in* socket_address)
    {
        m_socket_descriptor = socket_descriptor;
        m_endpoint_address.initialise(socket_address);
    }

    int get_socket_option_level_value(SocketOptionLevel level)
    {
        int ret{ -1 };

        switch (level)
        {
            case SocketOptionLevel::SOCKET:
                ret = SOL_SOCKET;
                break;
            case SocketOptionLevel::TCP:
                ret = IPPROTO_TCP;
                break;
            case SocketOptionLevel::IP:
                ret = IPPROTO_IP;
                break;
            default:
                break;
        }

        return ret;
    }

    int get_socket_option_value(SocketOption option)
    {
        int ret{ -1 };

        switch (option)
        {
            case SocketOption::GET_ERROR_AND_CLEAR:
                ret = SO_ERROR;
                break;
            case SocketOption::REUSE_ADDRESS:
                ret = SO_REUSEADDR;
                break;
            case SocketOption::KEEP_ALIVE:
                ret = SO_KEEPALIVE;
                break;
            case SocketOption::EXCLUSIVE_ADDRESS:
                ret = SO_REUSEADDR;
                break;
            case SocketOption::REUSE_PORT:
                #ifdef SO_REUSEPORT
                ret = SO_REUSEPORT;
                #endif
                break;
            case SocketOption::RECEIVE_BUFFER_SIZE:
                ret = SO_RCVBUF;
                break;
            case SocketOption::RECEIVE_BUFFER_TIMEOUT:
                ret = SO_RCVTIMEO;
                break;
            case SocketOption::SEND_BUFFER_SIZE:
                ret = SO_SNDBUF;
                break;
            case SocketOption::SEND_BUFFER_TIMEOUT:
                ret = SO_SNDTIMEO;
                break;
            case SocketOption::TCP_DISABLE_NAGLE:
                ret = TCP_NODELAY;
                break;
            case SocketOption::TCP_ENABLE_QUICKACK:
                #ifdef TCP_QUICKACK
                ret = TCP_QUICKACK;
                #endif
                break;
            case SocketOption::TCP_ENABLE_CORK:
                #ifdef TCP_CORK
                ret = TCP_CORK;
                #endif
                break;
            case SocketOption::SOCKET_PRIORITY:
                #ifdef SO_PRIORITY
                ret = SO_PRIORITY;
                #endif
                break;
            case SocketOption::BUSY_POLL_MICROSECONDS:
                #ifdef SO_BUSY_POLL
                ret = SO_BUSY_POLL;
                #endif
                break;
            case SocketOption::TIME_TO_LIVE:
                #ifdef IP_TTL
                ret = IP_TTL;
                #endif
                break;
            case SocketOption::ZERO_COPY:
                #ifdef SO_ZEROCOPY
                ret = SO_ZEROCOPY;
                #endif
                break;
            default:
                break;
        }

        return ret;
    }
};

} // namespace