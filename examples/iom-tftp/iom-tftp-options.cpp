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
#include "iom-tftpd-options.hpp"
#include "tftp.hpp"

#include <string>
#include <iostream>
#include <stdexcept>
#include <sys/types.h>
#include <getopt.h>


static constexpr const char* default_tftp_root = "/srv/tftp";
//static constexpr const char* default_pid_file = RUNSTATEDIR "/iom-tftpd.pid";
static constexpr const size_t default_max_clients = 0; // 0 == No limit


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static size_t parse_file_size (const char* arg)
{
    size_t pos;
    size_t retval = 0;

    retval = std::stoul (arg, &pos, 10);

    auto arg_len = strlen (arg);
    if (pos != arg_len) {
        if (pos != arg_len-1)
            throw std::invalid_argument ("EINVAL");
        switch (arg[pos]) {
        case 'K':
            retval *= 1024;
            break;

        case 'M':
            retval *= 1024 * 1024;
            break;

        case 'G':
            retval *= 1024 * 1024 * 1024;
            break;

        default:
            throw std::invalid_argument ("EINVAL");
        }
    }

    return retval;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void appargs_t::print_usage (std::ostream& out)
{
    out << "TFTP server." << std::endl;
    out << std::endl;
    out << "Usage: " << program_invocation_short_name << " [OPTIONS]" << std::endl;
    out << std::endl;
    out << "  -r, --tftproot=<directory>     Serve files from this directory. Default is: " << default_tftp_root << '.' << std::endl;
    out << "  -b, --bind=<address>           Bind the tftp server to this address[:port]." << std::endl;
    out << "                                 Default address is 0.0.0.0:" << tftp_default_port << " (any local IPv4 address)." << std::endl;
    out << "  -p, --port=<port>              Bind the tftp server to this port number." << std::endl;
    out << "                                 This overrides any port in option --bind." << std::endl;
    out << "  -m, --max-clients=<num>        Maximum number of concurrent clients." << std::endl;
    out << "                                 A value of 0 means no limit." << " Default is "<< default_max_clients << '.' << std::endl;
    out << "  -n, --worker-threads=<num>     Number of worker threads. Default is 1, max value is " << (SIGRTMAX-SIGRTMIN+1) << "." << std::endl;
    out << "  -u, --user=<user_id>           When the server is initialized, drop user privileges to this user." << std::endl;
    out << "  -g, --group=<group_id>         When the server is initialized, drop group privileges to this group." << std::endl;
    out << "  -i, --pid-file=<filename>      Create a pid file." << std::endl;
    out << "  -f, --foreground               Run the server in foreground." << std::endl;
    out << "  -s, --stdout                   Log to standard output instead of syslog." << std::endl;
    out << "                                 This option is only applicable if option --foreground is used." << std::endl;
    out << "  -w, --allow-wrq                Allow clients to write files (WRQ requests)." << std::endl;
    out << "                                 Default is to not allow WRQ requests." << std::endl;
    out << "  -o, --allow-overwrite          If WRQ is allowed, allow files to be overwritten." << std::endl;
    out << "  -l, --wrq-size-limit=<size>    Maximum size, in bytes, of files written with WRQ." << std::endl;
    out << "                                 A value of 0 means no limit. Default is no limit. " << std::endl;
    out << "                                 Use postfix 'K', 'M', 'G', for Kilobytes, Megabytes, and Gigabytes. " << std::endl;
    out << "  -t, --dtls                     Add TFTP option for DTLS. Default is to not support DTLS." << std::endl;
    out << "  -c, --certificate=<cert-file>  Server certificate file if using DTLS." << std::endl;
    out << "  -k, --privkey=<key-file>       Private key file if using DTLS." << std::endl;
    out << "  -v, --verbose                  Verbose logging." << std::endl;
    out << "  -h, --help                     Print this help message." << std::endl;
    out << std::endl;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
appargs_t::appargs_t ()
    : tftproot (default_tftp_root),
      max_wrq_size (0),
      num_workers (1),
      max_clients (default_max_clients),
      foreground (false),
      log_to_stdout (false),
      allow_wrq (false),
      allow_overwrite (false),
      dtls (false),
      verbose (false)
{
};


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int appargs_t::parse_args (int argc, char* argv[])
{
    static struct option long_options[] = {
        { "tftproot",       required_argument, 0, 'r'},
        { "bind",           required_argument, 0, 'b'},
        { "port",           required_argument, 0, 'p'},
        { "max-clients",    required_argument, 0, 'm'},
        { "worker-threads", required_argument, 0, 'n'},
        { "user",           required_argument, 0, 'u'},
        { "group",          required_argument, 0, 'g'},
        { "pid-file",       required_argument, 0, 'i'},
        { "foreground",     no_argument,       0, 'f'},
        { "stdout",         no_argument,       0, 's'},
        { "allow-wrq",      no_argument,       0, 'w'},
        { "allow-overwrite",no_argument,       0, 'o'},
        { "wrq-size-limit", required_argument, 0, 'l'},
        { "dtls",           no_argument,       0, 't'},
        { "certificate",    required_argument, 0, 'c'},
        { "privkey",        required_argument, 0, 'k'},
        { "verbose",        no_argument,       0, 'v'},
        { "help",           no_argument,       0, 'h'},
        { 0, 0, 0, 0}
    };
    static const char* arg_format = "r:b:p:m:n:u:g:i:fswol:tc:k:vh";
    int bind_port = -1;
    size_t len;

    while (1) {
        int c = getopt_long (argc, argv, arg_format, long_options, NULL);
        if (c == -1)
            break;
        switch (c) {
        case 'r':
            len = strlen (optarg);
            // Remove trailing / if not root dir.
            if (len>1 && optarg[len-1]=='/')
                tftproot = std::string (optarg, len-1);
            else
                tftproot = optarg;
            break;

        case 'b':
            if (!bind_addr.parse(optarg) || (bind_addr.family()!=AF_INET && bind_addr.family()!=AF_INET6)) {
                std::cerr << "Error: Invalid IPv[4|6] address and/or port number to argument '--bind'" << std::endl;
                return -1;
            }
            break;

        case 'p':
            bind_port = atoi (optarg);
            if (bind_port<=0 || bind_port>65535) {
                std::cerr << "Error: Invalid port number to argument '--port'" << std::endl;
                return -1;
            }
            break;

        case 'm':
            max_clients = atol (optarg);
            if (max_clients < 0) {
                std::cerr << "Error: Invalid number of maximum clients to argument '--max-clients'" << std::endl;
                return -1;
            }
            break;

        case 'n':
            num_workers = atoi (optarg);
            if (num_workers<=0 || num_workers>(SIGRTMAX-SIGRTMIN+1)) {
                std::cerr << "Error: Invalid value to argument '--worker-threads'" << std::endl;
                return -1;
            }
            break;

        case 'u':
            user = optarg;
            break;

        case 'g':
            group = optarg;
            break;

        case 'i':
            pid_file = optarg;
            break;

        case 'f':
            foreground = true;
            break;

        case 's':
            log_to_stdout = true;
            break;

        case 'w':
            allow_wrq = true;
            break;

        case 'o':
            allow_overwrite = true;
            break;

        case 'l':
            try {
                /*
                size_t pos;
                max_wrq_size = std::stoul (optarg, &pos, 10);
                if (pos != strlen(optarg))
                    throw std::invalid_argument ("invalid argument");
                */
                max_wrq_size = parse_file_size (optarg);
            }
            catch (...) {
                std::cerr << "Error: Invalid value to argument '--wrq-size-limit'" << std::endl;
                return -1;
            }
            break;

        case 't':
            dtls = true;
            break;

        case 'c':
            cert_file = optarg;
            break;

        case 'k':
            privkey_file = optarg;
            break;

        case 'v':
            verbose = true;
            break;

        case 'h':
            print_usage (std::cout);
            return 1;

        default:
            return -1;
        }
    }
    if (optind < argc) {
        std::cerr << "Error: Invalid argument" << std::endl;
        print_usage (std::cerr);
        return -1;
    }

    // Set port number
    //
    if (bind_port != -1)
        bind_addr.port ((uint16_t)bind_port);
    else if (!bind_addr.port())
        bind_addr.port (tftp_default_port);

    // Don't log to standard output if not running in foreground
    if (!foreground)
        log_to_stdout = false;

    return 0;
}
