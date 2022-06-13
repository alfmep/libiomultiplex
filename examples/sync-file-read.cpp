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
#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <iomultiplex.hpp>

using namespace std;
namespace iom = iomultiplex;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    if (argc < 2) {
        cerr << "Usage: sync-file-read <filename>" << endl;
        return 1;
    }

    // Start the I/O handler in a worker thread
    //
    // The I/O handler must be an instance of IOHandler_Poll,
    // since epoll doesn't work with regular files.
    //
    iom::IOHandler_Poll ioh;
    ioh.run (true);

    // Open a file
    //
    iom::FileConnection f (ioh);
    if (f.open(argv[1], O_RDONLY)) {
        perror ("open");
        return 1;
    }

    // Read from the file and print to standard output
    //
    char buf[4096];
    ssize_t result;
    do {
        result = f.read (buf, sizeof(buf));
        if (result > 0)
            cout.write (buf, result);
    }while (result > 0);

    if (result < 0)
        perror ("read");

    return result<0 ? 1 : 0;
}
