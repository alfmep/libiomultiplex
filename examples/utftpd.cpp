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
#include <thread>
#include <atomic>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <libgen.h>

#include "logging.hpp"
#include "utftp-common.hpp"
#include "utftpd.hpp"
#include "utftpd-options.hpp"


using namespace std;
namespace iom = iomultiplex;


static constexpr const int tftp_err_not_found = 1;
static constexpr const int tftp_err_io        = 2;
static constexpr const int tftp_err_opcode    = 3;
static constexpr const int tftp_err_busy      = 4;


static std::vector<std::unique_ptr<iom::IOHandler>> workers;
static std::mutex server_done;
static std::atomic_int num_clients (0);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void cmd_signal_handler (int sig)
{
    // Signal the TFTP server to stop
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
// Initialize a TFTP error packet
//------------------------------------------------------------------------------
static size_t init_err_pkt (tftp_pkt_t& pkt, const int16_t code, const char* msg)
{
    pkt.opcode = htons (op_error);
    pkt.op.err.code = (uint16_t) htons (code);
    strncpy (pkt.op.err.msg, msg, sizeof(pkt.op.err.msg)-1);
    pkt.op.err.msg[sizeof(pkt.op.err.msg)-1] = '\0';
    return sizeof(pkt.opcode) + sizeof(pkt.op.err.code) + strlen(pkt.op.err.msg) + 1;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void end_session (tftp_server_session_t& sess)
{
    sess.file.close ();
    sess.sock.close ();
    sess.self.reset (); // Free session resources
    --num_clients;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_file_read (tftp_server_session_t& sess, iom::io_result_t& ior)
{
    if (ior.result >= 0) {
        // Send data block to client
        ++sess.block_num;
        sess.pkt.opcode = htons (op_data);
        sess.pkt.op.dat.block = htons (sess.block_num);
        sess.pkt_size = sizeof(sess.pkt.opcode) + sizeof(sess.pkt.op.dat.block) + ior.result;
        sess.retry_cnt = 0;
        sess.sock.write (&sess.pkt, sess.pkt_size, nullptr, 500);
    }else{
        // Send error to client
        iom::Log::info ("%s: Error reading file: %s",
                        sess.client_addr.to_string(true).c_str(),
                        strerror(ior.errnum));
        sess.pkt_size = init_err_pkt (sess.pkt, tftp_err_io, "File I/O error");
        sess.sock.write (&sess.pkt, sess.pkt_size, nullptr, 500);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_tx (tftp_server_session_t& sess, iom::io_result_t& ior)
{
    if (ntohs(sess.pkt.opcode) == op_error) {
        // Error packet sent, ignore send result, session done.
        end_session (sess);
        return;
    }

    if (ior.result == (ssize_t)ior.size) {
        //
        // Packet sent ok, wait for ACK
        //
        auto timeout = sess.retry_cnt ? sess.retry_cnt*500 : 500;
        sess.sock.read (&sess.pkt, sizeof(sess.pkt.opcode)+sizeof(sess.pkt.op.ack), nullptr, timeout);
    }else{
        //
        // Error sending packet
        //
        if (ior.errnum==ETIMEDOUT && ++sess.retry_cnt<max_retry_count) {
            // Timeout, re-send packet
            /*
            iom::Log::debug ("%s: Timeout sending block #%d, re-send packet",
                             sess.client_addr.to_string(true).c_str(),
                             ntohs(sess.pkt.op.dat.block));
            */
            sess.sock.write (ior.buf, ior.size, nullptr, 500*sess.retry_cnt);
        }else{
            // Error sending packet, session done
            iom::Log::debug ("%s: Error sending block #%d, closing connection",
                             sess.client_addr.to_string(true).c_str(),
                             ntohs(sess.pkt.op.dat.block));
            end_session (sess);
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void on_rx (tftp_server_session_t& sess, iom::io_result_t& ior)
{
    bool resend = false;
    if (ior.result == (ssize_t)ior.size) {
        // Check opcode
        if (sess.pkt.opcode != ntohs(op_ack)) {
            sess.pkt_size = init_err_pkt (sess.pkt, tftp_err_opcode, "Invalid opcode");
            sess.sock.write (&sess.pkt, sess.pkt_size, nullptr, 500);
            return;
        }
        // Check block number
        if (ntohs(sess.pkt.op.ack.block) == sess.block_num) {
            // Is this the last block ?
            if (sess.pkt_size == sizeof(sess.pkt)) {
                // No, read next block from file
                sess.file.read (sess.pkt.op.dat.data,
                                sizeof(sess.pkt.op.dat.data),
                                nullptr, 3000);
                return;
            }
        }else{
            // Block number mismatch
            if (++sess.retry_cnt < max_retry_count)
                resend = true;
        }
    }else{
        // Error receiving packet
        if (ior.errnum==ETIMEDOUT && ++sess.retry_cnt<max_retry_count)
            resend = true; // Timeout, re-send packet
    }

    if (resend) {
        sess.pkt.opcode = htons (op_data);
        sess.pkt.op.dat.block = htons (sess.block_num);
        sess.sock.write (&sess.pkt, sess.pkt_size, nullptr, 500*sess.retry_cnt);
    }else{
        end_session (sess);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool check_file_dir (const string& filename, const appargs_t& opt)
{
    bool retval = false;

    if (strstr(filename.c_str(), ".."))
        return false; // No relative elements allowed in requested path

    string req_file = opt.directory + filename;
    auto* path = realpath (req_file.c_str(), nullptr);
    if (!path)
        return false;
    if (strstr(path, opt.directory.c_str()) == path)
        retval = true;
    free (path);

    return retval;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void start_session (tftp_server_session_t& sess,
                           const appargs_t& opt,
                           const std::string& filename)
{
    int current_num_clients = num_clients.fetch_add(1) + 1;

    // Initialize session socket that will serve the client
    if (sess.sock.open(AF_INET, SOCK_DGRAM) == 0)
        // "Lock" to the client address:port
        sess.sock.connect (sess.client_addr, nullptr, -1);
    if (errno) {
        iom::Log::warning ("Unable to create client socket: %s", strerror(errno));
        end_session (sess);
        return;
    }

    // Set default I/O callbacks for the session socket
    sess.sock.default_tx_callback ([&sess](iom::io_result_t& ior)->bool{
            // Called when a TX packet is sent
            on_tx (sess, ior);
            return false;
        });
    sess.sock.default_rx_callback ([&sess](iom::io_result_t& ior)->bool{
            // Called when an RX packet is received
            on_rx (sess, ior);
            return false;
        });

    // Check server limits
    if (opt.max_clients>0  &&  current_num_clients>opt.max_clients) {
        // Too many concurrent clients
        iom::Log::debug ("%s: Error - too many clients",
                         sess.client_addr.to_string(true).c_str());
        // Send error message and terminate session
        sess.pkt_size = init_err_pkt (sess.pkt, tftp_err_busy, "Server busy");
        sess.sock.write (&sess.pkt, sess.pkt_size, nullptr, 500);
        return;
    }

    // Open the requested file
    if (check_file_dir(filename, opt) && sess.file.open(filename, O_RDONLY) == 0) {
        // Set the file read callback
        sess.file.default_rx_callback ([&sess](iom::io_result_t& ior)->bool{
                // Called when a chunk of the file is read
                on_file_read (sess, ior);
                return false;
            });
        sess.block_num = 0;
        // Start reading file
        sess.file.read (sess.pkt.op.dat.data, sizeof(sess.pkt.op.dat.data), nullptr, 3000);
    }else{
        // Can't open file, send error packet and terminate session
        iom::Log::debug ("%s: Error opening file '%s': %s",
                         sess.client_addr.to_string(true).c_str(),
                         filename.c_str(), strerror(errno));
        sess.pkt_size = init_err_pkt (sess.pkt, tftp_err_not_found, "File not found");
        sess.sock.write (&sess.pkt, sess.pkt_size, nullptr, 500);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void handle_new_request (const appargs_t& opt,
                                iom::io_result_t& ior,
                                iom::SocketConnection& sock,
                                const iom::IpAddr& addr)
{
    tftp_pkt_t& pkt = *((tftp_pkt_t*)ior.buf);

    // Check for an incoming RRQ packet
    //
    if (ior.result > (ssize_t)sizeof(pkt.opcode)  &&  ntohs(pkt.opcode)==op_rrq) {
        // Make sure the filename is null-terminated
        pkt.op.rq.filename[ior.result - sizeof(pkt.opcode) - 1] = '\0';

        if (iom::Log::priority() >= LOG_DEBUG)
            iom::Log::debug ("RRQ %s - '%s'",
                             addr.to_string(true).c_str(),
                             pkt.op.rq.filename);

        // Create a new TFTP session
        try {
            auto sess = std::make_shared<tftp_server_session_t> (ior.conn.io_handler(), addr);
            sess->self = sess;
            // Start the TFTP session
            start_session (*sess, opt, std::string(pkt.op.rq.filename));
        }
        catch (...) {
            iom::Log::error ("Unable to allocate new TFTP session");
        }
    }else{
        // I/O error or not an RRQ packet
        if (ior.errnum) {
            if (ior.errnum == ECANCELED) {
                sock.close ();
            }else{
                iom::Log::debug ("Error waiting for client request on socket #%d: ",
                                 sock.handle(), strerror(ior.errnum));
            }
        }else{
            iom::Log::debug ("Ignoring invalid TFTP request from %s", addr.to_string(true).c_str());
        }
    }

    // Continue listen for new client requests
    //
    if (ior.errnum == 0) {
        sock.recvfrom (ior.buf, ior.size, [&opt, &sock](iom::io_result_t& ior, const iom::SockAddr& addr)->bool{
                // Called by the socket listener
                // on a new incomming connection
                handle_new_request (opt, ior, sock, dynamic_cast<const iom::IpAddr&>(addr));
                return false;
            }, -1);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool set_privileges (const appargs_t& opt)
{
    // Set user id
    if (opt.uid != getuid()) {
        iom::Log::debug ("Setting user id to %u", (unsigned(opt.uid)));
        if (setuid(opt.uid)) {
            iom::Log::error ("Unable to set user id: %s", strerror(errno));
            return false;
        }
    }

    // Set group id
    if (opt.gid != getgid()) {
        iom::Log::debug ("Setting group id to %u", (unsigned(opt.gid)));
        if (setgid(opt.gid)) {
            iom::Log::error ("Unable to set group id: %s", strerror(errno));
            return false;
        }
    }

    return true;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    // Parse arguments
    appargs_t opt (argc, argv);

    // Configure logging
    iom::Log::set_callback (example_log_callback);
    //iom::Log::priority (LOG_INFO);
    iom::Log::priority (LOG_DEBUG);

    // Change working directory to the tftp root
    if (chdir(opt.directory.c_str())) {
        iom::Log::error ("Unable to set working directory: %s", strerror(errno));
        exit (1);
    }

    // Exit gracefully on CTRL-C (SIGINT)
    server_done.lock ();
    init_signal_handler ();

    // Create the I/O workers
    std::vector<iom::SocketConnection> listeners;
    for (int i=0; i<opt.num_workers; ++i) {
        // Create a new I/O handler
        workers.emplace_back (std::make_unique<iom::IOHandler>(SIGRTMIN+i));
        auto& ioh = *workers.back();

        // Create new socket listener
        listeners.emplace_back (ioh);
        auto& sock = listeners.back ();

        if (sock.open(AF_INET, SOCK_DGRAM) == 0) {
            // Set SO_REUSEPORT so all worker threads
            // can bind to the same local port
            if (sock.setsockopt(SO_REUSEPORT, 1) == 0)
                sock.bind (opt.bind_addr);
        }
        if (errno) {
            iom::Log::error ("Unable to open/bind socket: %s", strerror(errno));
            exit (1);
        }
        // Start the I/O handler in a new thread
        ioh.run (true);
    }

    // Server ready to serve, but first set privileges
    if (!set_privileges(opt))
        exit (1);

    iom::Log::info ("Serving files from %s, directory %s",
                    opt.bind_addr.to_string(true).c_str(),
                    opt.directory.c_str());
    opt.directory.append ("/");

    // Start receiving requests on all listeners
    iom::BufferPool pool (sizeof(tftp_server_session_t), listeners.size());
    for (auto& sock : listeners) {
        sock.recvfrom (pool.get(),
                       pool.buf_size(),
                       [&opt, &sock](iom::io_result_t& ior,
                                     const iom::SockAddr& addr)->bool
                       {
                           // Called by the socket listener
                           // on a new incomming connection
                           handle_new_request (opt,
                                               ior,
                                               sock,
                                               dynamic_cast<const iom::IpAddr&>(addr));
                           return false;
                       },
                       -1);
    }
    // All listeners started and waiting for clients to connect

    // Wait for the TFTP server to terminate (on SIGINT)
    server_done.lock ();

    // Stop all I/O workers
    for (auto& ioh : workers) {
        ioh->stop ();
        ioh->join ();
    }

    iom::Log::info ("Stopped");
    return 0;
}
