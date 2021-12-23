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
#ifndef IOMULTIPLEX_CHUNKADAPTER_HPP
#define IOMULTIPLEX_CHUNKADAPTER_HPP

#include <iomultiplex/Adapter.hpp>
#include <memory>


namespace iomultiplex {


    /**
     */
    class ChunkAdapter : public Adapter {
    public:
        /**
         * Default constructor.
         * Constructs an adapter without a slave connection, and a chunk size of 1.
         */
        ChunkAdapter ();

        /**
         * Constructor.
         * @param conn The slave connection used by this adapter.
         * @param close_on_destruct If <code>true</code>, the
         *                          slave connection used by this adapter
         *                          will be closed in the descructor of
         *                          this object. if <code>false</code>,
         *                          the slave connection will be kept
         *                          open.
         */
        ChunkAdapter (Connection& conn, bool close_on_destruct=false);

        /**
         * Constructor.
         * @param conn_ptr A shared ponter to the slave connection
         *                 used by this adapter. The slave connection
         *                 isn't closed by the adapter in the destructor,
         *                 it will be closed by the slave connection itself
         *                 when the shared pointer calls its destructor.
         */
        ChunkAdapter (std::shared_ptr<Connection> conn_ptr);

        /**
         * Destructor.
         * The slave connection is only closed by this
         * destructor if the constructor was called with
         * parameter <code>close_on_destruct</code> set
         * to <code>true</code>.
         * <br/>In any case the slave connection will
         * be closed when itself is destroyed.
         */
        virtual ~ChunkAdapter () = default;

        /**
         * Queue a read operation to read <code>num_chunks</code> chunks of data.
         * This method queues a read operation and returns immediately.
         * The reading of data is done asynchronously and when the data
         * is read, or an error occurs, the supplied callback function
         * is called.
         * @param buf Where to store the data.
         * @param num_chunks The number of data chunks to read.
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
         */
        virtual int read (void* buf,
                          size_t chunk_size,
                          size_t num_chunks,
                          io_callback_t rx_cb,
                          unsigned timeout=-1);

        /**
         * Synchromized call to read <code>num_chunks</code> chunks of data.
         * This method blocks until the read operation is finished, cancelled,
         * timed out, or an error occurs.
         * @param buf The buffer where to store the data.
         * @param num_chunks The number of data chunks to read.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return The number of chunks read, or -1 on error.
         *         <br/>
         *         On error, <code>errno</code> is set to an appropriate value.
         */
        virtual ssize_t read (void* buf,
                              size_t chunk_size,
                              size_t num_chunks,
                              unsigned timeout=-1);

        /**
         * Queue an operation to wite <code>num_chunks</code> chunks of data.
         * This method queues a write operation and returns immediately.
         * The writing of data is done asynchronously and when the data
         * is written, or an error occurs, the supplied callback function
         * is called.
         * @param buf The buffer from where to write data.
         * @param num_chunks The number of data chunks to write.
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
        virtual int write (const void* buf,
                           size_t chunk_size,
                           size_t num_chunks,
                           io_callback_t tx_cb,
                           unsigned timeout=-1);

        /**
         * Synchromized call to write <code>num_chunks</code> chunks of data.
         * This method blocks until the write operation is finished, cancelled,
         * timed out, or an error occurs.
         * @param buf The buffer from where to write data.
         * @param num_chunks The number of data chunks to write.
         * @param timeout A timeout in milliseconds. If -1, no timeout is set.
         * @return The number of chunks written, or -1 on error.
         *         <br/>
         *         On error, <code>errno</code> is set to an appropriate value.
         */
        virtual ssize_t write (const void* buf,
                               size_t chunk_size,
                               size_t num_chunks,
                               unsigned timeout=-1);

        /**
         * Cancel I/O operations for this connection.
         */
        virtual void cancel (bool cancel_rx=true, bool cancel_tx=true);


    private:
        bool chunk_rx_cb (io_result_t& ior,
                          void* buf,
                          size_t tot_size,
                          size_t cur_size,
                          size_t chunk_size,
                          io_callback_t rx_cb,
                          unsigned timeout);
        bool chunk_tx_cb (io_result_t& ior,
                          void* buf,
                          size_t tot_size,
                          size_t cur_size,
                          size_t chunk_size,
                          io_callback_t io_cb,
                          unsigned timeout);
        std::unique_ptr<char> rx_buf;
        std::unique_ptr<char> tx_buf;
        size_t rx_buf_size;
        size_t tx_buf_size;
        size_t rx_buf_pos;
        size_t tx_buf_pos;
    };


}


#endif
