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
#include <iomultiplex.hpp>

//
// Simple example of a client using TCP/IP
//

using namespace std;
namespace iom = iomultiplex;


static constexpr const char* server_address = "127.0.0.1"; // For IPv6, use "::1"
static constexpr const uint16_t server_port = 42000;
static constexpr const unsigned default_timeout = 60000; // 1 minute


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    // Create and start an I/O handler instance
    //
    iom::default_iohandler ioh;
    ioh.run (true); // The I/O handler is run in a workder thread

    // Create the client socket
    //
    iom::SocketConnection sock (ioh);

    // Address of the server we're connecting to
    //
    iom::IpAddr addr (server_address, server_port);

    // Open the socket
    //
    if (sock.open(addr.family(), SOCK_STREAM)) {
        perror ("sock.open");
        return 1;
    }

    // Connect to the server
    //
    if (sock.connect(addr, default_timeout)) {
        perror ("sock.connect");
        return 1;
    }

    cout << "Connected to " << sock.peer().to_string() << endl;
    cout << "Enter some text to send (and receive), an empty line closes the connection" << endl;

    while (true) {
        // Read text from standard input
        //
        std::string text;
        getline (cin, text);
        if (!text.size())
            break;

        // Send the text to the server
        //
        if (sock.write(text.c_str(), text.size()) < 0) {
            cerr << "Error sending text: " << strerror(errno) << endl;
            break;
        }

        // Receive text from the server
        //
        char buf[4096];
        auto result = sock.read (buf, sizeof(buf));
        if (result <= 0) {
            if (result == 0)
                cerr << "Connection closed by peer" << endl;
            else
                perror ("sock.read");
            break;
        }

        // Write the received text to standard output
        //
        cout.write (buf, result);
        cout << endl;
    }

    sock.close ();
    ioh.stop ();

    return 0;
}
