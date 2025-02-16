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
#include <iomultiplex.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <set>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

#include "tftp.hpp"
#include "tftp-session.hpp"
#include "iom-tftpd-options.hpp"

//using namespace std;
using std::cout;
using std::cerr;
using std::endl;

namespace iom = iomultiplex;


class less_tftp_session_t {
public:
    bool operator() (const tftp_session_t& lhs, const tftp_session_t& rhs) const {
        return &lhs < &rhs;
    }
};


struct appstate_t {
    appstate_t (const appargs_t& appargs)
        : opt(appargs)
    {
        uid = getuid ();
        gid = getgid ();
    }

    const appargs_t& opt;
    std::string real_tftproot;
    std::vector<std::unique_ptr<iom::iohandler_base>> workers;
    std::vector<iom::socket_connection> listeners;
    uid_t uid;
    gid_t gid;

    std::set<tftp_session_t, less_tftp_session_t> sessions;
    std::mutex session_mutex;

    static std::mutex server_done;
};
std::mutex appstate_t::server_done;





static void accept_request (appstate_t& app,
                            iom::socket_connection& sock,
                            void* buf,
                            size_t size);
static void handle_new_request (appstate_t& app,
                                iom::io_result_t& ior,
                                iom::socket_connection& sock,
                                const iom::ip_addr& addr);
static void handle_rq (appstate_t& app,
                       opcode_t op,
                       const std::string& filename,
                       iom::socket_connection& sock,
                       const iom::ip_addr& addr);




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void cmd_signal_handler (int sig)
{
    // Signal the TFTP server to stop
    appstate_t::server_done.unlock ();
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
    sigaction (SIGTERM, &sa, nullptr);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void stdout_logger (unsigned prio, const char* msg)
{
    cout << '[' << gettid() << "] ";
    switch (prio) {
    case LOG_EMERG:
        cout << "EMERG: ";
        break;
    case LOG_ALERT:
        cout << "ALERT: ";
        break;
    case LOG_CRIT:
        cout << "CRIT: ";
        break;
    case LOG_ERR:
        cout << "ERROR: ";
        break;
    case LOG_WARNING:
        cout << "WARNING: ";
        break;
    case LOG_NOTICE:
        cout << "NOTICE: ";
        break;
    case LOG_INFO:
        cout << "INFO: ";
        break;
    case LOG_DEBUG:
        cout << "DEBUG: ";
        break;
    }
    cout << msg << endl;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int get_uid (const std::string& user, uid_t& uid)
{
    struct passwd* pwd = nullptr;
    errno = 0;
    try {
        pwd = getpwuid ((uid_t)std::stol(user));
    }
    catch (...) {
        pwd = getpwnam (user.c_str());
    }
    if (!pwd)
        return -1;

    uid = pwd->pw_uid;
    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int get_gid (const std::string& group, gid_t& gid)
{
    struct group* pwd = nullptr;
    errno = 0;
    try {
        pwd = getgrgid ((uid_t)std::stol(group));
    }
    catch (...) {
        pwd = getgrnam (group.c_str());
    }
    if (!pwd)
        return -1;

    gid = pwd->gr_gid;
    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int set_tftproot (appstate_t& app)
{
    // Get the canonical server root path
    //
    auto* path = realpath (app.opt.tftproot.c_str(), nullptr);
    if (path) {
        app.real_tftproot = path;
        app.real_tftproot.append ("/");
        free (path);
    }else{
        return -1;
    }

    return chdir (app.real_tftproot.c_str());
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int do_fork ()
{
    auto pid = fork ();
    if (pid < 0) {
        iom::log::error ("Unable to fork process: %s", strerror(errno));
        return -1;
    }
    else if (pid > 0) {
        // Exit parent process
        exit (0);
    }
    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int daemonize (appstate_t& app)
{
    // Reset umask
    umask (0);

    if (!app.opt.foreground) {
        // fork
        if (do_fork())
            return -1;

        // Disassociate from the process group and the controlling terminal
        setsid ();

        // Do a second fork to make a process that is not a session leader
        // and that will not be able to create a controlling terminal
        if (do_fork())
            return -1;
    }

    // Close open file descriptors
    for (int i=0; i<1024; ++i) {
        if ((i==1||i==2) && app.opt.log_to_stdout)
            continue;
        close (i);
    }

    // (Re)open syslog
    if (!app.opt.log_to_stdout)
        openlog ("iom-tftpd", LOG_PID, LOG_DAEMON);

    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool set_privileges (appstate_t& app)
{
    // Get uid and gid from application arguments
    if (!app.opt.user.empty() && get_uid(app.opt.user, app.uid)) {
        iom::log::error ("Unable to set user id: %s",
                         (errno ? strerror(errno) : "No such user."));
        return -1;
    }
    if (!app.opt.user.empty() && get_gid(app.opt.group, app.gid)) {
        iom::log::error ("Unable to set group id: %s",
                         (errno ? strerror(errno) : "No such group."));
        return -1;
    }

    // Set group id
    if (app.gid != getgid()) {
        iom::log::debug ("Setting group id to %u", (unsigned(app.gid)));
        errno = 0;
        if (setgid(app.gid)) {
            iom::log::error ("Unable to set group id: %s", strerror(errno));
            return -1;
        }
    }

    // Set user id
    if (app.uid != getuid()) {
        errno = 0;
        iom::log::debug ("Setting user id to %u", (unsigned(app.uid)));
        if (setuid(app.uid)) {
            iom::log::error ("Unable to set user id: %s", strerror(errno));
            return -1;
        }
    }

    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int create_io_workers (appstate_t& app)
{
    for (int i=0; i<app.opt.num_workers; ++i) {
        // Create a new I/O handler
        app.workers.emplace_back (std::make_unique<iom::default_iohandler>(SIGRTMIN+i));
        //app.workers.emplace_back (std::make_unique<iom::IOHandler_Poll>(SIGRTMIN+i));
        auto& ioh = *app.workers.back();

        // Create new socket listener
        app.listeners.emplace_back (iom::socket_connection(ioh));
        auto& sock = app.listeners.back();

        if (sock.open(app.opt.bind_addr.family(), SOCK_DGRAM) == 0) {
            // Set SO_REUSEPORT so all worker threads
            // can bind to the same local port
            if (sock.setsockopt(SO_REUSEPORT, 1) == 0)
                sock.bind (app.opt.bind_addr);
        }
        if (errno) {
            iom::log::error ("Unable to open/bind socket: %s", strerror(errno));
            return 1;
        }
    }
    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    // Parse arguments
    //
    appargs_t opt;
    auto parse_result = opt.parse_args (argc, argv);
    if (parse_result) {
        return parse_result<0 ? 1 : 0;
    }

    appstate_t app (opt);

    // Configure logging
    //
    if (opt.log_to_stdout) {
        iom::log::set_callback (stdout_logger);
    }else{
        openlog ("iom-tftpd", LOG_PID, LOG_DAEMON);
    }
    iom::log::priority (opt.verbose ? LOG_DEBUG : LOG_INFO);

    // Change working directory to the tftp root
    //
    if (set_tftproot(app)) {
        iom::log::error ("Unable to set working directory: %s", strerror(errno));
        exit (1);
    }

    // Daemonize
    //
    if (daemonize(app))
        exit (1);

    // Create PID file
    //
    if (!app.opt.pid_file.empty()) {
        std::ofstream pf (app.opt.pid_file);
        pf << getpid() << std::endl;
        if (!pf.good()) {
            iom::log::error ("Error creating pid file");
            exit (1);
        }
    }

    // Exit gracefully on CTRL-C (SIGINT) and SIGTERM
    //
    appstate_t::server_done.lock ();
    init_signal_handler ();

    // Create the I/O workers
    //
    if (create_io_workers(app))
        exit (1);

    // Server ready to serve, but first drop privileges
    //
    if (set_privileges(app))
        exit (1);

    // Start I/O workers
    //
    for (auto& ioh : app.workers)
        ioh->run (true);

    iom::log::info ("Serving files from %s, directory %s",
                    app.opt.bind_addr.to_string(true).c_str(),
                    app.opt.tftproot.c_str());
    if (app.opt.allow_wrq) {
        if (app.opt.max_wrq_size)
            iom::log::debug ("Allow write requests with a size limit of %lu bytes", app.opt.max_wrq_size);
        else
            iom::log::debug ("Allow write requests with no size limit");
    }

    // Start receiving requests on all socket listeners
    //
    iom::buffer_pool pool (sizeof(tftp_pkt_t), app.opt.num_workers);
    for (auto& sock : app.listeners)
        accept_request (app, sock, pool.get(), pool.buf_size());

    // Wait for the TFTP server to terminate (SIGINT or SIGTERM)
    //
    appstate_t::server_done.lock ();

    // Stop listeners
    //
    app.listeners.clear ();

    // Stop pending sessions
    //
    app.session_mutex.lock ();
    app.sessions.clear ();

    // Stop all I/O workers
    //
    for (auto& ioh : app.workers) {
        ioh->stop ();
        ioh->join ();
    }

    iom::log::info ("Stopped");
    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void accept_request (appstate_t& app,
                            iom::socket_connection& sock,
                            void* buf,
                            size_t size)
{
    sock.recvfrom (buf,
                   size,
                   [&app](iom::socket_connection& sock,
                          iom::io_result_t& ior,
                          const iom::sock_addr& addr)
                   {
                       // Called by the socket listener
                       // on a new incomming connection
                       handle_new_request (app,
                                           ior,
                                           sock,
                                           dynamic_cast<const iom::ip_addr&>(addr));
                   },
                   -1);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void handle_new_request (appstate_t& app,
                                iom::io_result_t& ior,
                                iom::socket_connection& sock,
                                const iom::ip_addr& addr)
{
    tftp_pkt_t& pkt = *((tftp_pkt_t*)ior.buf);

    // Check for error
    //
    if (ior.result < 0) {
        if (ior.errnum == ECANCELED) {
            // Server closing down
            sock.close ();
            return;
        }else{
            iom::log::info ("Error waiting for client request on socket #%d: ",
                            sock.handle(), strerror(ior.errnum));
        }
        // Continue listening for new clients
        accept_request (app, sock, ior.buf, ior.size);
        return;
    }

    // Sanity check
    //
    if (ior.result < tftp_min_packet_size) {
        // Invalid tftp packet, continue listening for new clients
        iom::log::info ("Ignoring TFTP request with packet size %d from %s",
                        ior.result, addr.to_string(true).c_str());
        // Continue listening for new clients
        accept_request (app, sock, ior.buf, ior.size);
        return;
    }

    // Check TFTP packet type
    //
    switch (ntohs(pkt.opcode)) {
    case op_rrq:
        // Read request
        // Make sure the filename is null-terminated
        pkt.op.rq.filename[ior.result - sizeof(pkt.opcode) - 1] = '\0';
        handle_rq (app, op_rrq, pkt.op.rq.filename, sock, addr);
        break;

    case op_wrq:
        // Write request
        if (app.opt.allow_wrq) {
            // Make sure the filename is null-terminated
            pkt.op.rq.filename[ior.result - sizeof(pkt.opcode) - 1] = '\0';
            handle_rq (app, op_wrq, pkt.op.rq.filename, sock, addr);
            break;
        }

    default:
        // Send error response
        iom::log::info ("Invalid request (opcode %d) from %s",
                        ntohs(pkt.opcode),
                        addr.to_string(true).c_str());
        sock.sendto (&tftp_err_pkt_illegal_op, tftp_err_pkt_illegal_op.size(), addr, nullptr, -1);
        break;
    }

    // Continue listening for new clients
    accept_request (app, sock, ior.buf, ior.size);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_session_done (appstate_t& app, tftp_session_t& session)
{
    std::lock_guard<std::mutex> lock (app.session_mutex);
    app.sessions.erase (session);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
/*
static std::map<std::string, std::string> parse_options (const tftp_pkt_t& pkt,
                                                         size_t pkt_size)
{
    std::map<std::string, std::string> retval;

    // Sanity check
    if (ntohs(pkt.opcode)!=op_rrq || ntohs(pkt.opcode)!=op_wrq)
        return retval;

    const char* const end = pkt.op.rq.filename + pkt_size - sizeof(pkt.opcode);
    const char* pos = (char*)pkt.op.rq.filename;
    if (pos >= end)
        return retval;

    auto len = strnlen (pos, end-pos);
    retval.emplace ("filename", std::string(pos, len));
    pos += len;

    while (pos < end) {
        auto len = strnlen (pos, end-pos);
        std::string key (pos, len);
        pos += len ? len : 1;

        len = strnlen (pos, end-pos);
        std::string val (pos, len);
        pos += len;

        retval.emplace (key, val);
    }

    return retval;
}
*/


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void handle_rq (appstate_t& app,
                       opcode_t op,
                       const std::string& filename,
                       iom::socket_connection& sock,
                       const iom::ip_addr& addr)
{
    std::lock_guard<std::mutex> lock (app.session_mutex);

    if (app.opt.max_clients  &&  app.sessions.size() >= app.opt.max_clients) {
        iom::log::info ("%s from %s - Too many clients",
                        (op==op_rrq ? "RRQ" : "WRQ"),
                        addr.to_string(true).c_str());
        sock.sendto (&tftp_err_pkt_server_busy, tftp_err_pkt_server_busy.size(), addr, nullptr, -1);
        return;
    }

    auto ib = app.sessions.emplace (sock.io_handler());
    tftp_session_t& session = const_cast<tftp_session_t&> (*ib.first);
    if (session.start(op,
                      app.opt.allow_overwrite,
                      app.opt.max_wrq_size,
                      filename,
                      addr,
                      [&app](tftp_session_t& session){on_session_done(app, session);}))
    {
        app.sessions.erase (ib.first);
        sock.sendto (&tftp_err_pkt_file_not_found, tftp_err_pkt_file_not_found.size(), addr, nullptr, -1);
    }
}
