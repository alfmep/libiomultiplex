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
#ifndef EXAMPLES_IOM_TFTPD_OPTIONS_HPP
#define EXAMPLES_IOM_TFTPD_OPTIONS_HPP

#include <iomultiplex.hpp>
#include <string>
#include <unistd.h>
#include <sys/types.h>



//------------------------------------------------------------------------------
//  T Y P E S
//------------------------------------------------------------------------------
struct appargs_t {
    appargs_t ();
    int parse_args (int argc, char* argv[]);
    void print_usage (std::ostream& out);

    iomultiplex::IpAddr bind_addr;
    std::string tftproot;
    std::string pid_file;
    std::string cert_file;
    std::string privkey_file;
    std::string user;
    std::string group;
    size_t max_wrq_size;
    int num_workers;
    size_t max_clients;
    bool foreground;
    bool log_to_stdout;
    bool allow_wrq;
    bool allow_overwrite;
    bool dtls;
    bool verbose;
};


#endif
