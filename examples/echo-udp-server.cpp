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
// Simple example of an echo server using UDP/IP
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


static void on_rx (iom::socket_connection& sock,
                   iom::io_result_t& ior,
                   const iom::sock_addr& peer_addr);



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    // Create an I/O handler instance
    //
    iom::default_iohandler ioh;

    // Create the server socket
    //
    iom::socket_connection sock (ioh);

    // Local IP address we're going to listen on
    //
    iom::ip_addr addr (local_address, local_port);

    // Open the server socket
    //
    if (sock.open(addr.family(), SOCK_DGRAM)) {
        perror ("sock.open");
        return 1;
    }

    // Bind to our local address
    //
    if (sock.bind(addr)) {
        perror ("sock.bind");
        return 1;
    }

    // Queue a recvfrom operation
    //
    auto* buf = buffer_pool.get ();
    if (sock.recvfrom(buf, buffer_pool.buf_size(), on_rx)) {
        // Failed to queue a recvfrom operation (unlikely error)
        perror ("sock.recvfrom");
        return 1;
    }

    cout << "Accepting UDP clients on " << sock.addr().to_string() << endl;

    // Run the I/O handler
    //
    ioh.run ();

    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_rx (iom::socket_connection& sock,
                   iom::io_result_t& ior,
                   const iom::sock_addr& peer_addr)
{
    if (ior.errnum) {
        // Check for error
        //
        cerr << "RX error: " << strerror(ior.errnum) << endl;
        buffer_pool.put (ior.buf); // Free rx buffer
    }else{
        cout << "Got " << ior.result << " bytes from " << peer_addr.to_string() << endl;
        //
        // Queue a write operation, send back what we got
        //
        if (sock.sendto(ior.buf,
                        ior.result,
                        peer_addr,
                        [](iom::socket_connection& sock, iom::io_result_t& ior, const iom::sock_addr& peer_addr){
                            // Ignore TX result, but free buffer
                            buffer_pool.put (ior.buf);
                        },
                        default_timeout))
        {
            // Failed to queue a TX operation (unlikely error)
            buffer_pool.put (ior.buf);
        }
    }

    // Queue a new recvfrom operation
    //
    auto* buf = buffer_pool.get ();
    if (sock.recvfrom(buf, buffer_pool.buf_size(), on_rx)) {
        // Failed to queue a recvfrom operation (unlikely error)
        perror ("sock.recvfrom");
        buffer_pool.put (ior.buf);
        sock.close ();
        ior.conn.io_handler().stop ();
    }
}
