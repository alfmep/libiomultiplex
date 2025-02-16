/*
 * Copyright (C) 2021,2025 Dan Arrhenius <dan@ultramarin.se>
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
#include <unistd.h>
#include <getopt.h>


namespace iom=iomultiplex;
using namespace std;


//
// Very simple example of sending and receiving data using a UDP socket
//


// Remote port number
static constexpr const char*    default_addr = "::1";
static constexpr const uint16_t default_port = 42000;


//------------------------------------------------------------------------------
// T Y P E S
//------------------------------------------------------------------------------
struct appdata_t {
    iom::default_iohandler ioh;
    iom::socket_connection sock;
    iom::ip_addr ip_addr;
    iom::ux_addr ux_addr;
    string ca_file;
    string cert_file;
    string privkey_file;
    bool udp;
    bool tls;
    bool verbose;

    appdata_t ()
        : sock (ioh),
          ip_addr (default_addr, default_port),
          udp (false),
          tls (false),
          verbose (false)
    {
    }
};


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void print_usage_and_exit (ostream& out, int exit_code)
{
    out << endl;
    out << "Usage: " << program_invocation_short_name << " [OPTIONS] [ADDRESS[:PORT]]" << endl;
    out << endl;
    out << "       Simple client that sends from standard input and receives to standard output." << endl;
    out << "       Default ADDRESS:PORT is [" << default_addr << "]:" << default_port << endl;
    out << endl;
    out << "       OPTIONS:" << endl;
    out << "       -d, --domain          Connecto to a UNIX Domain socket instead of an IP address." << endl;
    out << "       -u, --udp             Use UDP instead of TCP." << endl;
    out << "       -t, --tls             Use TLS for stream connections(TCP)." << endl;
    out << "       -a, --ca=FILE         Certificate authority file." << endl;
    out << "       -c, --cert=FILE       Certificate file." << endl;
    out << "       -k, --privkey=FILE    Private key file." << endl;
    out << "       -v, --verbose         Verbose output." << endl;
    out << "       -h, --help            Print this help and exit." << endl;
    out << endl;

    exit (exit_code);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void parse_args (int argc, char* argv[], appdata_t& app)
{
    static struct option long_options[] = {
        { "domain",  no_argument,       0, 'd'},
        { "udp",     no_argument,       0, 'u'},
        { "tls",     no_argument,       0, 't'},
        { "ca",      required_argument, 0, 'a'},
        { "cert",    required_argument, 0, 'c'},
        { "privkey", required_argument, 0, 'k'},
        { "verbose", no_argument,       0, 'v'},
        { "help",    no_argument,       0, 'h'},
        { 0, 0, 0, 0}
    };
    static const char* arg_format = "duta:c:k:vh";
    bool have_domain_arg = false;

    while (1) {
        int c = getopt_long (argc, argv, arg_format, long_options, NULL);
        if (c == -1)
            break;
        switch (c) {
        case 'd':
            have_domain_arg = true;
            break;
        case 'u':
            app.udp = true;
            break;
        case 't':
            app.tls = true;
            break;
        case 'a':
            app.ca_file = optarg;
            break;
        case 'c':
            app.cert_file = optarg;
            break;
        case 'k':
            app.privkey_file = optarg;
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
        if (have_domain_arg) {
            app.ux_addr.path (argv[optind++]);
        }else{
            if (app.ip_addr.parse(argv[optind++]) == false) {
                cerr << "Error: Invalid ADDRESS:PORT argument." << endl;
                exit (1);
            }
        }
    }else{
        if (have_domain_arg) {
            cerr << "Error: Must have ADDRESS argument when using Unix domain socket connection." << endl;
            exit (1);
        }
    }
    if (optind < argc) {
        cerr << "Error: Invalid argument" << endl;
        print_usage_and_exit (cerr, 1);
    }

    if (!app.ux_addr.path().empty()) {
        // We don't use UDP for Unix domain sockets
        app.udp = false;
    }
}


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
    iom::log::set_callback (logger);
    iom::log::priority (LOG_DEBUG);

    // Parse command line arguments
    //
    appdata_t app;
    parse_args (argc, argv, app);

    // Start the I/O handler in a worker thread
    //
    app.ioh.run (true);

    // Select IP address or Unix Domain Socket address
    //
    iom::sock_addr& srv_addr = app.ux_addr.path().empty() ?
        dynamic_cast<iom::sock_addr&>(app.ip_addr) :
        dynamic_cast<iom::sock_addr&>(app.ux_addr);

    // Open the client socket
    //
    if (app.sock.open(srv_addr.family(), app.udp ? SOCK_DGRAM : SOCK_STREAM)) {
        perror ("sock.open");
        exit (1);
    }

    if (app.sock.connect(srv_addr)) {
        perror ("sock.connect");
        return 1;
    }
    cout << "Remote address: " << app.sock.peer().to_string() << endl;

    iom::tls_adapter tlsa (app.sock);
    if (app.tls) {
        cout << "Start TLS handshake" << endl;
        if (tlsa.start_tls(iom::tls_config(false), false, app.udp)) {
            perror ("sock.start_tls");
            return 1;
        }
    }

    iom::connection& conn = app.tls ?
        dynamic_cast<iom::connection&>(tlsa) :
        dynamic_cast<iom::connection&>(app.sock);

    // Send some data
    //
    cout << "Enter some text to send: " << flush;
    std::string text;
    getline (cin, text);
    if (!text.size()) {
        conn.close ();
        return 0;
    }

    cout << "Sending " << text.size() << " bytes text to " << app.sock.peer().to_string() << endl;
    if (conn.write(text.c_str(), text.size()) < 0) {
        perror ("sock.write");
        return 1;
    }

    cout << "Wait for result..." << flush;

    // Read some data
    //
    char buf[4096];
    ssize_t result;
    result = conn.read (buf, sizeof(buf));
    if (result < 0) {
        cerr << endl;
        perror ("sock.read");
        return 1;
    }
    buf[sizeof(buf)-1] = '\0'; // Just making sure the buffer is null-terminated

    cout << " got back " << result << " bytes of data:" << endl;
    cout << '"' << buf << '"' << endl;

    conn.close ();

    return 0;
}
