/*
 * Copyright (C) 2021-2023,2025 Dan Arrhenius <dan@ultramarin.se>
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
#ifndef IOMULTIPLEX_IOHANDLER_BASE_HPP
#define IOMULTIPLEX_IOHANDLER_BASE_HPP

#include <iomultiplex/types.hpp>
#include <iomultiplex/connection.hpp>
#include <functional>
#include <memory>
#include <atomic>
#include <vector>
#include <thread>
#include <mutex>
#include <list>
#include <map>
#include <set>
#include <ctime>
#include <signal.h>
#include <sys/types.h>


namespace iomultiplex {


    // Forward declaration
    class iohandler_epoll;

    /**
     * Default I/O handler implementation.
     */
    using default_iohandler = iohandler_epoll;


    /**
     * I/O handler.
     * This class is responsible for managing the I/O operations
     * of all the connection objects using it.
     */
    class iohandler_base {
    public:
        /**
         * Constructor.
         */
        iohandler_base () = default;

        /**
         * Destructor.
         * Cancels all pending I/O operations and stops the I/O handling.
         * If a worker thread is running, it is stopped before the destructor
         * returns.
         */
        virtual ~iohandler_base () = default;

        /**
         * Run the I/O handler until stopped with method 'stop' or an error occurrs.
         * This method will block unless a worker thread is requested, in
         * that case this method will return when the worker thread is up and running.
         * @param start_worker_thread If <code>true</code> start a new thread
         *                            to handle the I/O.
         * @return 0 on success, -1 on error and <code>errno</code> is set.
         */
        virtual int run (bool start_worker_thread=false) = 0;

        /**
         * Stop the I/O handler.
         * All pending I/O operations are cancelled.
         * \note If the I/O handler has a worker thread running,
         * the worker thread is signaled to stop and this
         * method returns immediately. To block until the worker
         * thread is stopped, call method <code>join()</code>.
         * @see iohandler_base::join
         */
        virtual void stop () = 0;

        /**
         * Queue a read operation for a connection.
         * A read operation is queued and when the connection
         * have data available it will be read and the supplied
         * callback will be called.
         * \note This method is normally called by a connection
         *       object and not called directly.
         * @param conn The connection object that wants to read data.
         * @param buf The start address to where data is stored.
         * @param size The number of bytes to read.
         * @param rx_cb Callback to be called when there is a result.
         *              If <code>nullptr</code>, no callback is called
         *              when the read operation is finished or has
         *              timed out.
         * @param timeout Timeout in miliseconds.
         * @param dummy_operation If <code>true</code>, no data is
         *                        actually read. The callback will
         *                        be called when there is data available
         *                        to read or if a timeout occurs.
         * @return 0 on success, -1 if the file descriptor isn't valid
         *         the I/O handler is shutting down,
         *         or the file descriptor can't be used in poll/epoll.
         *         <br/><b>Note:</b> A return value of 0 means that the
         *         read operation was queued, not that the actual read
         *         operation was successful.
         */
        inline int read (connection& conn,
                         void* buf,
                         size_t size,
                         io_callback_t rx_cb=nullptr,
                         unsigned timeout=-1,
                         const bool dummy_operation=false)
        {
            return queue_io_op (conn, buf, size, rx_cb,
                                true, dummy_operation, timeout);
        }

        /**
         * Queue a write operation for a connection.
         * A write operation is queued and when the connection
         * is ready to write, it will be written and the supplied
         * callback will be called.
         * \note This method is normally called by a connection
         *       object and not called directly.
         * @param conn The connection object that wants to write data.
         * @param buf The start address from where data is written.
         * @param size The number of bytes to write.
         * @param tx_cb A callback to be called when there is a result.
         *              If <code>nullptr</code>, no callback is called
         *              when the write operation is finished or has
         *              timed out.
         * @param timeout Timeout in miliseconds.
         * @param dummy_operation If <code>true</code>, no data is
         *                        actually written. The callback will
         *                        be called when there is data available
         *                        to write or if a timeout occurs.
         * @return 0 on success, -1 if the file descriptor isn't valid,
         *         the I/O handler is shutting down,
         *         or the file descriptor can't be used in poll/epoll.
         *         <br/><b>Note:</b> a return value of 0 means that the
         *         write operation was queued, not that the actual write
         *         operation was successful.
         */
        inline int write (connection& conn,
                          const void* buf,
                          size_t size,
                          io_callback_t tx_cb=nullptr,
                          unsigned timeout=-1,
                          const bool dummy_operation=false)
        {
            return queue_io_op (conn, const_cast<void*>(buf), size, tx_cb,
                                false, dummy_operation, timeout);
        }

        /**
         * Cancel all input and/or output operations for a connection.
         * This will cancel all I/O operations for the
         * specified connection. The pending I/O operations
         * will have a result of -1 and <code>errnum</code>
         * set to <code>ECANCELED</code>.
         *
         * @note Until all I/O queued operations for the connection
         * are cancelled, trying to queue an I/O operation of the same
         * type (RX and/or TX), for the same connection, will fail with
         * <code>errno</code> being set to <code>ECANCELED</code>.
         *
         * @param conn The connection for which to cancel RX/TX operations.
         * @param cancel_rx If <code>true</code> (default),
         *                  cancel all RX operations.
         * @param cancel_tx If <code>true</code> (default),
         *                  cancel all TX operations.
         * @param fast If <code>true</code>, all queued I/O operations
         *             for this connection are removed immediately
         *             without the associated callback being called.
         */
        virtual void cancel (connection& conn,
                             bool cancel_rx,
                             bool cancel_tx,
                             bool fast) = 0;

        /**
         * Check if the I/O handler is running in the same
         * context(i.e. same thread) as the caller.
         * @return <code>true</code> if the I/O handler is
         *         running in the same thread as the caller.
         */
        virtual bool same_context () const = 0;

        /**
         * If the I/O handler has a worker thread running,
         * block until the worker thread is terminated.
         * If not, return immediately.
         * \note If the I/O handler has a worker thread
         *       running, and this method is called from
         *       within that thread, it will cause a deadlock
         *       and the application will probably terminate
         *       with an error.
         */
        virtual void join () = 0;


    protected:
        /**
         * Queue a single I/O operation.
         * @param conn The connection perforimg the I/O.
         * @param buf A pointer to a data buffer being operated upon.
         * @param size The number of bytes to read/receive or write/send.
         * @param cb A callback that is called when the I/O
         *           operation has a result.
         * @param read <code>true</code> for an input operation,
         *             <code>true</code> for an output operation.
         * @param dummy_operation If <code>true</code>, no data
         *                        is actually read or written.
         * @param timeout Timeout in milliseconds for the I/O operation.
         *                Use -1 for no timeout.
         * @return 0 on success, -1 and <code>errno</code> is
         *         set if the I/O operation couldn't be queued.
         */
        virtual int queue_io_op (connection& conn,
                                 void* buf,
                                 size_t size,
                                 io_callback_t cb,
                                 const bool read,
                                 const bool dummy_operation,
                                 unsigned timeout) = 0;
    };


}
#endif
