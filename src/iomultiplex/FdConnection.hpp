/*
 * Copyright (C) 2021-2023 Dan Arrhenius <dan@ultramarin.se>
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
#ifndef IOMULTIPLEX_FDCONNECTION_HPP
#define IOMULTIPLEX_FDCONNECTION_HPP

#include <iomultiplex/Connection.hpp>
#include <memory>
#include <atomic>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


namespace iomultiplex {

    /**
     * Base class for I/O connections using a file descriptor.
     * This is a connection that reads and writes using a
     * UN*X file descriptor. It is a base class for more
     * specific connections using file descriptors, but can
     * also be used as a generic wrapper for file descriptors
     * opened by other means.
     */
    class FdConnection : public Connection {
    public:
        /**
         * Constructor.
         * @param io_handler This object will manage I/O operations for this connection.
         */
        FdConnection (iohandler_base& io_handler);

        /**
         * Constructor.
         * Create a connection object for an already opened file descriptor.
         * @param io_handler This object will manage I/O operations for this connection.
         * @param file_descriptor An open file descriptor.
         *        \note The file descriptor should be opened in non blocking mode.
         * @param keep_open If <code>true</code>, don't close the file descriptor in the destructor.
         */
        FdConnection (iohandler_base& io_handler, int file_descriptor, bool keep_open=false);

        /**
         * Move Constructor.
         * \note When moving FdConnection objects that has queued read/write
         * operations, those operations will be cancelled.
         * @param fc The FdConnection object to move.
         */
        FdConnection (FdConnection&& fc);

        /**
         * Destructor.
         * All pending I/O operations will be cancelled and the connection will be closed.
         */
        virtual ~FdConnection ();

        /**
         * Move operator.
         * \note When moving FdConnection objects that has queued read/write
         * operations, those operations will be cancelled.
         * @param fc The FdConnection object to move.
         * @return A reference to this object.
         */
        FdConnection& operator= (FdConnection&& fc);

        virtual int handle ();
        virtual bool is_open () const;
        virtual iohandler_base& io_handler ();
        virtual void cancel (bool cancel_rx=true,
                             bool cancel_tx=true,
                             bool fast=false);
        virtual void close ();
        virtual ssize_t do_read (void* buf, size_t size, int& errnum);
        virtual ssize_t do_write (const void* buf, size_t size, int& errnum);


    protected:
        std::atomic_int fd; // File descriptor.


    private:
        FdConnection () = delete;
        FdConnection (const FdConnection& fc) = delete;
        FdConnection& operator= (const FdConnection& fc) = delete;

        iohandler_base* ioh;
        bool keep_open;
    };


}
#endif
