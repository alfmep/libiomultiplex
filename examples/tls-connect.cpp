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
#include <iomultiplex.hpp>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <memory>
#include <cstdlib>
#include <unistd.h>
#include <getopt.h>


namespace iom = iomultiplex;
using namespace std;


//------------------------------------------------------------------------------
// T Y P E S
//------------------------------------------------------------------------------
struct appdata_t {
    iom::default_iohandler ioh;
    iom::SocketConnection sock;
    std::unique_ptr<iom::SockAddr> addr;
    iom::TlsAdapter tls;
    iom::TlsConfig tls_cfg;
    string host_to_resolve;
    bool ipv4_only;
    bool ipv6_only;
    bool unix_only;
    bool use_dtls;

    appdata_t ()
        : sock (ioh),
          tls (sock),
          tls_cfg (false),  // Set verify server to false since we will verify the server 'manually'.
          ipv4_only (false),
          ipv6_only (false),
          unix_only (false),
          use_dtls (false)
    {
    }
};


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void print_usage_and_exit (ostream& out, int exit_code)
{
    out << endl;
    out << "Usage: " << program_invocation_short_name << " [OPTIONS] <HOST> [PORT]" << endl;
    out << endl;
    out << "       Connect to HOST:PORT using (D)TLS, print status and disconnect." << endl;
    out << "       PORT is ignored if using Unix Domain Sockets, otherwise it is required." << endl;
    out << endl;
    out << "       OPTIONS:" << endl;
    out << "       -4, --ipv4                Connect using IPv4 only." << endl;
    out << "       -6, --ipv6                Connect using IPv6 only." << endl;
    out << "       -u, --unix                Connect using a Unix Domain Socket. Parameter PORT is ignored." << endl;
    out << "       -d, --dtls                Use DLTS instead of TLS." << endl;
    out << "       -a, --ca=FILE             Optional certificate authority file." << endl;
    out << "       -p, --ca-path=PATH        Optional certificate authority path." << endl;
    out << "       -c, --cert=FILE           Optional client certificate file." << endl;
    out << "       -k, --privkey=FILE        Optional private key file for the client certificate." << endl;
    out << "       -s[NAME], --sni[=NAME]    Optional SNI value. HOST is used if NAME is omitted." << endl;
    out << "       --min-tls-ver=VER         Minimum allowed TLS version. " << endl
        << "                                 VER is one of tls1, tls1_1, tls1_2, and tls1_3." << endl;
    out << "       --max-tls-ver=VER         Maximum allowed TLS version. " << endl
        << "                                 VER is one of tls1, tls1_1, tls1_2, and tls1_3." << endl;
    out << "       --min-dtls-ver=VER        Minimum allowed DTLS version. " << endl
        << "                                 VER is one of dtls1 and dtls1_2." << endl;
    out << "       --max-dtls-ver=VER        Maximum allowed DTLS version. " << endl
        << "                                 VER is one of dtls1 and dtls1_2." << endl;
    out << "       -h, --help                Print this help and exit." << endl;
    out << endl;

    exit (exit_code);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void parse_args (int argc, char* argv[], appdata_t& app)
{
    int opt;
    static struct option long_options[] = {
        { "ipv4",         no_argument,       0, '4'},
        { "ipv6",         no_argument,       0, '6'},
        { "unix",         no_argument,       0, 'u'},
        { "dtls",         no_argument,       0, 'd'},
        { "ca",           required_argument, 0, 'a'},
        { "ca-path",      required_argument, 0, 'p'},
        { "cert",         required_argument, 0, 'c'},
        { "privkey",      required_argument, 0, 'k'},
        { "sni",          optional_argument, 0, 's'},
        { "min-tls-ver",  required_argument, &opt, 1},
        { "max-tls-ver",  required_argument, &opt, 2},
        { "min-dtls-ver", required_argument, &opt, 3},
        { "max-dtls-ver", required_argument, &opt, 4},
        { "help",    no_argument,       0, 'h'},
        { 0, 0, 0, 0}
    };
    static const char* arg_format = "46uda:p:c:k:s::h";
    bool have_sni_arg = false;

    while (1) {
        int c = getopt_long (argc, argv, arg_format, long_options, NULL);
        if (c == -1)
            break;
        switch (c) {
        case 0:
            switch (opt) {
            case 1:
                if (strcmp(optarg, "tls1")==0)
                    app.tls_cfg.min_tls_ver = TLS1_VERSION;
                else if (strcmp(optarg, "tls1_1")==0)
                    app.tls_cfg.min_tls_ver = TLS1_1_VERSION;
                else if (strcmp(optarg, "tls1_2")==0)
                    app.tls_cfg.min_tls_ver = TLS1_2_VERSION;
                else if (strcmp(optarg, "tls1_3")==0)
                    app.tls_cfg.min_tls_ver = TLS1_3_VERSION;
                else {
                    cerr << "Error: Invalid TLS version argument" << endl;
                    exit (1);
                }
                break;
            case 2:
                if (strcmp(optarg, "tls1")==0)
                    app.tls_cfg.max_tls_ver = TLS1_VERSION;
                else if (strcmp(optarg, "tls1_1")==0)
                    app.tls_cfg.max_tls_ver = TLS1_1_VERSION;
                else if (strcmp(optarg, "tls1_2")==0)
                    app.tls_cfg.max_tls_ver = TLS1_2_VERSION;
                else if (strcmp(optarg, "tls1_3")==0)
                    app.tls_cfg.max_tls_ver = TLS1_3_VERSION;
                else {
                    cerr << "Error: Invalid TLS version argument" << endl;
                    exit (1);
                }
                break;
            case 3:
                if (strcmp(optarg, "dtls1")==0)
                    app.tls_cfg.min_dtls_ver = DTLS1_VERSION;
                else if (strcmp(optarg, "dtls1_2")==0)
                    app.tls_cfg.min_dtls_ver = DTLS1_2_VERSION;
                else {
                    cerr << "Error: Invalid DTLS version argument" << endl;
                    exit (1);
                }
                break;
            case 4:
                if (strcmp(optarg, "dtls1")==0)
                    app.tls_cfg.max_dtls_ver = DTLS1_VERSION;
                else if (strcmp(optarg, "dtls1_2")==0)
                    app.tls_cfg.max_dtls_ver = DTLS1_2_VERSION;
                else {
                    cerr << "Error: Invalid DTLS version argument" << endl;
                    exit (1);
                }
                break;
            }
            break;
        case '4':
            app.ipv4_only = true;
            app.ipv6_only = false;
            app.unix_only = false;
            break;
        case '6':
            app.ipv6_only = true;
            app.ipv4_only = false;
            app.unix_only = false;
            break;
        case 'u':
            app.unix_only = true;
            app.ipv4_only = false;
            app.ipv6_only = false;
            break;
        case 'd':
            app.use_dtls = true;
            break;
        case 's':
            app.tls_cfg.sni = (optarg ? optarg : "");
            have_sni_arg = true;
            break;
        case 'a':
            app.tls_cfg.ca_file = optarg;
            break;
        case 'p':
            app.tls_cfg.ca_path = optarg;
            break;
        case 'c':
            app.tls_cfg.cert_file = optarg;
            break;
        case 'k':
            app.tls_cfg.privkey_file = optarg;
            break;
        case 'h':
            print_usage_and_exit (cout, 0);
            break;
        default:
            print_usage_and_exit (cerr, 1);
            break;
        }
    }

    try {
        if (optind < argc) {
            if (app.unix_only) {
                app.addr.reset (new iom::UxAddr(argv[optind++]));
                if (optind < argc)
                    ++optind;
            }else{
                app.addr.reset (new iom::IpAddr);
                app.host_to_resolve = argv[optind++];
                if (optind < argc) {
                    int port = stoi (argv[optind++]);
                    if (port<1 || port>65535)
                        throw invalid_argument ("Invalid port number");
                    dynamic_cast<iom::IpAddr*>(app.addr.get())->port (port);
                }else{
                    cerr << "Error: Missing PORT argument" << endl;
                    exit (1);
                }
            }
        }else{
            cerr << "Error: Missing HOST argument" << endl;
            exit (1);
        }
        if (optind < argc) {
            cerr << "Error: Too many arguments" << endl;
            exit (1);
        }
    }catch (...) {
            cerr << "Error: Invalid HOST and/or PORT argument" << endl;
            exit (1);
    }

    if (have_sni_arg && app.tls_cfg.sni.empty())
        app.tls_cfg.sni = app.host_to_resolve;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool resolve_host (appdata_t& app)
{
    bool retval = false;
    iom::IpAddr& addr = dynamic_cast<iom::IpAddr&> (*app.addr);

    auto addr_list = iom::Resolver().lookup_host (app.host_to_resolve, addr.port());

    for (auto& a : addr_list) {
        if (!app.ipv4_only && !app.ipv6_only) {
            addr = a;
            retval = true;
            break;
        }
        else if (app.ipv4_only && a.family()==AF_INET) {
            addr = a;
            retval = true;
            break;
        }
        else if (app.ipv6_only && a.family()==AF_INET6) {
            addr = a;
            retval = true;
            break;
        }
    }
    return retval;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static std::string time_point_to_string (const std::chrono::time_point<std::chrono::system_clock>& tp)
{
    std::time_t t = std::chrono::system_clock::to_time_t (tp);
    std::stringstream ss;
    ss << std::put_time(localtime(&t), "%Y-%m-%d %H:%M:%S %Z");
    return ss.str ();
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    appdata_t app;
    parse_args (argc, argv, app);

    // Resolv the hostname
    //
    if (!app.unix_only && !resolve_host(app)) {
        cerr << "Error: Can't resolve host '" << app.host_to_resolve << "'" << endl;
        return 1;
    }

    // Start the I/O handler
    //
    app.ioh.run (true);
    cout << "Connecting to " << app.addr->to_string() << endl;

    // Open the socket
    //
    if (app.sock.open(app.addr->family(), app.use_dtls?SOCK_DGRAM:SOCK_STREAM)) {
        cerr << "Error opening socket: " << strerror(errno) << endl;
        return 1;
    }

    // Connect to the remote host
    //
    if (app.sock.connect(*app.addr, 5000)) {
        cerr << "Error connecting socket: " << strerror(errno) << endl;
        return 1;
    }
    cout << "Connected to " << app.addr->to_string() << ", perform " << (app.use_dtls?"D":"") << "TLS handshake" << endl;

    // Perform TLS handshake
    //
    app.tls.start_tls (app.tls_cfg, false, app.use_dtls, 5000);
    if (app.tls.last_error()) {
        cerr << (app.use_dtls?"D":"") << "TLS handshake failed: "
             << app.tls.last_error_msg() << " (" << app.tls.last_error() << ")" << endl;
        return 1;
    }

    // Show connection status
    //
    cout << (app.use_dtls?"D":"") << "TLS handshake finished, connection status:" << endl;
    string err_msg;
    if (app.tls.verify_peer(err_msg))
        cout << "\tServer certificate successfully validated " << endl;
    else
        cout << "\tError validating server certificate: " << err_msg << endl;
    auto server_cert = app.tls.peer_cert ();
    cout << "\tServer certificte Common Name: " << (server_cert ? server_cert.common_name() : "n/a") << endl;
    if (server_cert) {
        cout << "\tServer certificate valid from: " << time_point_to_string(server_cert.not_before()) << endl;
        cout << "\tServer certificate valid to  : " << time_point_to_string(server_cert.not_after()) << endl;
    }
    cout << "\tUsed " << (app.use_dtls?"D":"") << "TLS protocol: " << app.tls.proto_ver() << endl;
    cout << "\tUsed cipher: " << app.tls.cipher_name() << endl;

    app.tls.close ();

    return 0;
}
