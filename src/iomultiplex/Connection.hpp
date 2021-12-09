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
#ifndef IOMULTIPLEX_CONNECTION_HPP
#define IOMULTIPLEX_CONNECTION_HPP

#include <iomultiplex/types.hpp>
#include <iomultiplex/io_result_t.hpp>
#include <condition_variable>
#include <mutex>
#include <cstdlib>


namespace iomultiplex {

    // Forward declaration
    class iohandler_base;


    /**
     * Base class for I/O connections.
     * A connection object represents an endpoint of some
     * type of I/O communication. It can be a network socket,
     * an open file, a timer, a pipe, or some other type
     * of I/O connection.
     * <br/>
     * Reading and writing can be done synchronously or
     * asynchronously.
     * <br/>
     * This class can't be instantiated directly,
     * it is a base class for more specific types of connections.
     */
    class Connection {
    public:
        /**
         * Default constructor.
         */
        Connection () = default;

        /**
         * Destructor.
         * All pending I/O operations will be cancelled
         * and the connection will be closed.
         */
        virtual ~Connection () = default;

        /**
         * Return the file descriptor associated with this connection.
         * @return The file desctiptor associated with this connection.
         */
        virtual int handle () = 0;

        /**
         * Check if the connection is open or not.
         * @return <code>true</code> if the connection is open.
         */
        virtual bool is_open () const = 0;

        /**
         * Return the iohandler_base object used by this connection.
         * @return The iohandler_base object that manages the
         *         I/O operations for this connection.
         */
        virtual iohandler_base& io_handler () = 0;

        /**
         * Cancel all I/O operations for this connection.
         * When an I/O operation is cancelled, the callback function
         * assiciated with that I/O operation is called and the
         * <code>result</code> and <code>errnum</code> members of
         * the io_result_t parameter will be set to <code>-1</code>
         * and <code>ECANCELED</code> respectively.<br/>
         * This is done so allocated resources associated
         * with the I/O operation can be released.
         *
         * @param cancel_rx If true, cancel all input operations.
         * @param cancel_tx If true, cancel all output operations.
         */
        virtual void cancel (bool cancel_rx=true, bool cancel_tx=true) = 0;

        /**
         * Close the connection.
         * This method should cancel all I/O operations before closing.
         */
        virtual void close () = 0;

        /**
         * Set a default read operation callback function.
         * @param rx_cb A default read operation callback,
         *        or no default callback if <code>nullptr</code>.
         */
        void default_rx_callback (io_callback_t rx_cb);

        /**
         * Set a default write operation callback function.
         * @param tx_cb A default write operation callback,
         *        or no default callback if <code>nullptr</code>.
         */
        void default_tx_callback (io_callback_t tx_cb);

        /**
         * Queue a read operation.
         * This method queues a read operation and returns immediately.
         * The reading of data is done asynchronously and when the data
         * is read, or an error occurs, the supplied callback function
         * is called.
         * @param buf The buffer where to store the data.
         * @param size The maximum number of bytes to read.
         * @param rx_cb If not <code>nullptr</code>, this callback
         *              is called when the read operation has generated
         *              a result (data is read, an error occurred,
         *              the operation is cancelled, or has timed out).
         *              <br/>
         *              If <code>nullptr</code>, the default read
         *              operation callback is called if one is set.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success, -1 if the file descriptor isn't valid.
         *         <br/><b>Note:</b> A return value of 0 means that the
         *         read operation was queued, not that the actual read
         *         operation was successful.
         * @see io_callback_t
         * @see io_result_t
         * @see default_rx_callback
         */
        int read (void* buf, size_t size, io_callback_t rx_cb, unsigned timeout=-1) {
            return this->pread (buf, size, -1, rx_cb, timeout);
        }

        /**
         * Synchromized read data into a buffer.
         * This method blocks until the read operation is finished, cancelled,
         * timed out, or an error occurs.
         * @param buf The buffer where to store the data.
         * @param size The maximum number of bytes to read.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return The number of bytes read, or -1 on error.
         *         <br/>
         *         On error, <code>errno</code> is set to an appropriate value.
         */
        ssize_t read (void* buf, size_t size, unsigned timeout=-1) {
            return this->pread (buf, size, -1, timeout);
        }

        /**
         * Queue a read operation from a specific offset.
         * This method queues a read operation and returns immediately.
         * The reading of data is done asynchronously and when the data
         * is read, or an error occurs, the supplied callback function
         * is called.
         * <br>
         * <b>Note:</b> not all types of connections supports reading
         * from a specific offset.
         *
         * @param buf The buffer where to store the data.
         * @param size The maximum number of bytes to read.
         * @param offset An offset from where to read.
         *               This may not be relevant for all types of connections.
         *               If -1 then the data will be read from the current
         *               position.
         * @param rx_cb If not <code>nullptr</code>, this callback
         *              is called when the read operation has generated
         *              a result (data is read, an error occurred,
         *              the operation is cancelled, or has timed out).
         *              <br/>
         *              If <code>nullptr</code>, the default read
         *              operation callback is called if one is set.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success, -1 if the file descriptor isn't valid.
         *         <br/><b>Note:</b> a return value of 0 means that the
         *         read operation was queued, not that the actual read
         *         operation was successful.
         * @see io_callback_t
         * @see io_result_t
         * @see default_rx_callback
         */
        virtual int pread (void* buf, size_t size, off_t offset, io_callback_t rx_cb, unsigned timeout=-1);

        /**
         * Blocking read into a buffer from a specific offset.
         * This method blocks until a read result is obtained
         * or the timeout expores.
         * <br>
         * <b>Note:</b> not all types of connections supports
         * reading from a specific offset.
         *
         * @param buf The buffer where to store the data.
         * @param size The maximum number of bytes to read.
         * @param offset An offset from where to read.
         *               This may not be relevant for all types of connections.
         *               If -1 then the data will be read from the current
         *               position.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return Number of bytes read, or -1 on failure.
         *         On failure, <code>errno</code> is set.
         */
        virtual ssize_t pread (void* buf, size_t size, off_t offset, unsigned timeout=-1);

        /**
         * Queue a write operation.
         * This method queues a write operation and returns immediately.
         * The writing of data is done asynchronously and when the data
         * is written, or an error occurs, the supplied callback function
         * is called.
         * @param buf The buffer from where to write data.
         * @param size The maximum number of bytes to write.
         * @param tx_cb If not <code>nullptr</code>, this callback
         *              is called when the write operation has generated
         *              a result (data is written, an error occurred, or
         *              the operation has timed out).
         *              <br/>
         *              If <code>nullptr</code>, the default write
         *              operation callback is called if one is set.
         * @param timeout A timeout in milliseconds. -1, no timeout is set.
         * @return 0 on success, -1 if the file descriptor isn't valid.
         *         <br/><b>Note:</b> a return value of 0 means that the
         *         write operation was queued, not that the actual write
         *         operation was successful.
         * @see io_callback_t
         * @see io_result_t
         * @see default_tx_callback
         */
        int write (const void* buf, size_t size, io_callback_t tx_cb, unsigned timeout=-1) {
            return this->pwrite (buf, size, -1, tx_cb, timeout);
        }

        /**
         * Synchromized write data from a buffer.
         * This method blocks until the write operation is finished, cancelled,
         * timed out, or an error occurs.
         * @param buf The buffer from where to write data.
         * @param size The maximum number of bytes to write.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return The number of bytes read, or -1 on error.
         *         <br/>
         *         On error, <code>errno</code> is set to an appropriate value.
         */
        ssize_t write (const void* buf, size_t size, unsigned timeout=-1) {
            return this->pwrite (buf, size, -1, timeout);
        }

        /**
         * Queue a write operation to a specific offset.
         * This method queues a write operation and returns immediately.
         * The writing of data is done asynchronously and when the data
         * is written, or an error occurs, the supplied callback function
         * is called.
         * @param buf The buffer from where to write data.
         * @param size The maximum number of bytes to write.
         * @param offset An offset from where to write.
         *               This may not be relevant for all types of connections.
         *               If -1 then the data will be written from the current
         *               position.
         * @param tx_cb If not <code>nullptr</code>, this callback
         *              is called when the write operation has generated
         *              a result (data is written, an error occurred, or
         *              the operation has timed out).
         *              <br/>
         *              If <code>nullptr</code>, the default write
         *              operation callback is called if one is set.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success, -1 if the file descriptor isn't valid.
         *         <br/><b>Note:</b> a return value of 0 means that the
         *         write operation was queued, not that the actual write
         *         operation was successful.
         * @see io_callback_t
         * @see io_result_t
         * @see default_tx_callback
         */
        virtual int pwrite (const void* buf, size_t size, off_t offset, io_callback_t tx_cb, unsigned timeout=-1);

        /**
         * Synchromized write data from a buffer to a specific offset.
         */
        virtual ssize_t pwrite (const void* buf, size_t size, off_t offset, unsigned timeout=-1);

        /**
         * Wait until data is available for reading.
         * Queue a read operation but don't try to read anything,
         * instead the callback is called when there is data available for reading,
         * the timeout expires, or an error occurs.
         * @param rx_cb A callback that will be called when data is available,
         *              the timeout expires, or an error occurred.
         *              If <code>nullptr</code>, the default read
         *              operation callback is called if one is set.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success, -1 if the file descriptor isn't valid.
         *         <br/>Note that a return value of 0 means that the read operation
         *         was queued, not that the actual read operation was successful.
         * @see io_callback_t
         * @see io_result_t
         * @see default_rx_callback
         */
        virtual int wait_for_rx (io_callback_t rx_cb, unsigned timeout=-1);

        /**
         * Block until data is available for reading.
         * @return 0 if data is available,
         *         otherwise -1 and <code>errno</code> is set.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         */
        virtual int wait_for_rx (unsigned timeout=-1);

        /**
         * Wait until data can be written.
         * Queue a write operation but don't try to write anything, instead the
         * callback is called when data can be written.
         * @param tx_cb A callback that will be called when data can be written,
         *              the timeout expires, or an error occurred.
         *              If <code>nullptr</code>, the default write
         *              operation callback is called if one is set.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 on success, -1 if the file descriptor isn't valid.
         *         <br/>Note that a return value of 0 means that the write operation
         *         was queued, not that the actual write operation was successful.
         * @see io_callback_t
         * @see io_result_t
         * @see default_tx_callback
         */
        virtual int wait_for_tx (io_callback_t tx_cb, unsigned timeout=-1);

        /**
         * Block until data can be written.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return 0 if data can be written,
         *         otherwise -1 and <code>errno</code> is set.
         */
        virtual int wait_for_tx (unsigned timeout=-1);

        /**
         * Do the actual reading from the connection.
         * This method should normally not be called directly,
         * it is called by the iohandler_base when the connection is
         * ready to read data.
         * @param buf A pointer to the memory area where data should be stored.
         * @param size The number of bytes to read.
         * @param offset The offset in the connection where to read.
         *               <br/><i>Not supported by all types of connections.</i>
         * @param errnum The value of <code>errno</code> after
         *               the read operation. Always 0 if no error occurred.
         * @return The number of bytes that was read.
         *         <br/>
         *         On error, -1 is returned and parameter <code>errnum</code>
         *         is set to some appropriate value.
         */
        virtual ssize_t do_read (void* buf, size_t size, off_t offset, int& errnum) = 0;

        /**
         * Do the actual writing to the connection.
         * This method should normally not be called directly,
         * it is called by the iohandler_base when the connection is
         * ready to write data.
         * @param buf A pointer to the memory area that should be written.
         * @param size The number of bytes to write.
         * @param offset The offset in the connection where to write.
         *               <br/><i>Not supported by all types of connections.</i>
         * @param errnum The value of <code>errno</code> after
         *               the write operation. Always 0 if no error occurred.
         * @return The number of bytes that was written.
         *         <br/>
         *         On error, -1 is returned and parameter <code>errnum</code>
         *         is set to some appropriate value.
         */
        virtual ssize_t do_write (const void* buf, size_t size, off_t offset, int& errnum) = 0;


    protected:
        io_callback_t def_rx_cb;
        io_callback_t def_tx_cb;

        // For synchronized I/O
        std::mutex sync_mutex;
        std::condition_variable sync_cond;


    private:
        Connection (const Connection& conn) = delete;
        Connection (Connection&& conn) = delete;
        Connection& operator= (const Connection& c) = delete;
        Connection& operator= (Connection&& c) = delete;
    };


}


#endif
