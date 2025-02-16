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
#include <cstring>
#include <iomultiplex.hpp>

//
// Simple example of a client using DTLS over UDP/IP
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
    char buf[2048];

    // Create and start an I/O handler instance
    //
    iom::default_iohandler ioh;
    ioh.run (true);

    // Create the client socket
    //
    iom::socket_connection sock (ioh);

    // Address of the server we're connecting to
    //
    iom::ip_addr srv_addr (server_address, server_port);

    // Open the socket
    //
    if (sock.open(srv_addr.family(), SOCK_DGRAM)) {
        perror ("sock.open");
        return 1;
    }

    // Tell server we want to start DTLS handshake
    //
    if (sock.sendto("STARTTLS", 8, srv_addr, default_timeout) <= 0) {
        perror ("sock.sendto");
        return 1;
    }

    // Wait for server to initiate DTLS handshake from a client port
    //
    ssize_t result = sizeof (buf);
    auto client_addr = sock.recvfrom (buf, result, default_timeout);
    if (result!=8 || strncmp(buf, "STARTTLS", 8) ) {
        if (result < 0)
            perror ("sock.recvfrom");
        else
            cerr << "Wrong response from server, expected 'STARTTLS'" << endl;
        return 1;
    }

    // Set default peer address when sending/receiving datagrams
    //
    if (sock.connect(*client_addr, nullptr)) {
        perror ("sock.connect");
        return 1;
    }

    // Create a TLS adapter object
    //
    iom::tls_adapter dtls (sock);

    // Do a DTLS handshake with the server
    //
    if (dtls.start_client_dtls(iom::tls_config(false), default_timeout)) {
        perror ("dtls.start_client_tls");
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
        if (dtls.write(text.c_str(), text.size()) < 0) {
            cerr << "Error sending text: " << strerror(errno) << endl;
            break;
        }

        // Receive text from the server
        //
        auto result = dtls.read (buf, sizeof(buf));
        if (result <= 0) {
            if (result == 0)
                cerr << "Connection closed by peer" << endl;
            else
                perror ("dtls.read");
            break;
        }

        // Write the received text to standard output
        //
        cout.write (buf, result);
        cout << endl;
    }

    dtls.close ();
    ioh.stop ();

    return 0;
}
