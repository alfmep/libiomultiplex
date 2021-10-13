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
#ifndef EXAMPLES_UTFTPD_HPP
#define EXAMPLES_UTFTPD_HPP

#include <iomultiplex.hpp>
#include <string>
#include <memory>
#include <cstdint>

#include "utftp-common.hpp"


static constexpr int max_retry_count = 20;
static constexpr const char* default_tftp_root = "/srv/tftp";
static constexpr const int default_max_clients = 0; // 0 == No limit


struct tftp_server_session_t {
    iomultiplex::SocketConnection sock;
    iomultiplex::FileConnection file;
    iomultiplex::IpAddr client_addr;
    tftp_pkt_t pkt;
    size_t pkt_size;
    uint16_t block_num;
    int retry_cnt;
    std::shared_ptr<tftp_server_session_t> self;

    tftp_server_session_t (iomultiplex::IOHandler& ioh,
                           const iomultiplex::IpAddr& caddr)
        : sock (ioh),
          file (ioh),
          client_addr (caddr),
          block_num (0),
          retry_cnt (0)
        {
        };
};


#endif
