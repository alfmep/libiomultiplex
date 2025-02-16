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
#include <iomultiplex/file_connection.hpp>
#include <iomultiplex/iohandler_base.hpp>
#include <iomultiplex/log.hpp>
#include <cstring>
#include <unistd.h>


namespace iomultiplex {

#define THIS_FILE "file_connection"



    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    file_connection::file_connection (iohandler_base& io_handler)
        : fd_connection (io_handler),
          name ("")
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    file_connection::file_connection (iohandler_base& io_handler,
                                      const std::string& file,
                                      int flags)
        : fd_connection (io_handler),
          name ("")
    {
        open (file, flags);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    file_connection::file_connection (iohandler_base& io_handler,
                                      const std::string& file,
                                      int flags,
                                      int mode)
        : fd_connection (io_handler),
          name ("")
    {
        open (file, flags, mode);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    file_connection::file_connection (file_connection&& fc)
        : fd_connection (std::move(fc)),
          name {fc.name}
    {
        fc.name = "";
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    file_connection& file_connection::operator= (file_connection&& fc)
    {
        if (this != &fc) {
            fd_connection::operator= (std::move(fc));
            name = fc.name;
            fc.name = "";
        }
        return *this;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int file_connection::open (const std::string& filename, int flags)
    {
        if (flags | (O_CREAT|O_TMPFILE))
            return open (filename, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        else
            return open (filename, flags, 0);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int file_connection::open (const std::string& filename, int flags, int mode)
    {
        if (fd.load() != -1) {
            errno = 0;
            return -1;
        }

        name = filename;

        fd = ::open (name.c_str(), flags, mode);
        if (fd >= 0) {
            int errnum = errno;
            int result = 0;
            int flags  = fcntl (fd, F_GETFL, 0);
            if (flags != -1)
                result = fcntl (fd, F_SETFL, flags | O_NONBLOCK);
            if (flags==-1 || result==-1) {
                errnum = errno;
                log::warning (THIS_FILE
                              " - Unable to set file handle in non-blocking mode: %s",
                              strerror(errno));
                ::close (fd);
                fd = -1;
            }
            errno = errnum;
        }
        return fd<0 ? -1 : 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    const std::string& file_connection::filename () const
    {
        return name;
    }



}
