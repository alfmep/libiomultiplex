/*
 * Copyright (C) 2022,2023,2025 Dan Arrhenius <dan@ultramarin.se>
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
#ifndef EXAMPLES_TFTP_SESSION_HPP
#define EXAMPLES_TFTP_SESSION_HPP

#include "tftp.hpp"
#include <iomultiplex.hpp>
#include <string>
#include <functional>


class tftp_session_t {
public:
    tftp_session_t (iomultiplex::iohandler_base& ioh);
    ~tftp_session_t ();

    int start (opcode_t op,
               bool allow_overwrite,
               size_t max_size,
               const std::string& filename,
               const iomultiplex::ip_addr& addr,
               std::function<void (tftp_session_t& sess)> on_session_done);
    void stop ();


private:
    int open_file (const iomultiplex::ip_addr& addr);
    void close_file ();
    void send_error_and_end_session (uint16_t err_code,
                                     const std::string& err_msg);

    void handle_opcode_error (tftp_pkt_t& pkt);

    // R R Q
    void send_block ();
    void on_ack (iomultiplex::io_result_t& ior, size_t bytes_in_block);
    void handle_ack_error (iomultiplex::io_result_t& ior, size_t bytes_in_block);

    // W R Q
    void send_ack ();
    void on_block (iomultiplex::io_result_t& ior);
    void handle_new_block (iomultiplex::io_result_t& ior);
    void handle_block_error (iomultiplex::io_result_t& ior, uint16_t expected_block);

    iomultiplex::socket_connection sock;
    std::function<void (tftp_session_t& sess)> session_done_cb;
    std::string filename;
    std::string sess_id;

    bool is_wrq;
    bool allow_overwrite;

    uint16_t block;
    size_t block_size;
    size_t max_write_size;
    size_t total_write_size;
    tftp_pkt_t* pkt;
    std::unique_ptr<char[]> buf;
    tftp_pkt_t ack;
    int retry_count;
    int num_wrong_block;

    int fd;
};


#endif
