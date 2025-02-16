/*
 * Copyright (C) 2021,2023,2025 Dan Arrhenius <dan@ultramarin.se>
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
#include <iomultiplex.hpp>


using namespace std;
namespace iom = iomultiplex;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    iom::default_iohandler ioh;
    iom::socket_connection s (ioh);

    ioh.run (true);

    if (s.open(AF_INET, SOCK_STREAM)) {
        perror ("s.open");
        return 1;
    }
    if (s.connect(iom::ip_addr(127,0,0,1, 12000), 1000)) {
        perror ("s.connect");
        return 1;
    }

    cout << "Connected" << endl;

    char buf[256];
    ssize_t result=0;
    while ((result=s.read (buf, sizeof(buf))) > 0) {
        for (ssize_t i=0; i<result; ++i)
            cout << (char)buf[i];
        cout << flush;
    }
    if (result < 0) {
        perror ("s.read");
        return 1;
    }

    return 0;
}
