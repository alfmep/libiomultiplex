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
// Simple example of an echo server using TLS over TCP/IP
// that handles multiple concurrent clients
//

using namespace std;
namespace iom = iomultiplex;


static constexpr const char* local_address = "127.0.0.1"; // For IPv6, use "::1"
static constexpr const uint16_t local_port = 42001;
static constexpr const unsigned default_timeout = 60000; // 1 minute

static constexpr const char* tls_cert_file = "tls-snakeoil.cert";
static constexpr const char* tls_key_file  = "tls-snakeoil.privkey";

//
// A memory buffer pool used by client connections
//
iom::BufferPool buffer_pool (2048, 4, 4);


static void on_accept (iom::SocketConnection& srv_sock,
                       std::shared_ptr<iom::SocketConnection> client_sock,
                       int errnum);
static void on_tls_handshake (shared_ptr<iom::TlsAdapter> tls,
                              int errnum,
                              const std::string errstr);
static void on_rx (shared_ptr<iom::TlsAdapter>,
                   iom::io_result_t& ior);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main (int argc, char* argv[])
{
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
    if (srv_sock.open(addr.family(), SOCK_STREAM)) {
        perror ("srv_sock.open");
        return 1;
    }

    // Set socket options
    //
    if (srv_sock.setsockopt(SO_REUSEADDR, 1)) {
        perror ("srv_sock.setsockopt");
        return 1;
    }

    // Bind to our local address
    //
    if (srv_sock.bind(addr)) {
        perror ("srv_sock.bind");
        return 1;
    }

    // Set the socket in listening state
    //
    if (srv_sock.listen()) {
        perror ("srv_sock.listen");
        return 1;
    }

    // Start accepting clients (when the I/O handler starts)
    // New clients are handled in function 'on_accept'
    //
    if (srv_sock.accept(on_accept)) {
        perror ("srv_sock.accept");
        return 1;
    }

    cout << "Accepting clients on " << srv_sock.addr().to_string() << endl;

    // Run the I/O handler
    //
    ioh.run ();

    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_accept (iom::SocketConnection& srv_sock,
                       std::shared_ptr<iom::SocketConnection> client_sock,
                       int errnum)
{
    if (errnum) {
        if (errnum != ECANCELED)
            cerr << "Error accepting new clients: " << strerror(errnum) << endl;
        // Stop the I/O handler (this will end the application)
        srv_sock.io_handler().stop ();
        return;
    }

    cout << "Got new connection from " << client_sock->peer().to_string() << endl;

    // Create a TLS adapter object
    //
    auto tls = make_shared<iom::TlsAdapter> (client_sock);

    // Queue a TLS handlshake request
    //
    iom::TlsConfig tls_cfg (false);
    tls_cfg.cert_file = tls_cert_file;
    tls_cfg.privkey_file = tls_key_file;

    if (tls->start_server_tls(tls_cfg,
                              [tls](iom::TlsAdapter& conn, int errnum, const std::string& errstr){
                                  // We have captured 'tls' so it doesn't go out of scope
                                  on_tls_handshake (tls, errnum, errstr);
                              },
                              default_timeout))
    {
        // Unable to queue TLS handshake request (unlikely error)
        cerr << "Error queueing a TLS handshake request: " << strerror(errno) << endl;
        client_sock->close ();
    }

    // Continue accpeting new clients
    //
    srv_sock.accept (on_accept);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_tls_handshake (shared_ptr<iom::TlsAdapter> tls,
                              int errnum,
                              const std::string errstr)
{
    if (errnum) {
        auto& sock = dynamic_cast<iom::SocketConnection&> (tls->connection());
        if (errnum != ECANCELED)
            cerr << "TLS handshake error: " << errstr << " (" << errnum << ") with "
                 << sock.peer().to_string() << endl;
        tls->close ();
        return;
    }

    // Queue a read operation from the client socket
    // Read result is handled in function 'on_rx'
    //
    auto* buf = buffer_pool.get ();
    if (tls->read(buf,
                  buffer_pool.buf_size(),
                  [tls](iom::io_result_t& ior)->bool{
                      on_rx (tls, ior);
                      return false;
                  },
                  default_timeout))
    {
        // Unable to queue read request (unlikely error)
        cerr << "Error queueing a read request: " << strerror(errno) << endl;
        buffer_pool.put (buf);
        tls->close ();
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_rx (shared_ptr<iom::TlsAdapter> tls,
                   iom::io_result_t& ior)
{
    // Check for error or closed connection
    //
    if (ior.result <= 0) {
        auto& sock = dynamic_cast<iom::SocketConnection&> (tls->connection());
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
        tls->close ();
        buffer_pool.put (ior.buf); // Free rx buffer
        return;
    }

    // Queue a write operation, send back what we got
    //
    if (tls->write(ior.buf,
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
    if (tls->read(buf,
                  buffer_pool.buf_size(),
                  [tls](iom::io_result_t& ior)->bool{
                      on_rx (tls, ior);
                      return false;
                  },
                  default_timeout))
    {
        // Failed to queue a RX operation (unlikely error)
        tls->close ();
        buffer_pool.put (buf);
    }
}
