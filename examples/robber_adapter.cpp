/*
 * Copyright (C) 2021,2025 Dan Arrhenius <dan@ultramarin.se>
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
#include "robber_adapter.hpp"
#include <set>
#include <cstring>
#include <ctype.h>


namespace adapter_test {


    static std::set<char> consonants = {{
            'B', 'C', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 'M', 'N', 'P', 'Q', 'R', 'S', 'T', 'V', 'W', 'X', 'Z',
            'b', 'c', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'm', 'n', 'p', 'q', 'r', 's', 't', 'v', 'w', 'x', 'z'
        }};;


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    robber_adapter::robber_adapter (iomultiplex::connection& conn, bool close_on_destruct)
        : iomultiplex::adapter (conn, close_on_destruct),
          wbuf_size (0)
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    robber_adapter::robber_adapter (std::shared_ptr<iomultiplex::connection> conn_ptr)
        : iomultiplex::adapter (conn_ptr),
          wbuf_size (0)
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t robber_adapter::do_write (const void* buf, size_t size, int& errnum)
    {
        if (wbuf_size < size*3) {
            wbuf.reset (new char[size*3]);
            wbuf_size = size;
        }

        size_t wsize = 0; // Size of modified buffer to write to slave connection

        if (size > 0) {
            char* dst = wbuf.get ();
            for (size_t i=0; i<size; ++i) {
                char ch = ((char*)buf)[i];
                dst[wsize++] = ch;
                if (consonants.find(ch) != consonants.end()) {
                    dst[wsize++] = isupper(ch) ? 'O' : 'o';
                    dst[wsize++] = ch;
                }
            }
        }

        auto retval = iomultiplex::adapter::do_write (wbuf.get(), wsize, errnum);

        // We can't really say we have written more than 'size' bytes, can we?
        if (retval>0  &&  (size_t)retval>size)
            return (ssize_t) size;
        else
            return retval;
    }


}
