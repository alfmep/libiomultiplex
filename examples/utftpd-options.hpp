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
#ifndef EXAMPLES_UTFTPD_OPTIONS_HPP
#define EXAMPLES_UTFTPD_OPTIONS_HPP

#include <iostream>
#include <string>
#include <iomultiplex.hpp>
#include <unistd.h>
#include <sys/types.h>


//------------------------------------------------------------------------------
//  T Y P E S
//------------------------------------------------------------------------------
struct appargs_t {
    appargs_t (int argc, char* argv[]);
    void print_usage_and_exit (std::ostream& out, int exit_code);

    iomultiplex::IpAddr bind_addr;
    std::string directory;
    int num_workers;
    int max_clients;
    uid_t uid;
    gid_t gid;
};

#endif
