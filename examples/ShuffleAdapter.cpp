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
#include "ShuffleAdapter.hpp"
#include <cstring>


namespace adapter_test {


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ShuffleAdapter::ShuffleAdapter (iomultiplex::Connection& conn, bool close_on_destruct)
        : iomultiplex::Adapter (conn, close_on_destruct),
          rbuf_size (0),
          wbuf_size (0)
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ShuffleAdapter::ShuffleAdapter (std::shared_ptr<iomultiplex::Connection> conn_ptr)
        : iomultiplex::Adapter (conn_ptr),
          rbuf_size (0),
          wbuf_size (0)
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t ShuffleAdapter::do_read (void* buf, size_t size, off_t offset, int& errnum)
    {
        if (size > rbuf_size) {
            rbuf.reset (new char[size+1]); // Make room for null-termination
            rbuf_size = size;
        }

        auto result = iomultiplex::Adapter::do_read (rbuf.get(), size, offset, errnum);
        if (result > 0) {
            rbuf.get()[result] = '\0'; // Make sure the tmp buffer is always null-terminated
            strfry (rbuf.get());       // Shuffle: strfry works with null-terminated strings
            memcpy (buf, rbuf.get(), result);
        }
        return result;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t ShuffleAdapter::do_write (const void* buf, size_t size, off_t offset, int& errnum)
    {
        if (size > wbuf_size) {
            wbuf.reset (new char[size+1]); // Make room for null-termination
            wbuf_size = size;
            wbuf.get()[wbuf_size] = '\0'; // Make sure the tmp buffer is always null-terminated
        }
        if (size > 0) {
            memcpy (wbuf.get(), buf, size);
            strfry (wbuf.get()); // Shuffle: strfry works with null-terminated strings
        }

        return iomultiplex::Adapter::do_write (wbuf.get(), size, offset, errnum);
    }


}
