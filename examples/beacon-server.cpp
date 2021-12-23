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
#include <iostream>
#include <string>
#include <memory>
#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <getopt.h>
#include <iomultiplex.hpp>


//
// Simple example of a server running in a single thread, using async I/O, accepting multiple clients.
//
// Telnet to port 12000 on 127.0.0.1 to get data beacons.
//

using namespace std;
namespace iom = iomultiplex;


static constexpr const char*    default_bind_addr = "127.0.0.1";
static constexpr const uint16_t default_port = 12000;
static constexpr const size_t   default_interval = 1000; // 1 second
static constexpr const size_t   default_size = 1;      // 1 byte


struct appdata_t {
    iom::default_iohandler ioh;
    iom::SocketConnection srv_sock;
    iom::IpAddr srv_addr;
    iom::TimerSet beacon_timers;
    unique_ptr<char> beacons[16];

    // Command line options
    size_t interval; // milliseconds
    size_t size;
    bool verbose;

    // Default constructor
    appdata_t () :
        srv_sock (ioh),
        beacon_timers (ioh),
        interval (default_interval),
        size (default_size),
        verbose (false)
        {}
};


static void print_usage_and_exit (ostream& out, int exit_code);
static void parse_args (int argc, char* argv[], appdata_t& app);
static void on_new_client (appdata_t& app, shared_ptr<iom::SocketConnection> cs, int errnum);
static void send_beacon (appdata_t& app,
                         shared_ptr<iom::SocketConnection> cs,
                         shared_ptr<int> beacon_index,
                         long timer_id);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void print_usage_and_exit (ostream& out, int exit_code)
{
    out << endl;
    out << "Usage: " << program_invocation_short_name << " [OPTIONS]" << endl;
    out << endl;
    out << "       Simple example of a server that periodically sends out beacons of data to connected clients." << endl;
    out << endl;
    out << "       OPTIONS:" << endl;
    out << "       -b, --bind=ADDRESS             Address to bind to. Default is " << default_bind_addr << "." << endl;
    out << "       -p, --port=PORT                TCP port to listen to. Default is " << default_port << endl;
    out << "       -i, --interval=MILLISECONDS    Interval between data beacons." << endl;
    out << "       -s, --size=BYTES               Size in bytes of each data beacon." << endl;
    out << "       -v, --verbose                  Print debug info." << endl;
    out << "       -h, --help                     Print this help and exit." << endl;
    out << endl;

    exit (exit_code);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void parse_args (int argc, char* argv[], appdata_t& app)
{
    static struct option long_options[] = {
        { "bind",     required_argument, 0, 'b'},
        { "port",     required_argument, 0, 'p'},
        { "interval", required_argument, 0, 'i'},
        { "size",     required_argument, 0, 's'},
        { "verbose",  no_argument,       0, 'v'},
        { "help",     no_argument,       0, 'h'},
        { 0, 0, 0, 0}
    };
    static const char* arg_format = "b:p:i:s:vh";

    string bind_addr = default_bind_addr;
    int port = default_port;
    bool have_port_argument = false;

    while (1) {
        int c = getopt_long (argc, argv, arg_format, long_options, NULL);
        if (c == -1)
            break;
        switch (c) {
        case 'b':
            bind_addr = optarg;
            break;
        case 'p':
            port = atoi (optarg);
            have_port_argument = true;
            break;
        case 'i':
            app.interval = atoi (optarg);
            break;
        case 's':
            app.size = atoi (optarg);
            break;
        case 'v':
            app.verbose = true;
            break;
        case 'h':
            print_usage_and_exit (cout, 0);
            break;
        default:
            print_usage_and_exit (cerr, 1);
            break;
        }
    }
    if (optind < argc) {
        cerr << "Error: Invalid argument" << endl;
        print_usage_and_exit (cerr, 1);
    }
    if (have_port_argument && (port<=0 || port>65535)) {
        cerr << "Error: Invalid port argument." << endl;
        exit (1);
    }
    if (app.srv_addr.parse(bind_addr) == false) {
        cerr << "Error: Invalid bind argument." << endl;
        exit (1);
    }
    if (app.interval == 0) {
        cerr << "Error: Invalid time argument. Must be greater than 0." << endl;
        exit (1);
    }
    if (app.size == 0) {
        cerr << "Error: Invalid size argument. Must be greater than 0." << endl;
        exit (1);
    }
    if (have_port_argument || !app.srv_addr.port())
        app.srv_addr.port ((uint16_t)port);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    appdata_t app;

    // Parse command line arguments
    //
    parse_args (argc, argv, app);

    // Initialize payload data in beacons:
    //
    for (int i=0; i<16; ++i) {
        char payload;
        if (i < 10)
            payload = '0' + i;
        else
            payload = 'a' + (i-10);
        app.beacons[i].reset (new char[app.size]);
        memset (app.beacons[i].get(), payload, app.size);
    }

    // Open the server socket
    //
    if (app.srv_sock.open(app.srv_addr.family(), SOCK_STREAM)) {
        perror ("sock.open");
        return 1;
    }

    // Set socket option(s)
    //
    app.srv_sock.setsockopt (SO_REUSEADDR, 1);

    // Bind the server socket to our local address
    //
    if (app.srv_sock.bind(app.srv_addr)) {
        perror ("sock.bind");
        return 1;
    }

    // Put the socket in listening state
    //
    if (app.srv_sock.listen()) {
        perror ("sock.listen");
        return 1;
    }

    // Start accepting clients
    //
    cout << "Accepting clients on " << app.srv_addr.to_string() << endl;
    app.srv_sock.accept ([&app](iom::SocketConnection& ss,
                                shared_ptr<iom::SocketConnection> cs,
                                int errnum)
                             {
                                 on_new_client (app, cs, errnum);
                             });

    // Run the I/O handler
    //
    app.ioh.run ();

    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_new_client (appdata_t& app,
                           shared_ptr<iom::SocketConnection> cs,
                           int errnum)
{
    if (errnum) {
        errno = errnum;
        perror ("sock.accept");
        exit (1);
    }

    if (app.verbose)
        cout << "Got connection from " << cs->peer().to_string() << endl;

    // Start sending data beacons to the client.
    //
    auto beacon_index = make_shared<int> ();
    *beacon_index = 0;
    app.beacon_timers.set (0, app.interval, [&app, cs, beacon_index](iom::TimerSet& ts, long timer_id)
                                            {
                                                // Send beacon when the timer expires
                                                send_beacon (app, cs, beacon_index, timer_id);
                                            });

    // Continue accepting new clients
    //
    app.srv_sock.accept ([&app](iom::SocketConnection& ss,
                                shared_ptr<iom::SocketConnection> cs,
                                int errnum)
                             {
                                 on_new_client (app, cs, errnum);
                             });
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void send_beacon (appdata_t& app,
                         shared_ptr<iom::SocketConnection> cs,
                         shared_ptr<int> beacon_index,
                         long timer_id)
{
    if (app.verbose) {
        cout << "Send beacon '" << app.beacons[*beacon_index].get()[0]
             << "' to " << cs->peer().to_string() << endl;
    }

    // Send next data beacon
    //
    cs->write (app.beacons[*beacon_index].get(),
               app.size,
               [&app, cs, timer_id](iom::io_result_t& ior)->bool
               {
                   // Stop the beacon if the client disconnected, or on error
                   if (ior.result <= 0) {
                       if (app.verbose)
                           cout << "Client " << cs->peer().to_string() << " disconnected." << endl;
                       app.beacon_timers.cancel (timer_id);
                       // Free client socket resources
                       const_cast<shared_ptr<iom::SocketConnection>&>(cs).reset ();
                   }
                   return false;
               });

    // Update the beacon index
    //
    *beacon_index += 1;
    if (*beacon_index >= 16)
        *beacon_index = 0;
}
