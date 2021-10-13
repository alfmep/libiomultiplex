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
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <cerrno>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

#include "utftp-common.hpp"
#include "utftpd.hpp"
#include "utftpd-options.hpp"


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool get_uid (const char* optarg, uid_t& uid)
{
    struct passwd* pwd = nullptr;
    try {
        pwd = getpwuid ((uid_t)std::stol(optarg));
    }
    catch (...) {
        pwd = getpwnam (optarg);
    }
    if (!pwd)
        return false;

    uid = pwd->pw_uid;
    return true;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static bool get_gid (const char* optarg, gid_t& gid)
{
    struct group* pwd = nullptr;
    try {
        pwd = getgrgid ((uid_t)std::stol(optarg));
    }
    catch (...) {
        pwd = getgrnam (optarg);
    }
    if (!pwd)
        return false;

    gid = pwd->gr_gid;
    return true;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void appargs_t::print_usage_and_exit (std::ostream& out, int exit_code)
{
    out << std::endl
        << "Usage: " << program_invocation_short_name << " [OPTIONS]" << std::endl
        << std::endl
        << "  -d, --dir=<directory>       Serve files from this directory. Default is: " << default_tftp_root << '.' << std::endl
        << "  -b, --bind=<address>        Bind the tftp server to this address[:port]." << std::endl
        << "                              Default address is 0.0.0.0:" << tftp_default_port_num << " (any local IPv4 address)." << std::endl
        << "  -p, --port=<port>           Bind the tftp server to this port number." << std::endl
        << "                              This overrides any port in option --bind." << std::endl
        << "  -m, --max-clients=<num>     Maximum number of concurrent clients per worker thread." << std::endl
        << "                              A value of zero means no limit." << " Default is "<< default_max_clients << '.' << std::endl
        << "  -w, --worker-threads=<num>  Number of worker threads. Default is 1." << std::endl
        << "  -u, --user=<user_id>        Set the user id of the server." << std::endl
        << "  -g, --group=<group_id>      Set the group id of the server." << std::endl
        << "  -h, --help                  Print this help message." << std::endl
        << std::endl;

        exit (exit_code);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
appargs_t::appargs_t (int argc, char* argv[])
    : directory (default_tftp_root),
      num_workers (1),
      max_clients (default_max_clients)
{
    static struct option long_options[] = {
        { "dir",            required_argument, 0, 'd'},
        { "bind",           required_argument, 0, 'b'},
        { "port",           required_argument, 0, 'p'},
        { "max-clients",    required_argument, 0, 'm'},
        { "worker-threads", required_argument, 0, 'w'},
        { "user",           required_argument, 0, 'u'},
        { "group",          required_argument, 0, 'g'},
        { "help",           no_argument,       0, 'h'},
        { 0, 0, 0, 0}
    };
    static const char* arg_format = "d:b:p:m:w:u:g:h";
    int bind_port = -1;
    size_t len;

    uid = getuid ();
    gid = getgid ();

    while (1) {
        int c = getopt_long (argc, argv, arg_format, long_options, NULL);
        if (c == -1)
            break;
        switch (c) {
        case 'd':
            len = strlen (optarg);
            // Remove trailing / if not root dir.
            if (len>1 && optarg[len-1]=='/')
                directory = std::string (optarg, len-1);
            else
                directory = optarg;
            break;

        case 'b':
            if (!bind_addr.parse(optarg) || bind_addr.family()!=AF_INET) {
                std::cerr << "Error: Invalid IPv4 address and/or port number to argument '--bind'" << std::endl;
                exit (1);
            }
            break;

        case 'p':
            bind_port = atoi (optarg);
            if (bind_port<=0 || bind_port>65535) {
                std::cerr << "Error: Invalid port number to argument '--port'" << std::endl;
                exit (1);
            }
            break;

        case 'm':
            max_clients = atoi (optarg);
            if (max_clients < 0) {
                std::cerr << "Error: Invalid number of maximum clients to argument '--max-clients'" << std::endl;
                exit (1);
            }
            break;

        case 'w':
            num_workers = atoi (optarg);
            if (num_workers<=0 || num_workers>(SIGRTMAX-SIGRTMIN+1)) {
                std::cerr << "Error: Invalid value to argument '--worker-threads'" << std::endl;
                exit (1);
            }
            break;

        case 'u':
            if (!get_uid(optarg, uid)) {
                std::cerr << "Error: No such user" << std::endl;
                exit (1);
            }
            break;

        case 'g':
            if (!get_gid(optarg, gid)) {
                std::cerr << "Error: No such group" << std::endl;
                exit (1);
            }
            break;

        case 'h':
            print_usage_and_exit (std::cout, 0);
            break;

        default:
            print_usage_and_exit (std::cerr, 1);
            break;
        }
    }
    if (optind < argc) {
        std::cerr << "Error: Invalid argument" << std::endl;
        print_usage_and_exit (std::cerr, 1);
    }

    // Get the canonical server root path
    //
    auto* path = realpath (directory.c_str(), nullptr);
    if (path) {
        directory = path;
        free (path);
    }else{
        std::cerr << "Unable to serve files from directory '"
                  << directory.c_str() << "': "
                  << strerror(errno)
                  << std::endl;
        exit (1);
    }

    // Make sure that tftp root is a directory
    //
    struct stat st;
    if (stat(directory.c_str(), &st)) {
        std::cerr << "Error accessing directory '" << directory << "': " << strerror(errno) << std::endl;
        exit (1);
    }
    if ((st.st_mode & S_IFMT) != S_IFDIR) {
        std::cerr << "Error: Not a directory: '" << directory << "'" << std::endl;
        exit (1);
    }

    // Set port number
    //
    if (bind_port != -1)
        bind_addr.port ((uint16_t)bind_port);
    else if (!bind_addr.port())
        bind_addr.port (tftp_default_port_num);
}
