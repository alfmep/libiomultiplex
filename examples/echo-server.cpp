/*
 * Copyright (C) 2021-2023 Dan Arrhenius <dan@ultramarin.se>
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
#include <map>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <getopt.h>
#include <iomultiplex.hpp>


namespace iom=iomultiplex;
using namespace std;


//------------------------------------------------------------------------------
// C O N S T A N T S
//------------------------------------------------------------------------------
static constexpr const char*    default_bind_addr = "::1";
static constexpr const char*    default_ipv4_bind_addr = "127.0.0.1";
static constexpr const uint16_t default_port = 42000;
static constexpr const unsigned default_timeout = 30000; // Timeout in milliseconds
static constexpr const size_t   buf_size = 2048;


//------------------------------------------------------------------------------
// G L O B A L S
//------------------------------------------------------------------------------
static std::mutex server_done;


//------------------------------------------------------------------------------
// T Y P E S
//------------------------------------------------------------------------------
struct appdata_t {
    iom::default_iohandler ioh;
    iom::SocketConnection srv_sock;
    iom::IpAddr ip_addr;
    iom::UxAddr ux_addr;
    string ca_file;
    string cert_file;
    string privkey_file;
    unsigned timeout; // Timeout in milliseconds
    bool udp;
    bool tls;
    bool verbose;

    appdata_t ()
        : srv_sock (ioh),
          ip_addr (default_bind_addr, default_port),
          timeout (default_timeout),
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
    out << "Usage: " << program_invocation_short_name << " [OPTIONS]" << endl;
    out << endl;
    out << "       Simple server that echoes back everything it gets." << endl;
    out << endl;
    out << "       OPTIONS:" << endl;
    out << "       -b, --bind=ADDRESS   Address to bind to (IPv4 or IPv6). Default is " << default_bind_addr << "." << endl;
    out << "       -d, --domain=PATH    Bind to a UNIX Domain address instead of an IP address." << endl;
    out << "       -p, --port=PORT      TCP/UDP port to listen to. Default is " << default_port << endl;
    out << "       -u, --udp            Use UDP instead of TCP." << endl;
    out << "       -i, --timeout=SEC    Terminate session after SEC seconds of inactivity. Default is "
        <<                              (default_timeout/1000) << " seconds." << endl;
    out << "       -t, --tls            Use TLS for stream connections(TCP)." << endl;
    out << "       -a, --ca=FILE        Certificate authority file." << endl;
    out << "       -c, --cert=FILE      Certificate file." << endl;
    out << "       -k, --privkey=FILE   Private key file." << endl;
    out << "       -v, --verbose        Verbose output." << endl;
    out << "       -h, --help           Print this help and exit." << endl;
    out << endl;

    exit (exit_code);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void parse_args (int argc, char* argv[], appdata_t& app)
{
    static struct option long_options[] = {
        { "bind",    required_argument, 0, 'b'},
        { "port",    required_argument, 0, 'p'},
        { "domain",  required_argument, 0, 'd'},
        { "udp",     no_argument,       0, 'u'},
        { "timeout", no_argument,       0, 'i'},
        { "tls",     no_argument,       0, 't'},
        { "ca",      required_argument, 0, 'a'},
        { "cert",    required_argument, 0, 'c'},
        { "privkey", required_argument, 0, 'k'},
        { "verbose", no_argument,       0, 'v'},
        { "help",    no_argument,       0, 'h'},
        { 0, 0, 0, 0}
    };
    static const char* arg_format = "b:p:d:ui:ta:c:k:vh";

    string bind_addr = default_bind_addr;
    int port = 0;
    bool have_port_argument = false;
    int tmp_timeout = (int) (default_timeout/1000);

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
        case 'd':
            app.ux_addr.path (optarg);
            break;
        case 'u':
            app.udp = true;
            break;
        case 'i':
            tmp_timeout = atoi(optarg);
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
        cerr << "Error: Invalid argument" << endl;
        print_usage_and_exit (cerr, 1);
    }

    if (!app.ux_addr.path().empty()) {
        // We don't use UDP for Unix domain sockets
        app.udp = false;
    }else{
        // Check non-Unix domain socket options
        if (app.ip_addr.parse(bind_addr) == false) {
            cerr << "Error: Invalid bind argument." << endl;
            exit (1);
        }
        if (have_port_argument) {
            if (port<=0 || port>65535) {
                cerr << "Error: Invalid port argument." << endl;
                exit (1);
            }
            app.ip_addr.port ((uint16_t)port);
        }
        /*
        if (app.tls && app.udp) {
            cerr << "Error: Can't use TLS when using option '--udp'" << endl;
            exit (1);
        }
        */
    }

    if (tmp_timeout <= 0) {
        cerr << "Error: Invalid timeout argument." << endl;
        exit (1);
    }
    app.timeout = tmp_timeout * 1000;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void cmd_signal_handler (int sig)
{
    // Signal the server to stop
    server_done.unlock ();
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void init_signal_handler ()
{
    struct sigaction sa;
    memset (&sa, 0, sizeof(sa));
    sigemptyset (&sa.sa_mask);
    sa.sa_handler = cmd_signal_handler;
    sigaction (SIGINT, &sa, nullptr);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_rx (appdata_t& app,
                   shared_ptr<iom::Connection> conn,
                   shared_ptr<char[]> buf,
                   string peer,
                   iom::io_result_t& ior)
{
    if (ior.result <= 0) {
        if (app.verbose) {
            switch (ior.errnum) {
            case 0:
                cout << "Connection closed by peer " << peer << endl;
                break;
            case ETIMEDOUT:
                cout << "Connection timed out, closing peer " << peer << endl;
                break;
            case ECANCELED:
                break;
            default:
                cout << "Error reading from " << peer << ": " << strerror(ior.errnum) << endl;
            }
        }
        conn->close ();
        buf.reset ();
        return;
    }

    if (app.verbose)
        cout << "Got '" << string(buf.get(), ior.result) << "' from " << peer << endl;

    // Echo back what we got, ignoring errors
    conn->write (ior.buf, ior.result, nullptr);

    // Continue reading
    conn->read (ior.buf,
                ior.size,
                [&app, conn, peer, buf](iom::io_result_t& ior)->bool{
                    on_rx (app, conn, buf, peer, ior);
                    return false;
                },
                app.timeout);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_udp_rx (appdata_t& app,
                       shared_ptr<char[]> buf,
                       string peer,
                       iom::io_result_t& ior)
{
    if (ior.result <= 0) {
        if (app.verbose) {
            switch (ior.errnum) {
            case 0:
                cout << "Connection closed by peer " << peer << endl;
                break;
            case ETIMEDOUT:
                cout << "Connection timed out, closing peer " << peer << endl;
                break;
            case ECANCELED:
                break;
            default:
                cout << "Error reading from " << peer << ": " << strerror(ior.errnum) << endl;
            }
        }
        app.srv_sock.close ();
        buf.reset ();
        return;
    }

    if (app.verbose)
        cout << "Got '" << string(buf.get(), ior.result) << "' from " << peer << endl;

    // Echo back what we got, ignoring errors
    app.srv_sock.write (ior.buf, ior.result, nullptr);

    // Continue reading
    app.srv_sock.read (ior.buf,
                       ior.size,
                       [&app, peer, buf](iom::io_result_t& ior)->bool{
                           on_udp_rx (app, buf, peer, ior);
                           return false;
                       },
                       app.timeout);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void start_rx (appdata_t& app,
                             shared_ptr<iom::Connection> conn,
                             string peer)
{
    shared_ptr<char[]> buf (new char[buf_size]);
    conn->read (buf.get(),
                buf_size,
                [&app, conn, peer, buf](iom::io_result_t& ior)->bool{
                    on_rx (app, conn, buf, peer, ior);
                    return false;
                },
                app.timeout);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_tls_handshake (appdata_t& app,
                              shared_ptr<iom::Connection> conn,
                              string peer,
                              int errnum,
                              const std::string& errstr)
{
    if (errnum) {
        if (app.verbose && errnum!=ECANCELED)
            cout << "TLS handshaked from " << peer << " failed: " << errstr << " (" << errnum << ")" << endl;
        conn->close ();
        conn.reset ();
        return;
    }
    cout << "Successful TLS handshaked with " << peer << endl;

    // Start reading data from the client
    start_rx (app, conn, peer);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_accept (appdata_t& app,
                       shared_ptr<iom::SocketConnection> cs,
                       int errnum)
{
    if (errnum) {
        if (errnum == ECANCELED)
            return;
        errno = errnum;
        perror ("sock.accept");
        exit (1);
    }

    string peer = cs->peer().to_string ();

    if (app.verbose) {
        if (peer.empty())
            cout << "Got connection" << endl;
        else
            cout << "Got connection from " << peer << endl;
    }

    if (app.tls) {

        cout << "Start TLS handshake with " << peer << endl;
        //
        // Perform TLS handshake
        //
        string peer = cs->peer().to_string ();
        iom::TlsConfig tls_cfg (false, app.ca_file, app.cert_file, app.privkey_file);
        shared_ptr<iom::Connection> tlsa (new iom::TlsAdapter(cs));

        dynamic_cast<iom::TlsAdapter*>(tlsa.get())->start_server_tls (
                tls_cfg,
                [&app, tlsa, peer](iom::TlsAdapter& tls){
                    on_tls_handshake (app,
                                      tlsa,
                                      peer,
                                      tls.last_error(),
                                      tls.last_error_msg());
                },
                5000); // 5 sec timeout
    }else{
        //
        // Start reading data from the client
        //
        start_rx (app, cs, cs->peer().to_string());
    }

    // Continue accepting new clients
    //
    app.srv_sock.accept ([&app](iom::SocketConnection& ss,
                                shared_ptr<iom::SocketConnection> cs,
                                int errnum)
                         {
                             on_accept (app, cs, errnum);
                         });
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_dtls_handshake (appdata_t& app,
                               shared_ptr<iom::Connection> conn,
                               shared_ptr<char[]> buf,
                               string peer,
                               int errnum,
                               const std::string& errstr)
{
    if (errnum) {
        if (app.verbose && errnum!=ECANCELED)
            cout << "DTLS handshaked from " << peer << " failed: " << errstr << " (" << errnum << ")" << endl;
        conn->close ();
        conn.reset ();
        return;
    }
    if (app.verbose)
        cout << "Successful DTLS handshake with " << peer << endl;

    // Start reading data from the client
    conn->read (buf.get(),
                buf_size,
                [&app, conn, peer, buf](iom::io_result_t& ior)->bool{
                    on_rx (app, conn, buf, peer, ior);
                    return false;
                },
                app.timeout);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_datagram_rx (appdata_t& app,
                            shared_ptr<char[]> buf,
                            const iom::SockAddr& peer_addr,
                            iom::io_result_t& ior)
{
    if (ior.errnum == ECANCELED)
        return;

    if (app.verbose) {
        if (ior.result<0)
            cout << "Error reading from " << peer_addr.to_string() << ": " << strerror(ior.errnum) << endl;
        else
            cout << "Got UDP packet from " << peer_addr.to_string() << endl;
    }

    if (app.tls) {
        // "Lock" UDP socket to peer
        app.srv_sock.connect (peer_addr, nullptr);

        // Perform TLS handshake
        //
        string peer = peer_addr.to_string ();
        iom::TlsConfig dtls_cfg (false, app.ca_file, app.cert_file, app.privkey_file);
        shared_ptr<iom::Connection> dtlsa (new iom::TlsAdapter(app.srv_sock, false));

        if (app.verbose)
            cout << "Start DTLS handshake with " << peer << endl;

        auto result = dynamic_cast<iom::TlsAdapter*>(dtlsa.get())->start_server_dtls (
                dtls_cfg,
                ior.buf,
                (size_t)(ior.result>0 ? ior.result : 0),
                [&app, dtlsa, buf, peer](iom::TlsAdapter& tls){
                    on_dtls_handshake (app,
                                       dtlsa,
                                       buf,
                                       peer,
                                       tls.last_error(),
                                       tls.last_error_msg());
                },
                5000); // 5 sec timeout
        if (result) {
            perror ("start_tls");
            exit (1);
        }
    }else{
        // Echo back what we got, ignoring errors
        ///*
        if (ior.result > 0)
            app.srv_sock.sendto (ior.buf, ior.result, peer_addr, nullptr);
        //*/

        // "Lock" UDP socket to peer
        app.srv_sock.connect (peer_addr, nullptr);
        string peer = peer_addr.to_string ();
        app.srv_sock.read (buf.get(),
                           buf_size,
                           [&app, peer, buf](iom::io_result_t& ior)->bool{
                               on_udp_rx (app, buf, peer, ior);
                               return false;
                           },
                           app.timeout);
        /*
        // Continue reading
        app.srv_sock.recvfrom (ior.buf,
                               ior.size,
                               [&app, buf](iom::io_result_t& ior, const iom::SockAddr& peer_addr)->bool{
                                   on_datagram_rx (app, buf, peer_addr, ior);
                                   return false;
                               });
        */
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
    iom::Log::set_callback (logger);
    iom::Log::priority (LOG_DEBUG);

    appdata_t app;

    // Parse command line arguments
    //
    parse_args (argc, argv, app);

    // Select IP address or Unix Domain Socket address
    //
    iom::SockAddr& srv_addr = app.ux_addr.path().empty() ?
        dynamic_cast<iom::SockAddr&>(app.ip_addr) :
        dynamic_cast<iom::SockAddr&>(app.ux_addr);

    if (app.verbose) {
        if (srv_addr.family()==AF_UNIX) {
            cout << "Waiting for " << (app.udp ? "packet" : "stream")
                 << " data " << (app.tls ? "TLS " : "") << "clients on '"
                 << srv_addr.to_string() << "'" << endl;
        }else{
            cout << "Waiting for "
                 << (app.tls ? (app.udp ? "DTLS" : "TLS") : (app.udp ? "UDP" : "TCP")) << "/"
                 << (srv_addr.family()==AF_INET ? "IPv4" : "IPv6")
                 << " clients on "
                 << srv_addr.to_string() << endl;
        }
    }

    // Exit gracefully on CTRL-C (SIGINT)
    server_done.lock ();
    init_signal_handler ();

    // Start the I/O handler
    app.ioh.run (true);

    // Open the server socket
    //
    if (app.srv_sock.open(srv_addr.family(), app.udp ? SOCK_DGRAM : SOCK_STREAM)) {
        perror ("sock.open");
        exit (1);
    }

    // Set socket option(s)
    //
    if (srv_addr.family()==AF_INET || srv_addr.family()==AF_INET6) {
        app.srv_sock.setsockopt (SO_REUSEADDR, 1);
        if (srv_addr.family()==AF_INET6)
            app.srv_sock.setsockopt (IPPROTO_IPV6, IPV6_V6ONLY, 1);
    }

    // Bind the server socket to its local address
    //
    if (app.srv_sock.bind(srv_addr)) {
        perror ("sock.bind");
        exit (1);
    }

    if (app.udp) {
        // Start accepting UDP clients
        //accept_udp (app);
        shared_ptr<char[]> buf (new char[buf_size]);
        app.srv_sock.recvfrom (buf.get(),
                               buf_size,
                               [&app, buf](iom::SocketConnection& ss, iom::io_result_t& ior, const iom::SockAddr& peer_addr){
                                   on_datagram_rx (app, buf, peer_addr, ior);
                               });
    }else{
        // Put the socket in listening state
        //
        if (app.srv_sock.listen()) {
            perror ("sock.listen");
            exit (1);
        }

        // Start accepting clients
        //
        app.srv_sock.accept ([&app](iom::SocketConnection& ss,
                                    shared_ptr<iom::SocketConnection> cs,
                                    int errnum)
                             {
                                 on_accept (app, cs, errnum);
                             });
    }

    // Wait for the server to terminate
    server_done.lock ();
    app.ioh.stop ();
    app.ioh.join ();

    cout << "Done." << endl;

    return 0;
}
