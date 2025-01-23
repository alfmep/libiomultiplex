/*
 * Copyright (C) 2021,2023 Dan Arrhenius <dan@ultramarin.se>
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
#include <iomultiplex/ChunkAdapter.hpp>
#include <iomultiplex/iohandler_base.hpp>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <cerrno>
#include <cstring>


namespace iomultiplex {


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool ChunkAdapter::chunk_rx_cb (io_result_t& ior,
                                    void* buf, // The original read buffer
                                    size_t tot_size,
                                    size_t cur_size,
                                    size_t chunk_size,
                                    io_callback_t io_cb,
                                    unsigned timeout)
    {
        if (ior.result <= 0) {
            ior.buf = buf; // Return the original buf pointer
            // Let's take what we have in our buffer, if any
            ssize_t result = (ssize_t) (cur_size / chunk_size);
            if (result) {
                ior.result = cur_size / chunk_size; // Number of read chunks
                ior.size = (size_t)ior.result * chunk_size; // Number of bytes resd
            }
            if (cur_size > (size_t)ior.size) {
                // We have leftover data, save it for later.
                size_t leftover = cur_size - (size_t)ior.size;
                rx_buf_pos = 0;
                rx_buf_size = leftover;
                rx_buf.reset (new char[rx_buf_size]);
                memcpy (rx_buf.get(), ((char*)ior.buf)+ior.size, leftover);
            }

            if (ior.result > 0 && ior.errnum == EAGAIN)
                ior.errnum = 0;
            return io_cb ? io_cb(ior) : false;
        }
        else if (cur_size + (size_t)ior.result == tot_size) {
            // We have just enough data
            ior.buf = buf;
            ior.size = tot_size;
            ior.result = tot_size / chunk_size;
            return io_cb ? io_cb(ior) : false;
        }
        else {
            // We need more data !
            cur_size += (size_t) ior.result;
            Adapter::read (((char*)ior.buf) + ior.result,
                           tot_size-cur_size,
                           [this,
                            buf,
                            tot_size,
                            cur_size,
                            chunk_size,
                            io_cb,
                            timeout](io_result_t& ior)->bool
                               {
                                   return chunk_rx_cb(ior,
                                                      buf,
                                                      tot_size,
                                                      cur_size,
                                                      chunk_size,
                                                      io_cb,
                                                      timeout);
                               },
                           timeout);
        }
        return true;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ChunkAdapter::ChunkAdapter ()
        : rx_buf_size (0),
          tx_buf_size (0),
          rx_buf_pos  (0),
          tx_buf_pos  (0)
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ChunkAdapter::ChunkAdapter (Connection& conn, bool close_on_destruct)
        : Adapter (conn, close_on_destruct),
          rx_buf_size (0),
          tx_buf_size (0),
          rx_buf_pos  (0),
          tx_buf_pos  (0)
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ChunkAdapter::ChunkAdapter (std::shared_ptr<Connection> conn_ptr)
        : Adapter (conn_ptr),
          rx_buf_size (0),
          tx_buf_size (0),
          rx_buf_pos  (0),
          tx_buf_pos  (0)
    {
    }


    //--------------------------------------------------------------------------
    // Async read
    //--------------------------------------------------------------------------
    int ChunkAdapter::read (void* buf,
                            size_t chunk_size,
                            size_t num_chunks,
                            io_callback_t rx_cb,
                            unsigned timeout)
    {
        if (!slave) {
            errno = EBADF;
            return -1;
        }

        size_t tot_size = chunk_size * num_chunks;
        size_t cur_size = 0;

        if (rx_buf && rx_buf_size) {
            // We have leftover data from the last read
            size_t bytes_in_buf = rx_buf_size - rx_buf_pos;
            if (bytes_in_buf >= tot_size) {
                // Enough data in the buffer from the last read
                memcpy (buf, rx_buf.get()+rx_buf_pos, tot_size);
                rx_buf_pos += tot_size;
                if (rx_buf_pos >= rx_buf_size) {
                    rx_buf_size = 0;
                    rx_buf_pos = 0;
                    rx_buf.reset ();
                }
                if (rx_cb) {
                    // Make a dummy read for the callback to be
                    // called in the I/O handler context.
                    wait_for_rx ([buf, tot_size, num_chunks, rx_cb](io_result_t& ior)->bool{
                                     ior.buf = buf;
                                     ior.size = tot_size;
                                     ior.result = num_chunks;
                                     ior.errnum = 0;
                                     return rx_cb (ior);
                                 }, 0);
                }
                return 0;
            }else{
                // We have some of the data requested in our
                // buffer from the last read.
                memcpy (buf, rx_buf.get()+rx_buf_pos, bytes_in_buf);
                rx_buf_size = 0;
                rx_buf_pos = 0;
                rx_buf.reset ();
                cur_size = bytes_in_buf;
            }
        }

        return Adapter::read (((char*)buf) + cur_size,
                              tot_size - cur_size,
                              [this,
                               buf,
                               tot_size,
                               cur_size,
                               chunk_size,
                               rx_cb,
                               timeout](io_result_t& ior)->bool
                                  {
                                      return chunk_rx_cb(ior,
                                                         buf,
                                                         tot_size,
                                                         cur_size,
                                                         chunk_size,
                                                         rx_cb,
                                                         timeout);
                                  },
                              timeout);
    }


    //--------------------------------------------------------------------------
    // Sync read
    //--------------------------------------------------------------------------
    ssize_t ChunkAdapter::read (void* buf,
                                size_t chunk_size,
                                size_t num_chunks,
                                unsigned timeout)
    {
        if (!slave) {
            errno = EBADF;
            return -1;
        }

        if (io_handler().same_context()) {
            errno = EDEADLK;
            return -1;
        }
        ssize_t result = -1;
        bool io_done = false;
        int errnum = 0;
        // Queue a read operation
        if (read(buf,
                 chunk_size,
                 num_chunks,
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
    //--------------------------------------------------------------------------
    bool ChunkAdapter::chunk_tx_cb (io_result_t& ior,
                                    void* buf, // The original write buffer
                                    size_t tot_size,
                                    size_t cur_size,
                                    size_t chunk_size,
                                    io_callback_t io_cb,
                                    unsigned timeout)
    {
        if (ior.result <= 0) {
            ior.buf = buf; // Return the original buf pointer
            ssize_t result = (ssize_t) (cur_size / chunk_size);
            if (result) {
                ior.result = cur_size / chunk_size; // Number of written chunks
                ior.size = cur_size; //(size_t)ior.result * chunk_size; // Number of bytes written
            }
            if (ior.result > 0 && ior.errnum == EAGAIN)
                ior.errnum = 0;
            return io_cb ? io_cb(ior) : false;
        }
        else if (cur_size + (size_t)ior.result >= tot_size) {
            // Just enought data written
            ior.buf = buf;
            ior.size = tot_size;
            ior.result = tot_size / chunk_size;
            return io_cb ? io_cb(ior) : false;
        }
        else {
            // Write more data
            cur_size += (size_t) ior.result;
            return Adapter::write (((char*)buf)+cur_size,
                                   tot_size-cur_size,
                                   [this, buf, tot_size, cur_size, chunk_size, io_cb, timeout](io_result_t& ior)->bool
                                   {
                                       return chunk_tx_cb(ior,
                                                          buf,
                                                          tot_size,
                                                          cur_size,
                                                          chunk_size,
                                                          io_cb,
                                                          timeout);
                                   },
                                   timeout);
        }
        return false;
    }


    //--------------------------------------------------------------------------
    // Async write
    //--------------------------------------------------------------------------
    int ChunkAdapter::write (const void* buf,
                             size_t chunk_size,
                             size_t num_chunks,
                             io_callback_t tx_cb,
                             unsigned timeout)
    {
        if (!slave) {
            errno = EBADF;
            return -1;
        }

        size_t tot_size = chunk_size * num_chunks;
        return Adapter::write (buf,
                               tot_size,
                               [this, tot_size, chunk_size, tx_cb, timeout](io_result_t& ior)->bool
                                   {
                                       return chunk_tx_cb(ior,
                                                          ior.buf,
                                                          tot_size,
                                                          0,
                                                          chunk_size,
                                                          tx_cb,
                                                          timeout);
                                   },
                               timeout);
    }


    //--------------------------------------------------------------------------
    // Sync write
    //--------------------------------------------------------------------------
    ssize_t ChunkAdapter::write (const void* buf,
                                 size_t chunk_size,
                                 size_t num_chunks,
                                 unsigned timeout)
    {
        if (!slave) {
            errno = EBADF;
            return -1;
        }

        if (io_handler().same_context()) {
            errno = EDEADLK;
            return -1;
        }
        ssize_t result = -1;
        bool io_done = false;
        int errnum = 0;
        // Queue a write operation
        if (write(buf,
                  chunk_size,
                  num_chunks,
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
    //--------------------------------------------------------------------------
    void ChunkAdapter::cancel (bool cancel_rx,
                               bool cancel_tx,
                               bool fast)
    {
        if (cancel_rx) {
            if (rx_buf)
                rx_buf.reset ();
            rx_buf_size = 0;
            rx_buf_pos = 0;
        }
        if (cancel_tx) {
            if (tx_buf)
                tx_buf.reset ();
            tx_buf_size = 0;
            tx_buf_pos = 0;
        }
        Adapter::cancel (cancel_rx, cancel_tx, fast);
    }


}
