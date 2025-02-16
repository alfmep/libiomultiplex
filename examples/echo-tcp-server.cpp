/*
 * Copyright (C) 2022,2025 Dan Arrhenius <dan@ultramarin.se>
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
// Simple example of an echo server using TCP/IP
//

using namespace std;
namespace iom = iomultiplex;


static constexpr const char* local_address = "127.0.0.1"; // For IPv6, use "::1"
static constexpr const uint16_t local_port = 42000;
static constexpr const unsigned default_timeout = 60000; // 1 minute

//
// A memory buffer pool used by client connections
//
iom::buffer_pool buffer_pool (2048, 4, 4);


static void on_accept (iom::socket_connection& srv_sock,
                       std::shared_ptr<iom::socket_connection> client_sock,
                       int errnum);
static void on_rx (std::shared_ptr<iom::socket_connection> sock,
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
    iom::socket_connection srv_sock (ioh);

    // Local IP address we're going to listen on
    //
    iom::ip_addr addr (local_address, local_port);

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

    // Run the I/O handler without a worker thread
    //
    ioh.run ();

    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_accept (iom::socket_connection& srv_sock,
                       std::shared_ptr<iom::socket_connection> client_sock,
                       int errnum)
{
    if (errnum) {
        if (errnum != ECANCELED)
            cerr << "Error accepting new clients: " << strerror(errnum) << endl;
        // Stop the I/O handler
        srv_sock.io_handler().stop ();
        return;
    }

    cout << "Got new connection from " << client_sock->peer().to_string() << endl;

    // Queue a read operation from the client socket
    // Read result is handled in function 'on_rx'
    //
    auto* buf = buffer_pool.get ();
    if (client_sock->read(buf,
                          buffer_pool.buf_size(),
                          [client_sock](iom::io_result_t& ior)->bool{
                              on_rx (client_sock, ior);
                              return false;
                          },
                          default_timeout))
    {
        // Unable to queue read request (unlikely error)
        cerr << "Error queueing a read request: " << strerror(errno) << endl;
        buffer_pool.put (buf);
        client_sock->close ();
    }

    // Continue accpeting new clients
    //
    srv_sock.accept (on_accept);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_rx (std::shared_ptr<iom::socket_connection> sock,
                   iom::io_result_t& ior)
{
    // Check for error or closed connection
    //
    if (ior.result <= 0) {
        switch (ior.errnum) {
        case 0:
            cerr << "Connection closed by peer: " << sock->peer().to_string() << endl;
        case ECANCELED:
            break;
        case ETIMEDOUT:
            cerr << "Timeout, closing peer " << sock->peer().to_string() << endl;
            break;
        default:
            cerr << "Rx error from " << sock->peer().to_string() << ": " << strerror(ior.errnum) << endl;
            break;
        }
        sock->close ();
        buffer_pool.put (ior.buf); // Free rx buffer
        return;
    }

    // Queue a write operation, send back what we got
    //
    if (sock->write(ior.buf,
                    ior.result,
                    [](iom::io_result_t& ior)->bool{
                        // Ignore TX result, but free buffer
                        buffer_pool.put (ior.buf);
                        return false;
                    },
                    default_timeout))
    {
        // Failed to queue a TX operation (unlikely error)
        cerr << "Error queueing a write request: " << strerror(errno) << endl;
        buffer_pool.put (ior.buf);
    }

    // Queue a new read operation
    //
    auto* buf = buffer_pool.get ();
    if (sock->read(buf,
                   buffer_pool.buf_size(),
                   [sock](iom::io_result_t& ior)->bool{
                       on_rx (sock, ior);
                       return false;
                   },
                   default_timeout))
    {
        // Failed to queue a RX operation (unlikely error)
        cerr << "Error queueing a read request: " << strerror(errno) << endl;
        sock->close ();
        buffer_pool.put (buf);
    }
}
