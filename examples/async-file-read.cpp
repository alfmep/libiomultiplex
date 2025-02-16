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
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <iomultiplex.hpp>

using namespace std;
namespace iom = iomultiplex;


//------------------------------------------------------------------------------
// Called when a read operation has completed
//------------------------------------------------------------------------------
static bool on_read (iom::io_result_t& ior)
{
    // Check for EOF or error
    //
    if (ior.result <= 0) {
        if (ior.errnum) {
            cerr << "Read error: " << strerror(ior.errnum) << endl;
            exit (1);
        }
        // EOF. We're done, stop the I/O handler
        ior.conn.io_handler().stop ();
    }else{

        // Write the read result to standard output
        //
        cout.write (static_cast<const char*>(ior.buf), ior.result);

        // Queue the next read operation
        //
        ior.conn.read (ior.buf, ior.size, on_read, ior.timeout);
    }
    return true;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    if (argc < 2) {
        cerr << "Usage: sync-file-read <filename> [timeout_ms]" << endl;
        cerr << endl;
        cerr << "Note that epoll can't be used to read regular files." << endl;
        cerr << "But files on a network disk can be read, or some" << endl;
        cerr << "device files." << endl;
        cerr << endl;
        return 1;
    }

    // Create and start the I/O handler in a worker thread
    //
    iom::default_iohandler ioh;
    ioh.run (true);

    // Open a file
    //
    iom::file_connection f (ioh);
    if (f.open(argv[1], O_RDONLY)) {
        perror ("open");
        return 1;
    }

    // Set optional read timeout. Default is no timeout.
    //
    unsigned timeout = (unsigned) -1;
    if (argc > 2)
        timeout = stoi (argv[2]);

    // Queue a read operation
    // The result is handled in function 'on_read'
    //
    char buf[2048];
    if (f.read(buf, sizeof(buf), on_read, timeout)) {
        if (errno == EPERM)
            cerr << "Error: epoll can't be used with regular files." << endl;
        else
            cerr << "Error: " << strerror(errno) << endl;
        return 1;
    }

    // Wait for the I/O handler to finish.
    //
    ioh.join ();

    return 0;
}
