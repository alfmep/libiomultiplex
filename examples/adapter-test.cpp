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
#include <memory>
#include <string>
#include <list>
#include <cstdio>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>

#include "ObfuscateAdapter.hpp"
#include "ShuffleAdapter.hpp"
#include "CaseAdapter.hpp"
#include "ReverseAdapter.hpp"
#include "RobberAdapter.hpp"


using namespace std;
namespace iom = iomultiplex;

static constexpr size_t chunk_size = 4096;


static bool verbose = false;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void print_usage_and_exit (ostream& out, int exit_code)
{
    out << endl;
    out << "Usage: " << program_invocation_short_name << " [-v] [-i input_adapter ...] [-o output_adapter ...]" << endl;
    out << endl;
    out << "       Simple example application to demonstrate Adapter objects in libiomultiplex." << endl;
    out << "       Data is read from standard input and written to standard output. The RX/TX data is modified in optional I/O adapters." << endl;
    out << "       Data is read and written in chunks of " << chunk_size << " bytes." << endl;
    out << endl;
    out << "       Data flow:" << endl;
    out << "       RX: stdin ==> input_adapter_1  ==> ... ==> input_adapter_n ==> buffer" << endl;
    out << "       TX:           buffer ==> output_adapter_1 ==> ... ==> output_adapter_n ==> stdout" << endl;
    out << endl;
    out << "       OPTIONS:" << endl;
    out << "       -i, --rx-adapter=ADAPTER  Add an adapter to the chain of input adapters." << endl;
    out << "       -o, --tx-adapter=ADAPTER  Add an adapter to the chain of output adapters." << endl;
    out << "       -v, --verbose             Print debug info about attached adapters to standard error." << endl;
    out << "       -h, --help                Print this help and exit." << endl;
    out << endl;
    out << "       Available input adapters:" << endl;
    out << "           obfuscate   - Reversibly obfuscate read data chunks using the glibc funtion memfrob()." << endl;
    out << "           shuffle     - Irreversibly obfuscate read data chunks using the glibc funtion strfry()." << endl;
    out << "                         Works best with text data." << endl;
    out << "           uppercase   - Set characters to upper case using toupper()." << endl;
    out << "                         Works best with text data." << endl;
    out << "           lowercase   - Set characters to lower case using tolower()." << endl;
    out << "                         Works best with text data." << endl;
    out << "           randomcase  - Randomly change case of characters using toupper()/tolower()." << endl;
    out << "                         Works best with text data." << endl;
    out << "           reverse     - Reverse each read data chunk." << endl;
    out << endl;
    out << "       Available output adapters:" << endl;
    out << "           obfuscate   - Reversibly obfuscate written data chunks using the glibc funtion memfrob()." << endl;
    out << "           shuffle     - Irreversibly obfuscate written data chunks using the glibc funtion strfry()." << endl;
    out << "                         Works best with text data." << endl;
    out << "           uppercase   - Set characters to upper case using toupper()." << endl;
    out << "                         Works best with text data." << endl;
    out << "           lowercase   - Set characters to lower case using tolower()." << endl;
    out << "                         Works best with text data." << endl;
    out << "           randomcase  - Randomly change case of characters using toupper()/tolower()." << endl;
    out << "                         Works best with text data." << endl;
    out << "           reverse     - Reverse each written data chunk." << endl;
    out << "           robber      - The Robber Language." << endl;
    out << "                         Every consonant character is doubled, and an o is inserted in-between." << endl;
    out << "                         Works best with text data." << endl;
    out << endl;

    exit (exit_code);
}


//------------------------------------------------------------------------------
// Return pair< rx_connection, tx_connection >
//------------------------------------------------------------------------------
void parse_args (int argc,
                 char* argv[],
                 list<string>& rx_adapter_names,
                 list<string>& tx_adapter_names)
{
    static struct option long_options[] = {
        { "rx-adapter", required_argument, 0, 'i'},
        { "tx-adapter", required_argument, 0, 'o'},
        { "verbose",    no_argument,       0, 'v'},
        { "help",       no_argument,       0, 'h'},
        { 0, 0, 0, 0}
    };
    static const char* arg_format = "i:o:vh";

    while (1) {
        int c = getopt_long (argc, argv, arg_format, long_options, NULL);
        if (c == -1)
            break;
        switch (c) {
        case 'i':
            rx_adapter_names.emplace_back (optarg);
            break;

        case 'o':
            tx_adapter_names.emplace_front (optarg);
            break;

        case 'v':
            verbose = true;
            break;

        case 'h':
            print_usage_and_exit (cout, 0);
            break;

        default:
            print_usage_and_exit (cerr, 1);
            break;
        }
    }
    if (optind < argc) {
        std::cerr << "Error: Invalid argument" << std::endl;
        print_usage_and_exit (std::cerr, 1);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int add_adapter (shared_ptr<iom::Connection>& cptr, const string& name, bool rx)
{
    // Before:
    //     cptr points to a connection or adapter object
    // After:
    //     cptr points to newly created adapter with original cptr as slave connection
    //
    if (name == "obfuscate")
        cptr.reset (new adapter_test::ObfuscateAdapter(cptr));
    else if (name == "shuffle")
        cptr.reset (new adapter_test::ShuffleAdapter(cptr));
    else if (name == "uppercase")
        cptr.reset (new adapter_test::CaseAdapter(cptr, adapter_test::CaseAdapter::uppercase));
    else if (name == "lowercase")
        cptr.reset (new adapter_test::CaseAdapter(cptr, adapter_test::CaseAdapter::lowercase));
    else if (name == "randomcase")
        cptr.reset (new adapter_test::CaseAdapter(cptr, adapter_test::CaseAdapter::randomcase));
    else if (name == "reverse")
        cptr.reset (new adapter_test::ReverseAdapter(cptr));
    else if (name == "robber" && !rx)
        cptr.reset (new adapter_test::RobberAdapter(cptr));
    else
        return 1;

    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int initialize (iom::iohandler_base& ioh,
                       shared_ptr<iom::Connection>& rx,
                       shared_ptr<iom::Connection>& tx,
                       list<string>& rx_adapter_names,
                       list<string>& tx_adapter_names)
{
    static constexpr const char* file_stdin  {"/dev/stdin"};
    static constexpr const char* file_stdout {"/dev/stdout"};

    // Opend standard input and output
    //
    rx.reset (new iom::FileConnection(ioh, file_stdin,  O_RDONLY));
    tx.reset (new iom::FileConnection(ioh, file_stdout, O_WRONLY));
    if (!rx->is_open() || !tx->is_open()) {
        cerr << "Error opening stdin/stdout" << endl;
        return 1;
    }

    string dbg_txt;

    // Create RX adapters
    //
    if (verbose) {
        dbg_txt = "stdin.read()";
        cerr << "RX data flow:" << endl;
        cerr << "    buffer <== " << dbg_txt << endl;
        if (!rx_adapter_names.empty())
            cerr << "    Attatch RX adapters" << endl;
    }
    for (auto& name : rx_adapter_names) {
        if (verbose) {
            dbg_txt = name + string(" <== ") + dbg_txt;
            cerr << "    buffer <== " << dbg_txt << endl;
        }
        if (add_adapter(rx, name, true)) {
            cerr << "Error: Invalid input adapter: '" << name << "' (use argument '-h' for help)" << endl;
            return 1;
        }
    }

    // Create TX adapters
    //
    if (verbose) {
        dbg_txt = "stdout.write()";
        cerr << "TX data flow:" << endl;
        cerr << "    buffer ==> " << dbg_txt << endl;
        if (!tx_adapter_names.empty())
            cerr << "    Attach TX adapters" << endl;
    }
    for (auto& name : tx_adapter_names) {
        if (verbose) {
            dbg_txt = name + string(" ==> ") + dbg_txt;
            cerr << "    buffer ==> " << dbg_txt << endl;
        }
        if (add_adapter(tx, name, false)) {
            cerr << "Error: Invalid output adapter: '" << name << "' (use argument '-h' for help)" << endl;
            return 1;
        }
    }

    return 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    list<string> rx_adapter_names;
    list<string> tx_adapter_names;

    // Parse command line options
    //
    parse_args (argc, argv, rx_adapter_names, tx_adapter_names);

    if (verbose) {
        cerr << "==== Start of debug info ====" << endl;
        cerr << "Number of RX adapters: " << rx_adapter_names.size() << endl;
        cerr << "Number of TX adapters: " << tx_adapter_names.size() << endl;
    }

    //
    // Start I/O multiplexer with a worker thread
    //
    iom::default_iohandler ioh;
    ioh.run (true);

    // Open files and create adapters
    //
    shared_ptr<iom::Connection> rx;
    shared_ptr<iom::Connection> tx;
    if (initialize(ioh, rx, tx, rx_adapter_names, tx_adapter_names))
        exit (1);

    if (verbose)
        cerr << "==== End of debug info ====" << endl;

    //
    // Read from standard input and write to standard output
    //
    int exit_code = 0;
    while (true) {
        char buf[chunk_size];

        //
        // Read
        //
        auto len = rx->read (buf, sizeof(buf));
        if (len <= 0) {
            if (len < 0) {
                cerr << "Read error: " << strerror(errno) << endl;
                exit_code = 1;
            }
            break;
        }

        //
        // write
        //
        size_t size = (size_t) len;
        size_t pos = 0;
        while (size) {
            len = tx->write (buf+pos, size);
            if (len <= 0) {
                break;
            }
            pos  += (size_t) len;
            size -= (size_t) len;
        }
        if (len <= 0) {
            if (len < 0) {
                cerr << "Write error: " << strerror(errno) << endl;
                exit_code = 1;
            }
            break;
        }
    }

    return exit_code;
}
