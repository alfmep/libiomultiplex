/*
 * Copyright (C) 2021 Dan Arrhenius <dan@ultramarin.se>
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
#include <iomultiplex.hpp>
#include <iostream>
#include <cstdio>
#include <cerrno>


namespace iom=iomultiplex;
using namespace std;


//
// Very simple example of receiving and sending data using a UDP socket
//


// Local port number
static constexpr uint16_t echo_port = 42000;


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

    iom::default_iohandler ioh;
    iom::SocketConnection sock (ioh);

    // Start the I/O handler in a worker thread
    //
    ioh.run (true);

    // Open a UDP/IP socket
    //
    if (sock.open(AF_INET, SOCK_DGRAM)) {
        perror ("sock.open");
        return 1;
    }

    // Bind the socket to 127.0.0.1:echo_port
    //
    if (sock.bind(iom::IpAddr(127,0,0,1, echo_port))) {
        perror ("sock.bind");
        return 1;
    }

    // Read some data
    //
    //cout << "Wait for some data on " << sock.addr().to_string() <<  "..." << flush;
    cout << "Wait for some data on " << sock.addr().to_string() <<  "..." << endl;
    char buf[4096];
    ssize_t result = sizeof (buf);
    auto client_addr = sock.recvfrom (buf, result);
    if (result<0) {
        perror ("sock.accept");
        return 1;
    }

    //cout << " got " << result << " bytes of data from " << client_addr->to_string() << endl;
    cout << "Got " << result << " bytes of data from " << client_addr->to_string() << endl;
    cout << "Sending it back." << endl;

    // Write some data
    //
    result = sock.sendto (buf, result, *client_addr);
    if (result < 0) {
        perror ("sock.sendto");
        return 1;
    }

    cout << "All done." << endl;

    // No need to close the socket here, it is closed in its destructor
    // No need to stop the I/O handler, it is nicely shut down in its destructor

    return 0;
}
