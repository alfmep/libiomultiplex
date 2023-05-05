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
#ifndef IOMULTIPLEX_BUFFERPOOL_HPP
#define IOMULTIPLEX_BUFFERPOOL_HPP

#include <memory>
#include <mutex>
#include <vector>
#include <forward_list>
#include <system_error>
#include <cstddef>


namespace iomultiplex {

    /**
     * A fast memory buffer pool.
     * \note All buffers returned from this pool using method <code>get_ptr</code>
     *       <b>must</b> be deleted before the memory pool object itself is destroyed.
     *
     */
    class BufferPool {
    public:
        static constexpr size_t default_capacity {16}; /**< Default number of buffers in the buffer pool (16). */

        /**
         * Construct a memory buffer pool.
         * Initially all buffers are cleared (filled with zeroes).
         * @param chunk_size The size in bytes of each memory buffer in the pool.
         * @param capacity The number of buffers in the buffer pool.
         * @param grow_capacity If the pool is empty, increase it with this
         *                      number of buffers, each with size <code>chunk_size</code>.
         *                      <br/>If 0, the pool will return <code>nullptr</code> when
         *                      it is empty.
         * @throw std::invalid_argument If <code>chunk_size</code> is 0, or if both
         *                              <code>capacity</code> and <code>grow_capacity</code>
         *                              are 0.
         * @throw std::system_error If memory for the pool can't be allocated.
         * @see default_capacity
         */
        BufferPool (const size_t chunk_size,
                    const size_t capacity=default_capacity,
                    const size_t grow_capacity=0);

        /**
         * Get a memory buffer from the buffer pool.
         * \note The returned buffer is not cleared and may contain
         *       whatever the last user of the buffer filled it with.
         * @return A pointer to a memory buffer.
         *         If no more buffers are available, or not enough menory
         *         to grow the buffer pool,
         *         <code>nullptr</code> is returned.
         */
        void* get ();

        /**
         * Put back a buffer into the pool.
         * \note No error checking is made to validate that the buffer
         *       to put back actually belongs to the buffer pool, or
         *       if the same buffer is but back more than once in a row.
         * @param buf The buffer to put back to the pool.
         * @throw std::runtime_error If too many buffers are returned to the pool.
         */
        void put (void* buf);

        /**
         * Get a shared pointer to a memory buffer from the buffer pool.
         * Do not call the <code>put()</code> method for buffers
         * returned by this method. The buffer will be put back to
         * the pool when the shared pointer object is destroyed.
         * \note The returned buffer is not cleared and may contain
         *       whatever the last user of the buffer filled it with.
         * \note All shared pointers returned by this method <b>must</b>
         *       be destroyed before the buffer pool itself is destroyed.
         * @return A shared pointer to a memory buffer.
         *         If no more buffers are available, a
         *         <code>nullptr</code> pointer is returned.
         */
        std::shared_ptr<char> get_ptr ();

        /**
         * Get the size in bytes of the buffers returned from this pool.
         * @return A buffer size in bytes.
         */
        const size_t buf_size () const {
            return chunk_size;
        }

        /**
         * Return the total amount of buffer memory allocated by the buffer pool.
         * @return Total amount of allocated memory in bytes.
         */
        const size_t pool_size ();


    private:
        const size_t chunk_size;
        const size_t grow_num;
        size_t top;
        std::vector<char*> chunks;
        std::forward_list<std::unique_ptr<char[]>> buffers;
        std::mutex mutex;
        bool grow (size_t num);
    };

}



#endif
