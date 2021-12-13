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
         */
        FdConnection& operator= (FdConnection&& fc);

        /**
         * Return the file descriptor associated with this connection.
         * @return The file descriptor associated with this connection.
         */
        virtual int handle ();

        /**
         * Check is the connection is open or not.
         * @return <code>true</code> if the connection is open.
         */
        virtual bool is_open () const;

        /**
         * Return the iohandler_base object that this connection uses.
         * @return The iohandler_base object that manages the I/O operations for this connection.
         */
        virtual iohandler_base& io_handler ();

        /**
         * Cancel I/O operations for this connection.
         */
        virtual void cancel (bool cancel_rx=true, bool cancel_tx=true);

        /**
         * Cancel all pending I/O operations and close the file descriptor.
         */
        virtual void close ();

        /**
         * Do the actual reading from the file descriptor.
         * This method should normally not be called directly,
         * it is called by the iohandler_base when the file descriptor is
         * ready to read data.
         * @param buf A pointer to the memory area where data should be stored.
         * @param size The number of bytes to read.
         * @param errnum The value of <code>errno</code> after
         *               the read operation. Always 0 if no error occurred.
         * @return The number of bytes that was read.
         *         <br/>
         *         On error, -1 is returned and parameter <code>errnum</code>
         *         is set to some appropriate value.
         */
        virtual ssize_t do_read (void* buf, size_t size, int& errnum);

        /**
         * Do the actual writing to the file descriptor.
         * This method should normally not be called directly,
         * it is called by the iohandler_base when the file descriptor is
         * ready to write data.
         * @param buf A pointer to the memory area that should be written.
         * @param size The number of bytes to write.
         * @param errnum The value of <code>errno</code> after
         *               the write operation. Always 0 if no error occurred.
         * @return The number of bytes that was written.
         *         <br/>
         *         On error, -1 is returned and parameter <code>errnum</code>
         *         is set to some appropriate value.
         */
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
