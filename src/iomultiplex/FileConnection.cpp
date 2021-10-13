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
#include <iomultiplex/FileConnection.hpp>
#include <iomultiplex/IOHandler.hpp>
#include <iomultiplex/Log.hpp>
#include <cstring>
#include <unistd.h>


namespace iomultiplex {

#define THIS_FILE "FileConnection"



    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    FileConnection::FileConnection (IOHandler& io_handler)
        : FdConnection (io_handler),
          name ("")
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    FileConnection::FileConnection (IOHandler& io_handler,
                                    const std::string& file,
                                    int flags)
        : FdConnection (io_handler),
          name ("")
    {
        open (file, flags);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    FileConnection::FileConnection (IOHandler& io_handler,
                                    const std::string& file,
                                    int flags,
                                    int mode)
        : FdConnection (io_handler),
          name ("")
    {
        open (file, flags, mode);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    FileConnection::FileConnection (FileConnection&& fc)
        : FdConnection (std::move(fc)),
          name {fc.name}
    {
        fc.name = "";
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    FileConnection& FileConnection::operator= (FileConnection&& fc)
    {
        if (this != &fc) {
            FdConnection::operator= (std::move(fc));
            name = fc.name;
            fc.name = "";
        }
        return *this;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int FileConnection::open (const std::string& filename, int flags)
    {
        if (flags | (O_CREAT|O_TMPFILE))
            return open (filename, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        else
            return open (filename, flags, 0);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int FileConnection::open (const std::string& filename, int flags, int mode)
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
                Log::warning (THIS_FILE
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
    bool FileConnection::is_open () const
    {
        return fd.load() != -1;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    const std::string& FileConnection::filename () const
    {
        return name;
    }



}
