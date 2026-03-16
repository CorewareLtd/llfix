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

#include <cstddef>
#include <string>

namespace llfix
{

struct TCPConnectorOptions
{
    int m_socket_rx_size = 212992;
    int m_socket_tx_size = 212992;
    int m_nic_ringbuffer_tx_size = 2048;
    int m_nic_ringbuffer_rx_size = 4096;
    bool m_enable_quick_ack = false;
    bool m_disable_nagle = true;
    int m_receive_size=4096;
    int m_busy_poll_microseconds = 0;
    long m_async_io_timeout_nanoseconds = 1000;
    std::string m_nic_interface_name;
    std::string m_nic_interface_ip;
    std::size_t m_rx_buffer_capacity = 212992;
    int m_send_try_count = 0;
    // OTHERS
    int m_spin_count = 1;
    void* m_stack = nullptr;
    int m_connect_timeout_seconds = 3;
    // SSL
    #ifdef LLFIX_ENABLE_OPENSSL
    bool m_use_ssl = false;
    bool m_ssl_verify_peer = true;
    std::string m_ssl_cert_pem_file;
    std::string m_ssl_private_key_pem_file;
    std::string m_ssl_private_key_password;
    std::string m_ssl_ca_pem_file;
    std::string m_ssl_version = "TLS12";
    std::string m_ssl_cipher_suite;
    #endif
};

} // namespace