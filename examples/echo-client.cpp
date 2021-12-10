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
#include <string>


namespace iom=iomultiplex;
using namespace std;


//
// Very simple example of sending and receiving data using a UDP socket
//


// Remote port number
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

    // "Connect" to our echo server.
    // Since is is a UDP socket nothing is done but
    // setting the remote address for sending and
    // receiving data.
    //
    if (sock.connect(iom::IpAddr(127,0,0,1, echo_port))) {
        perror ("sock.connect");
        return 1;
    }

    cout << "Remote address: " << sock.peer().to_string() << endl;
    cout << "Enter some text to send: " << flush;
    std::string text;
    getline (cin, text);

    // Send some data
    //
    cout << "Sending text to " << sock.peer().to_string() << endl;
    if (sock.write(text.c_str(), text.size()) < 0) {
        perror ("sock.write");
        return 1;
    }

    cout << "Wait for result..." << flush;

    // Read some data
    //
    char buf[4096];
    auto result = sock.read (buf, sizeof(buf));
    if (result < 0) {
        cerr << endl;
        perror ("sock.read");
        return 1;
    }
    buf[sizeof(buf)-1] = '\0'; // Just making sure the buffer is null-terminated

    cout << " got back " << result << " bytes of data:" << endl;
    cout << '"' << buf << '"' << endl;

    // No need to close the socket here, it is closed in its destructor
    // No need to stop the I/O handler, it is nicely shut down in its destructor

    return 0;
}
