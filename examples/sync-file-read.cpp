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
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <unistd.h>
#include <iomultiplex.hpp>

using namespace std;
namespace iom = iomultiplex;


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

    // Read from the file and print to standard output
    // until end-of-file or an error occurs.
    //
    char buf[2048];
    ssize_t result;
    do {
        result = f.read (buf, sizeof(buf), timeout);
        if (result > 0)
            cout.write (buf, result);
    }while (result > 0);

    // Check the result of the last read operation
    //
    if (result == -1) {
        if (errno == EPERM)
            cerr << "Error: epoll can't be used with regular files." << endl;
        else
            cerr << "Error: " << strerror(errno) << endl;
    }

    return result<0 ? 1 : 0;
}
