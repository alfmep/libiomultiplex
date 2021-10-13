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
#include <iomultiplex/FdConnection.hpp>
#include <iomultiplex/IOHandler.hpp>
#include <iomultiplex/Log.hpp>
#include <cstring>
#include <cerrno>
#include <unistd.h>


//#define TRACE_DEBUG

#ifdef TRACE_DEBUG
#define TRACE(format, ...) Log::debug("%s:%s:%d: " format, __FILE__, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#define TRACE(format, ...)
#endif


namespace iomultiplex {


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    FdConnection::FdConnection (IOHandler& io_handler)
        : fd {-1},
          ioh {&io_handler}
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    FdConnection::FdConnection (IOHandler& io_handler, int file_descriptor, bool keep_open)
        : fd {file_descriptor},
          ioh {&io_handler}
    {
        this->keep_open = keep_open;
        fd = file_descriptor;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    FdConnection::FdConnection (FdConnection&& conn)
    {
        conn.cancel ();
        fd  = conn.fd.exchange (-1);
        ioh = conn.ioh;
        conn.ioh = nullptr;
        def_rx_cb = std::move (conn.def_rx_cb);
        def_tx_cb = std::move (conn.def_tx_cb);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    FdConnection::~FdConnection ()
    {
        TRACE ("ENTER");
        if (!keep_open)
            close ();
        TRACE ("EXIT");
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    FdConnection& FdConnection::operator= (FdConnection&& rhs)
    {
        if (this != &rhs) {
            cancel ();
            rhs.cancel ();
            fd  = rhs.fd.exchange (-1);
            ioh = rhs.ioh;
            rhs.ioh = nullptr;
            def_rx_cb = std::move (rhs.def_rx_cb);
            def_tx_cb = std::move (rhs.def_tx_cb);
        }
        return *this;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int FdConnection::handle ()
    {
        return fd;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool FdConnection::is_open () const
    {
        return fd != -1;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IOHandler& FdConnection::io_handler ()
    {
        return *ioh;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void FdConnection::cancel ()
    {
        TRACE ("ENTER");
        ioh->cancel (*this);
        TRACE ("EXIT");
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void FdConnection::close ()
    {
        TRACE ("ENTER");
        cancel ();
        int tmp_fd = fd.exchange (-1);
        if (tmp_fd != -1)
            ::close (tmp_fd);
        TRACE ("EXIT");
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t FdConnection::do_read (void* buf, size_t size, off_t offset, int& errnum)
    {
        TRACE ("ENTER");
        ssize_t result = offset==-1 ? ::read(fd, buf, size) : ::pread(fd, buf, size, offset);
        errnum = result<0 ? errno : 0;
        TRACE ("EXIT");
        return result;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ssize_t FdConnection::do_write (const void* buf, size_t size, off_t offset, int& errnum)
    {
        TRACE ("ENTER");
        ssize_t result = offset==-1 ? ::write(fd, buf, size) : ::pwrite(fd, buf, size, offset);
        errnum = result<0 ? errno : 0;
        TRACE ("EXIT");
        return result;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    /*
    void FdConnection::sync ()
    {
        if (fd != -1)
            syncfs (fd);
    }
    */

}
