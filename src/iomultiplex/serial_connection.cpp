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
#include <iomultiplex/serial_connection.hpp>
#include <iomultiplex/iohandler_base.hpp>
#include <iomultiplex/log.hpp>
#include <cstring>
#include <unistd.h>


namespace iomultiplex {

#define THIS_FILE "serial_connection"


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    serial_connection::serial_connection (iohandler_base& io_handler)
        : fd_connection (io_handler),
          name ("")
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    serial_connection::serial_connection (iohandler_base& io_handler, int file_descriptor)
        : fd_connection (io_handler, file_descriptor),
          name ("")
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    serial_connection::serial_connection (iohandler_base& io_handler,
                                          const std::string& device_filename,
                                          int  baud_rate,
                                          int  data_bits,
                                          parity_t p,
                                          int stop_bits)
        : fd_connection (io_handler),
          name ("")
    {
        open (device_filename, baud_rate, data_bits, p, stop_bits);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    serial_connection::serial_connection (serial_connection&& sc)
        : fd_connection (std::forward<serial_connection>(sc)),
          name {std::move(sc.name)}
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    serial_connection& serial_connection::operator= (serial_connection&& sc)
    {
        if (this != &sc) {
            fd_connection::operator= (std::move(sc));
            name = std::move (sc.name);
        }
        return *this;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int serial_connection::open (const std::string& device_filename,
                                 int baud_rate,
                                 int data_bits,
                                 parity_t p,
                                 int stop_bits)
    {
        if (fd.load() != -1) {
            errno = 0;
            return 0;
        }

        name = device_filename;

        int errnum = 0;
        fd = ::open (name.c_str(), O_RDWR);
        if (fd < 0) {
            errnum = errno;
            log::warning (THIS_FILE " - Unable to open serial device %s", name.c_str());
        }else{
            int result = 0;
            int flags  = fcntl (fd, F_GETFL, 0);
            if (flags != -1)
                result = fcntl (fd, F_SETFL, flags | O_NONBLOCK);
            if (flags==-1 || result==-1) {
                errnum = errno;
                log::warning (THIS_FILE
                              " - Unable to set serial device in non-blocking mode: %s",
                              strerror(errno));
                ::close (fd);
                fd = -1;
            }
        }

        termios_cfg tio;

        // Cet current terminal attributes
        //
        if (get_cfg(tio)) {
            errnum = errno;
            log::warning (THIS_FILE " - Unable to read terminal attributes %s - %s",
                          name.c_str(), strerror(errnum));
            ::close (fd);
            fd = -1;
        }

        // Set baud rate
        //
        if (fd >= 0) {
            if (tio.speed(baud_rate)) {
                errnum = errno;
                log::warning (THIS_FILE " - Unable to set baud rate for %s - %s",
                              name.c_str(), strerror(errnum));
                ::close (fd);
                fd = -1;
            }
        }

        // Set data bits
        //
        if (fd >= 0) {
            if (tio.data_bits(data_bits)) {
                errnum = errno;
                log::warning (THIS_FILE " - Unable to set data bits value for %s - %s",
                              name.c_str(), strerror(errnum));
                ::close (fd);
                fd = -1;
            }
        }

        // Set parity
        //
        if (fd >= 0) {
            if (tio.parity(p)) {
                errnum = errno;
                log::warning (THIS_FILE " - Unable to set parity for %s - %s",
                              name.c_str(), strerror(errnum));
                ::close (fd);
                fd = -1;
            }
        }

        // Set stop bits
        //
        if (fd >= 0) {
            if (tio.stop_bits(stop_bits)) {
                errnum = errno;
                log::warning (THIS_FILE " - Unable to set stop bits for %s - %s",
                              name.c_str(), strerror(errnum));
                ::close (fd);
                fd = -1;
            }
        }

        // Activate settings
        //
        if (fd >= 0) {
            if (set_cfg(tio)) {
                errnum = errno;
                log::warning (THIS_FILE " - Unable to configure %s - ", name.c_str(), strerror(errnum));
                ::close (fd);
                fd = -1;
            }
        }

        errno = errnum;
        return fd<0 ? -1 : 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int serial_connection::get_cfg (termios_cfg& cfg)
    {
        return tcgetattr (fd, &cfg);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int serial_connection::set_cfg (const termios_cfg& cfg)
    {
        return tcsetattr (fd, TCSANOW, &cfg);
    }


}
