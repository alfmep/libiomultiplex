/*
 * Copyright (C) 2022 Dan Arrhenius <dan@ultramarin.se>
 *
 * This file is part of libiomultiplex
 *
 * libiomultiplex is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <iostream>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <iomultiplex.hpp>

//
// Simple example of an echo server using DTLS over UDP/IP
//

using namespace std;
namespace iom = iomultiplex;


static constexpr const char* local_address = "127.0.0.1"; // For IPv6, use "::1"
static constexpr const uint16_t local_port = 42000;
static constexpr const unsigned default_timeout = 60000; // 1 minute

static constexpr const char* tls_cert_file = "tls-snakeoil.cert";
static constexpr const char* tls_key_file  = "tls-snakeoil.privkey";

//
// A memory buffer pool used by client connections
//
iom::BufferPool buffer_pool (2048, 4, 4);


static void on_new_client (iom::SocketConnection& srv_sock,
                           iom::io_result_t& ior,
                           const iom::SockAddr& peer_addr);
static void on_dtls_handshake (shared_ptr<iom::TlsAdapter> dtls,
                               int errnum,
                               const std::string errstr);
static void on_rx (shared_ptr<iom::TlsAdapter> dtls,
                   iom::io_result_t& ior);



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void logger (unsigned prio, const char* msg)
{
    cerr << msg << endl;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    iom::Log::set_callback (logger);
    iom::Log::priority (LOG_DEBUG);

    // Create an I/O handler instance
    //
    iom::default_iohandler ioh;

    // Create the server socket
    //
    iom::SocketConnection srv_sock (ioh);

    // Local IP address we're going to listen on
    //
    iom::IpAddr addr (local_address, local_port);

    // Open the server socket
    //
    if (srv_sock.open(addr.family(), SOCK_DGRAM)) {
        perror ("srv_sock.open");
        return 1;
    }

    // Bind to our local address
    //
    if (srv_sock.bind(addr)) {
        perror ("srv_sock.bind");
        return 1;
    }

    // Queue a recvfrom operation
    //
    auto* buf = buffer_pool.get ();
    if (srv_sock.recvfrom(buf, buffer_pool.buf_size(), on_new_client)) {
        // Failed to queue a recvfrom operation (unlikely error)
        perror ("srv_sock.recvfrom");
        return 1;
    }

    cout << "Accepting DTLS clients on " << srv_sock.addr().to_string() << endl;

    // Run the I/O handler
    //
    ioh.run ();

    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_new_client (iom::SocketConnection& srv_sock,
                           iom::io_result_t& ior,
                           const iom::SockAddr& peer_addr)
{
    if (ior.errnum) {
        // Check for error
        cerr << "recvfrom error: " << strerror(ior.errnum) << endl;
        // Free rx buffer
        buffer_pool.put (ior.buf);
        // Stop the I/O handler (this will end the application)
        srv_sock.io_handler().stop ();
        return;
    }

    cout << "Got new connection from " << peer_addr.to_string() << endl;

    // Create and open a client socket
    //
    auto sock = make_shared<iom::SocketConnection> (srv_sock.io_handler());
    if (sock->open(peer_addr.family(), srv_sock.type())) {
        perror ("srv_sock.open");
    }

    // "connect" the client socket
    // Since it is an UDP socket, this call doesn't generate handshake traffic,
    // it only "locks" read and write operations to the peer address.
    //
    sock->connect (peer_addr, nullptr);

    // Tell client to start DTLS handshake
    //
    sock->write ("STARTTLS", 8, nullptr);

    // Create a TLS adapter object
    //
    auto dtls = make_shared<iom::TlsAdapter> (sock);

    // Queue a DTLS handlshake request
    //
    iom::TlsConfig tls_cfg (false);
    tls_cfg.cert_file = tls_cert_file;
    tls_cfg.privkey_file = tls_key_file;

    if (dtls->start_server_dtls(tls_cfg,
                                [dtls](iom::TlsAdapter& conn){
                                    // We have captured 'dtls' so it doesn't go out of scope
                                    on_dtls_handshake (dtls,
                                                       dtls->last_error(),
                                                       dtls->last_error_msg());
                                },
                                default_timeout))
    {
        // Unable to queue TLS handshake request (unlikely error)
        cerr << "Error queueing a TLS handshake request: " << strerror(errno) << endl;
        sock->close ();
    }

    // Continue accepting new clients on the server socket
    // queue a new recvfrom operation
    //
    if (srv_sock.recvfrom(ior.buf, buffer_pool.buf_size(), on_new_client)) {
        // Failed to queue a recvfrom operation (unlikely error)
        perror ("srv_sock.recvfrom");
        // Free rx buffer
        buffer_pool.put (ior.buf);
        // Stop the I/O handler (this will end the application)
        srv_sock.io_handler().stop ();
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_dtls_handshake (shared_ptr<iom::TlsAdapter> dtls,
                               int errnum,
                               const std::string errstr)
{
    if (errnum) {
        auto& sock = dynamic_cast<iom::SocketConnection&> (dtls->connection());
        if (errnum != ECANCELED)
            cerr << "DTLS handshake error: " << errstr << " (" << errnum << ") with "
                 << sock.peer().to_string() << endl;
        dtls->close ();
        return;
    }

    // Queue a read operation from the client socket
    // Read result is handled in function 'on_rx'
    //
    auto* buf = buffer_pool.get ();
    if (dtls->read(buf,
                   buffer_pool.buf_size(),
                   [dtls](iom::io_result_t& ior)->bool{
                       on_rx (dtls, ior);
                       return false;
                   },
                   default_timeout))
    {
        // Unable to queue read request (unlikely error)
        cerr << "Error queueing a read request: " << strerror(errno) << endl;
        buffer_pool.put (buf);
        dtls->close ();
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_rx (shared_ptr<iom::TlsAdapter> dtls,
                   iom::io_result_t& ior)
{
    // Check for error or closed connection
    //
    if (ior.result <= 0) {
        auto& sock = dynamic_cast<iom::SocketConnection&> (dtls->connection());
        switch (ior.errnum) {
        case 0:
            cerr << "Connection closed by peer " << sock.peer().to_string() << endl;
        case ECANCELED:
            break;
        case ETIMEDOUT:
            cerr << "Timeout, closing peer " << sock.peer().to_string() << endl;
            break;
        default:
            cerr << "Rx error from " << sock.peer().to_string() << ": " << strerror(ior.errnum) << endl;
            break;
        }
        dtls->close ();
        buffer_pool.put (ior.buf); // Free rx buffer
        return;
    }

    // Queue a write operation, send back what we got
    //
    if (dtls->write(ior.buf,
                    ior.result,
                    [](iom::io_result_t& ior)->bool{
                        // Ignore TX result, but free buffer
                        buffer_pool.put (ior.buf);
                        return false;
                    },
                    default_timeout))
    {
        // Failed to queue a TX operation (unlikely error)
        buffer_pool.put (ior.buf);
    }

    // Queue a new read operation
    //
    auto* buf = buffer_pool.get ();
    if (dtls->read(buf,
                   buffer_pool.buf_size(),
                   [dtls](iom::io_result_t& ior)->bool{
                       on_rx (dtls, ior);
                       return false;
                   },
                   default_timeout))
    {
        // Failed to queue a RX operation (unlikely error)
        dtls->close ();
        buffer_pool.put (buf);
    }
}
