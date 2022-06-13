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
#include "tftp-session.hpp"
#include <sstream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;


namespace iom = iomultiplex;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
tftp_session_t::tftp_session_t (iomultiplex::iohandler_base& ioh)
    : sock (ioh),
      is_wrq (false),
      allow_overwrite (false),
      block (0),
      block_size (512),
      pkt (nullptr),
      retry_count (0),
      num_wrong_block (0),
      fd (-1)
{
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
tftp_session_t::~tftp_session_t ()
{
    stop ();
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int tftp_session_t::start (opcode_t op,
                           bool allow_overwrite,
                           size_t max_size,
                           const std::string& filename,
                           const iomultiplex::IpAddr& addr,
                           std::function<void (tftp_session_t& sess)> on_session_done)
{
    is_wrq = op == op_wrq;
    this->allow_overwrite = allow_overwrite;
    this->filename = filename;

    block = 0;
    block_size = sizeof (tftp_pkt_t::op.dat.data);
    total_write_size = 0;
    max_write_size = max_size;
    session_done_cb = on_session_done;

    buf.reset (new char[tftp_min_packet_size + block_size]);
    pkt = (tftp_pkt_t*) buf.get ();

    // Open the file
    //
    if (open_file(addr)) {
        return -1;
    }

    // Open the client socket
    //
    if (sock.open(addr.family(), SOCK_DGRAM)) {
        iom::Log::info ("%s from %s - Error opening client socket: %s",
                        (is_wrq ? "WRQ" : "RRQ"),
                        addr.to_string(true).c_str(),
                        strerror(errno));
        close_file ();
        return -1;
    }
    // Bind the client socket to the client address
    sock.connect (addr, nullptr);

    std::stringstream ss;
    ss << dynamic_cast<const iom::IpAddr&>(sock.addr()).port() << ':' << addr.to_string(true).c_str();
    sess_id = ss.str ();

    iom::Log::info ("%s session %s, file %s, block size %u",
                    (is_wrq ? "WRQ" : "RRQ"),
                    sess_id.c_str(),
                    filename.c_str(),
                    block_size);

    if (is_wrq)
        send_ack ();
    else
        send_block ();

    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int tftp_session_t::open_file (const iomultiplex::IpAddr& addr)
{
    // Sanity check
    //
    if (filename.empty() ||                       // Emtpy filename not allowed
        filename.find("/")==0 ||                  // Absolute path not allowed
        filename.find("../")==0 ||                // Relative path not allowed
        filename.find("/../")!=std::string::npos) // Relative path not allowed
    {
        iom::Log::info ("%s from %s - Illegal file name '%s'",
                        (is_wrq ? "WRQ" : "RRQ"),
                        addr.to_string(true).c_str(),
                        filename.c_str());
        return -1;
    }

    // Check the file
    // We only allow regular files to be read/overwritten
    //
    struct stat sb;
    if (stat(filename.c_str(), &sb)  ||  !S_ISREG(sb.st_mode)) {
        iom::Log::info ("%s from %s - Can't open file '%s'",
                        (is_wrq ? "WRQ" : "RRQ"),
                        addr.to_string(true).c_str(),
                        filename.c_str());
        return -1;
    }
    else if (is_wrq && !allow_overwrite) {
        iom::Log::info ("WRQ from %s - File already exists '%s'",
                        addr.to_string(true).c_str(),
                        filename.c_str());
        return -1;
    }

    // Open the file
    //
    if (is_wrq)
        fd = open (filename.c_str(),
                   O_WRONLY|O_CREAT|O_TRUNC,
                   S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
    else
        fd = open (filename.c_str(), O_RDONLY);
    if (fd < 0) {
        iom::Log::info ("%s from %s - Can't open file '%s'",
                        (is_wrq ? "WRQ" : "RRQ"),
                        addr.to_string(true).c_str(),
                        filename.c_str());
        return -1;
    }

    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void tftp_session_t::close_file ()
{
    if (fd >= 0) {
        close (fd);
        fd = -1;
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void tftp_session_t::stop ()
{
    sock.close ();
    close_file ();
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void tftp_session_t::send_error_and_end_session (uint16_t err_code,
                                                 const std::string& err_msg)
{
    close_file ();
    pkt->opcode = htons (op_error);
    pkt->op.err.code = htons (err_code);
    strcpy (pkt->op.err.msg, err_msg.c_str());
    sock.write (pkt, pkt->size(), [this](iom::io_result_t& ior)->bool{
        sock.close ();
        if (session_done_cb)
            session_done_cb (*this);
        return false;
    }, 500);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void tftp_session_t::handle_opcode_error (tftp_pkt_t& pkt)
{
    if (ntohs(pkt.opcode) == op_error) {
        // Make sure error message is null-terminated
        pkt.op.err.msg[sizeof(pkt.op.err.msg)-1] = '\0';
        iom::Log::info ("Session %s, TFTP error received: %d (%s)",
                        sess_id.c_str(), (int)ntohs(pkt.op.err.code), pkt.op.err.msg);
        stop ();
        if (session_done_cb)
            session_done_cb (*this);
    }
    else {
        // Wrong packet type
        iom::Log::info ("Session %s, invalid TFTP opcode %d received",
                        sess_id.c_str(), (int)ntohs(pkt.opcode));
        send_error_and_end_session (4, "Illegal TFTP operation");
    }
}



//
//   R R Q
//


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void tftp_session_t::send_block ()
{
    auto bytes = read (fd, &pkt->op.dat.data, block_size);
    if (bytes < 0) {
        iom::Log::info ("Session %s, file read error: %s",
                        sess_id.c_str(), strerror(errno));
        send_error_and_end_session (0, "File read error");
        return;
    }

    ++block;

    pkt->opcode = htons (op_data);
    pkt->op.dat.block = htons (block);
    retry_count = 0;
    num_wrong_block = 0;
    sock.write (pkt, pkt->size()+bytes, nullptr, 500);
    sock.read (&ack, sizeof(ack), [this, bytes](iom::io_result_t& ior)->bool{
        on_ack (ior, bytes);
        return false;
    }, 500);
}


//------------------------------------------------------------------------------
// Receive error or received packet too small
//------------------------------------------------------------------------------
void tftp_session_t::handle_ack_error (iomultiplex::io_result_t& ior, size_t bytes_in_block)
{
    switch (ior.errnum) {
    case ECANCELED:
        // Server shutting down, just stop the session
        stop ();
        break;

    case ETIMEDOUT:
        // Timeout waiting for ACK
        if (++retry_count > 10) {
            // Too many resend attempts
            iom::Log::info ("Session %s, timeout waiting for ACK #%u, stop session",
                            sess_id.c_str(), block);
            send_error_and_end_session (0, "Transmit error");
        }else{
            // Resend data and continue waiting for ACK
            iom::Log::debug ("Session %s, timeout waiting for ACK, resend block #%u",
                             sess_id.c_str(), block);
            sock.write (pkt, pkt->size()+bytes_in_block, nullptr, 500);
            sock.read (&ack, sizeof(ack), [this, bytes_in_block](iom::io_result_t& ior)->bool{
                on_ack (ior, bytes_in_block);
                return false;
            }, 500 * retry_count); // Increase timeout 500ms for each retry
        }
        break;

    default:
        if (ior.result < 0) {
            // Misc error
            iom::Log::info ("Session %s, error receiving ACK #%u, stop session. Error: %s",
                            sess_id.c_str(), block, strerror(ior.errnum));
            send_error_and_end_session (0, "Transmit error");
        }
        else if (ior.result == 0) {
            // Connection terminated by peer
            iom::Log::info ("Session %s, connection reset by peer", sess_id.c_str());
            stop ();
            if (session_done_cb)
                session_done_cb (*this);
        }else{
            // Invalid packet size
            iom::Log::info ("Session %s, invalid TFTP packet received, stop session.",
                            sess_id.c_str());
            send_error_and_end_session (4, "Illegal TFTP operation");
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void tftp_session_t::on_ack (iomultiplex::io_result_t& ior, size_t bytes_in_block)
{
    // Check for packet receive error (receive error or packet too small)
    //
    if (ior.result < tftp_min_packet_size) {
        handle_ack_error (ior, bytes_in_block);
        return;
    }

    // Check opcode
    //
    if (ntohs(ack.opcode) != op_ack) {
        handle_opcode_error (ack);
        return;
    }

    if (ntohs(ack.op.ack.block) == block) {
        //
        // Ack ok
        //
        if (bytes_in_block < block_size) {
            // All blocks sent
            iom::Log::debug ("Session %s done", sess_id.c_str());
            stop ();
            if (session_done_cb)
                session_done_cb (*this);
        }else{
            // Send next data block
            send_block ();
        }
    }else{
        //
        // Wrong ACK #
        //
        if (++num_wrong_block > 20) {
            iom::Log::info ("Session %s, error: too many wrong consecutive ACKs",
                            sess_id.c_str());
            send_error_and_end_session (0, "ACK error");
        }else{
            // Continue to wait for ACK
            sock.read (&ack, sizeof(ack), [this, bytes_in_block](iom::io_result_t& ior)->bool{
                on_ack (ior, bytes_in_block);
                return false;
            }, 500 * retry_count);
        }
    }
}



//
//   W R Q
//


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void tftp_session_t::send_ack ()
{
    retry_count = 0;
    num_wrong_block = 0;
    ack.opcode = htons (op_ack);
    ack.op.ack.block = htons (block);
    sock.write (&ack, tftp_min_packet_size, nullptr, 500);
    sock.read (pkt, tftp_min_packet_size+block_size, [this](iom::io_result_t& ior)->bool{
        on_block (ior);
        return false;
    }, 500);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void tftp_session_t::on_block (iomultiplex::io_result_t& ior)
{
    uint16_t expected_block = block + 1;

    // Check for packet receive error (receive error or packet too small)
    //
    if (ior.result < tftp_min_packet_size) {
        handle_block_error (ior, expected_block);
        return;
    }

    // Verify opcode
    //
    if (ntohs(pkt->opcode) != op_data) {
        handle_opcode_error (*pkt);
        return;
    }

    // Verify block number
    //
    if (ntohs(pkt->op.dat.block) != expected_block) {
        iom::Log::debug ("Session %s, error: wrong data block number", sess_id.c_str());
        if (++num_wrong_block > 20) {
            iom::Log::info ("Session %s, error: too many wrong consecutive wrong block numbers",
                            sess_id.c_str());
            send_error_and_end_session (0, "DATA error");
        }else{
            // Resend last ack and wait for data
            sock.write (&ack, tftp_min_packet_size, nullptr, 500);
            sock.read (pkt, tftp_min_packet_size+block_size, [this](iom::io_result_t& ior)->bool{
                on_block (ior);
                return false;
            }, 500 * retry_count);
        }
        return;
    }

    // Handle the data block
    //
    handle_new_block (ior);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void tftp_session_t::handle_new_block (iomultiplex::io_result_t& ior)
{
    size_t bytes = ior.result - tftp_min_packet_size;

    // Update our last received block number
    ++block;

    // Check maximum size for written files
    if (max_write_size &&
        (total_write_size + bytes) > max_write_size)
    {
        iom::Log::info ("Session %s, max file size exceeded", sess_id.c_str());
        send_error_and_end_session (3, "Disk full or allocation exceeded");
        return;
    }

    // Write to file
    if (bytes  &&  write(fd, pkt->op.dat.data, bytes) != (ssize_t)bytes) {
        iom::Log::info ("Session %s, file write error: %s",
                        sess_id.c_str(), strerror(errno));
        send_error_and_end_session (0, "File write error");
        return;
    }
    total_write_size += bytes;

    // Check if this is the last data block
    if (bytes < block_size) {
        // Send last ack
        ack.opcode = htons (op_ack);
        ack.op.ack.block = htons (block);
        sock.write (&ack, tftp_min_packet_size, [this](iom::io_result_t& ior)->bool{
            // End the session after the last ACK is sent (ignore send error)
            iom::Log::debug ("Session %s done", sess_id.c_str());
            stop ();
            if (session_done_cb)
                session_done_cb (*this);
            return false;
        }, 500);
    }else{
        // Send ACK and wait for next data block
        send_ack ();
    }
}


//------------------------------------------------------------------------------
// Receive error or received packet too small
//------------------------------------------------------------------------------
void tftp_session_t::handle_block_error (iomultiplex::io_result_t& ior,
                                         uint16_t expected_block)
{
    switch (ior.errnum) {
    case ECANCELED:
        // Server shutting down, just stop the session
        stop ();
        break;

    case ETIMEDOUT:
        // Timeout waiting for data
        if (++retry_count > 10) {
            // Too many resend attempts
            iom::Log::info ("Session %s, timeout waiting for block #%u, stop session",
                            sess_id.c_str(), expected_block);
            send_error_and_end_session (0, "Receive error");
        }else{
            // Resend ACK and continue waiting for data
            iom::Log::debug ("Session %s, timeout waiting for data, resend ACK #%u",
                             sess_id.c_str(), block);
            sock.write (&ack, tftp_min_packet_size, nullptr, 500);
            sock.read (pkt, tftp_min_packet_size+block_size, [this](iom::io_result_t& ior)->bool{
                on_block (ior);
                return false;
            }, 500 * retry_count); // Increase timeout 500ms for each retry
        }
        break;

    default:
        if (ior.result < 0) {
            // Misc error
            iom::Log::info ("Session %s, error receiving block #%u, stop session. Error: %s",
                            sess_id.c_str(), expected_block, strerror(ior.errnum));
            send_error_and_end_session (0, "Transmit error");
        }
        else if (ior.result == 0) {
            // Connection terminated by peer
            iom::Log::info ("Session %s, connection reset by peer", sess_id.c_str());
            stop ();
            if (session_done_cb)
                session_done_cb (*this);
        }else{
            // Invalid packet size
            iom::Log::info ("Session %s, invalid TFTP packet received, stop session.",
                            sess_id.c_str());
            send_error_and_end_session (4, "Illegal TFTP operation");
        }
    }
}
