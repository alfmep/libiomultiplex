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
#ifndef EXAMPLES_UTFTP_COMMON_HPP
#define EXAMPLES_UTFTP_COMMON_HPP

#include <iomultiplex.hpp>
#include <string>
#include <atomic>
#include <memory>
#include <cstdint>


static constexpr size_t   tftp_data_block_size = 512;
static constexpr uint16_t tftp_default_port_num = 69;


enum opcode_t : uint16_t {
    op_rrq   = 1,
    op_wrq   = 2,
    op_data  = 3,
    op_ack   = 4,
    op_error = 5
};

struct tftp_pkt_t {
    uint16_t opcode;
    union {
        struct {
            char filename[514];
        }rq;
        struct {
            uint16_t block;
            char data[512];
        }dat;
        struct {
            uint16_t block;
        }ack;
        struct {
            uint16_t code;
            char msg[512];
        }err;
    }op;
};


#endif
