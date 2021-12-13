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
#include "ReverseAdapter.hpp"
#include <algorithm>
#include <cstring>


namespace adapter_test {


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ReverseAdapter::ReverseAdapter (iomultiplex::Connection& conn, bool close_on_destruct)
        : iomultiplex::Adapter (conn, close_on_destruct),
          wbuf_size (0)
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ReverseAdapter::ReverseAdapter (std::shared_ptr<iomultiplex::Connection> conn_ptr)
        : iomultiplex::Adapter (conn_ptr),
          wbuf_size (0)
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t ReverseAdapter::do_read (void* buf, size_t size, int& errnum)
    {
        auto result = iomultiplex::Adapter::do_read (buf, size, errnum);
        if (result > 0)
            std::reverse ((char*)buf, ((char*)buf)+result);
        return result;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t ReverseAdapter::do_write (const void* buf, size_t size, int& errnum)
    {
        if (size > wbuf_size) {
            wbuf.reset (new char[size]);
            wbuf_size = size;
        }
        if (size > 0) {
            for (size_t i=0; i<size; ++i)
                wbuf.get()[i] = ((char*)buf)[size-1-i];
        }
        return iomultiplex::Adapter::do_write (wbuf.get(), size, errnum);
    }


}
