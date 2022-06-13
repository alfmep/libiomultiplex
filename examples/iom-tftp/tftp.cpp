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
#include "tftp.hpp"
#include <cstring>
#include <arpa/inet.h>


tftp_pkt_t tftp_err_pkt_server_busy    (op_error, 0, "Server busy");
tftp_pkt_t tftp_err_pkt_file_not_found (op_error, 1, "File not found");
tftp_pkt_t tftp_err_pkt_illegal_op     (op_error, 4, "Illegal TFTP operation");


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
tftp_pkt_t::tftp_pkt_t (uint16_t code, uint16_t error_code, const char* str)
{
    if ((code==op_rrq || code==op_wrq) && str) {
        strncpy (op.rq.filename, str, sizeof(op.rq.filename)-1);
        op.rq.filename[sizeof(op.rq.filename)-1] = '\0';
    }
    else if (code==op_error && str) {
        op.err.code = htons (error_code);
        strncpy (op.err.msg, str, sizeof(op.err.msg)-1);
        op.err.msg[sizeof(op.err.msg)-1] = '\0';
    }
    opcode = htons (code);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
tftp_pkt_t::tftp_pkt_t (uint16_t code, const char* str)
    : tftp_pkt_t (code, 0, str)
{
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
size_t tftp_pkt_t::size ()
{
    size_t s = tftp_min_packet_size;
    switch (ntohs(opcode)) {
    case op_rrq:
    case op_wrq:
        s = sizeof(opcode) + strlen(op.rq.filename) + 1;
        if (s > sizeof(tftp_pkt_t))
            s = sizeof(tftp_pkt_t);
        break;

    case op_data:
        s = sizeof(opcode) + sizeof(op.dat.block);
        break;

    case op_ack:
        s = sizeof(opcode) + sizeof(op.ack.block);
        break;

    case op_error:
        s = sizeof(opcode) + sizeof(op.err.code);
        s += strlen(op.err.msg) + 1;
        if (s > sizeof(tftp_pkt_t))
            s = sizeof(tftp_pkt_t);
        break;
    }
    return s;
}
