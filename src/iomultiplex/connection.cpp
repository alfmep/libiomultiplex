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
#include <iomultiplex/connection.hpp>
#include <iomultiplex/iohandler_base.hpp>
#include <cerrno>
#include <unistd.h>


namespace iomultiplex {



    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void connection::default_rx_callback (io_callback_t rx_cb)
    {
        def_rx_cb = rx_cb;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void connection::default_tx_callback (io_callback_t tx_cb)
    {
        def_tx_cb = tx_cb;
    }


    //--------------------------------------------------------------------------
    // Asynchronized operation
    //--------------------------------------------------------------------------
    int connection::read (void* buf,
                          size_t size,
                          io_callback_t rx_cb,
                          unsigned timeout)
    {
        // Queue a read operation
        return io_handler().read (*this,
                                  buf,
                                  size,
                                  rx_cb != nullptr ? rx_cb : def_rx_cb,
                                  timeout);
    }


    //--------------------------------------------------------------------------
    // Synchronized operation
    // (assumes the I/O handler running in another thread)
    //--------------------------------------------------------------------------
    ssize_t connection::read (void* buf,
                              size_t size,
                              unsigned timeout)
    {
        if (io_handler().same_context()) {
            errno = EDEADLK;
            return -1;
        }
        ssize_t result = -1;
        bool io_done = false;
        int errnum = 0;
        // Queue a read operation
        if (read(buf,
                 size,
                 [this, &result, &io_done, &errnum](io_result_t& ior)->bool{
                     // Called from iohandler_base
                     std::unique_lock<std::mutex> lock (sync_mutex);
                     result = ior.result;
                     errnum = ior.errnum;
                     io_done = true;
                     sync_cond.notify_one ();
                     return false;
                 },
                 timeout) == 0)
        {
            // Wait for the read operation to finish or timeout
            std::unique_lock<std::mutex> lock (sync_mutex);
            sync_cond.wait (lock, [&io_done]{return io_done;});
            errno = errnum;
        }
        return result;
    }


    //--------------------------------------------------------------------------
    // Asynchronized operation
    //--------------------------------------------------------------------------
    int connection::write (const void* buf,
                           size_t size,
                           io_callback_t tx_cb,
                           unsigned timeout)
    {
        // Queue a write operation
        return io_handler().write (*this,
                           buf,
                           size,
                           tx_cb != nullptr ? tx_cb : def_tx_cb,
                           timeout);
    }


    //--------------------------------------------------------------------------
    // Synchronized operation
    // (assumes the I/O handler running in another thread)
    //--------------------------------------------------------------------------
    ssize_t connection::write (const void* buf,
                               size_t size,
                               unsigned timeout)
    {
        if (io_handler().same_context()) {
            errno = EDEADLK;
            return -1;
        }
        ssize_t result = -1;
        bool io_done = false;
        int errnum = 0;
        // Queue a write operation
        if (write(buf,
                  size,
                  [this, &result, &io_done, &errnum](io_result_t& ior)->bool{
                      // Called from iohandler_base
                      std::unique_lock<std::mutex> lock (sync_mutex);
                      result = ior.result;
                      errnum = ior.errnum;
                      io_done = true;
                      sync_cond.notify_one ();
                      return false;
                  },
                  timeout) == 0)
        {
            // Wait for the write operation to finish or timeout
            std::unique_lock<std::mutex> lock (sync_mutex);
            sync_cond.wait (lock, [&io_done]{return io_done;});
            errno = errnum;
        }
        return result;
    }


    //--------------------------------------------------------------------------
    // Asynchronized operation
    //--------------------------------------------------------------------------
    int connection::wait_for_rx (io_callback_t rx_cb, unsigned timeout)
    {
        // Queue a dummy read operation
        return io_handler().read (*this,
                          nullptr,
                          0,
                          rx_cb != nullptr ? rx_cb : def_rx_cb,
                          timeout,
                          true);
    }


    //--------------------------------------------------------------------------
    // Synchronized operation
    // (assumes the I/O handler running in another thread)
    //--------------------------------------------------------------------------
    int connection::wait_for_rx (unsigned timeout)
    {
        if (io_handler().same_context()) {
            errno = EDEADLK;
            return -1;
        }
        int retval = -1;
        int errnum = 0;
        bool io_done = false;
        // Queue a dummy read operation
        if (wait_for_rx([this, &retval, &errnum, &io_done](io_result_t& ior)->bool{
                    // Called from iohandler_base
                    std::unique_lock<std::mutex> lock (sync_mutex);
                    retval = ior.result;
                    errnum = ior.errnum;
                    sync_cond.notify_one ();
                    io_done = true;
                    return false;
                },
                timeout) == 0)
        {
            // Dummy read operation queued, wait for result
            std::unique_lock<std::mutex> lock (sync_mutex);
            sync_cond.wait (lock, [&io_done]{return io_done;});
            errno = errnum;
        }
        return retval;
    }


    //--------------------------------------------------------------------------
    // Asynchronized operation
    //--------------------------------------------------------------------------
    int connection::wait_for_tx (io_callback_t tx_cb, unsigned timeout)
    {
        // Queue a dummy write operation
        return io_handler().write (*this,
                           nullptr,
                           0,
                           tx_cb != nullptr ? tx_cb : def_tx_cb,
                           timeout,
                           true);
    }


    //--------------------------------------------------------------------------
    // Synchronized operation
    // (assumes the I/O handler running in another thread)
    //--------------------------------------------------------------------------
    int connection::wait_for_tx (unsigned timeout)
    {
        if (io_handler().same_context()) {
            errno = EDEADLK;
            return -1;
        }
        int retval = -1;
        int errnum = 0;
        bool io_done = false;
        if (wait_for_tx([this, &retval, &errnum, &io_done](io_result_t& ior)->bool{
                    // Called from iohandler_base
                    std::unique_lock<std::mutex> lock (sync_mutex);
                    retval = ior.result;
                    errnum = ior.errnum;
                    io_done = true;
                    sync_cond.notify_one ();
                    return false;
                },
                timeout) == 0)
        {
            // Dummy write operation queued, wait for result
            std::unique_lock<std::mutex> lock (sync_mutex);
            sync_cond.wait (lock, [&io_done]{return io_done;});
            errno = errnum;
        }
        return retval;
    }


}
