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
#include <iomultiplex/BufferPool.hpp>
#include <iomultiplex/Log.hpp>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <cerrno>


//#define TRACE_DEBUG

#ifdef TRACE_DEBUG
#define TRACE(format, ...) Log::debug("%s:%s:%d: " format, __FILE__, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#define TRACE(format, ...)
#endif


namespace iomultiplex {


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    BufferPool::BufferPool (const size_t buffer_size,
                            const size_t capacity,
                            const size_t grow_capacity)
        : chunk_size {buffer_size},
          grow_num {grow_capacity},
          top {0}
    {
        if (chunk_size <= 0)
            throw std::invalid_argument ("Invalid chunk_size");

        if (capacity <= 0  &&  grow_num <= 0)
            throw std::invalid_argument ("Invalid capacity");

        if (!grow(capacity))
            throw std::system_error (ENOMEM, std::generic_category());
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    const size_t BufferPool::pool_size ()
    {
        std::lock_guard<std::mutex> lock (mutex);
        return chunk_size * chunks.size();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void* BufferPool::get ()
    {
        std::lock_guard<std::mutex> lock (mutex);

        if (top>=chunks.size()  &&  !grow(grow_num))
            return nullptr;
        else
            return chunks[top++];
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void BufferPool::put (void* buf)
    {
        std::lock_guard<std::mutex> lock (mutex);
        if (!top)
            throw std::runtime_error ("Too many buffers returned to the buffer pool");
        chunks[--top] = static_cast<char*> (buf);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::shared_ptr<char> BufferPool::get_ptr ()
    {
        return std::shared_ptr<char> ((char*)get(), [this](char* chunk){
                if (chunk != nullptr)
                    put (chunk);
            });
    }


    //--------------------------------------------------------------------------
    // Assume mutex is locked
    //--------------------------------------------------------------------------
    bool BufferPool::grow (size_t num)
    {
        if (!num)
            return false;

        std::unique_ptr<char[]> buffer (new char[chunk_size * num]);
        if (!buffer)
            return false;

        try {
            chunks.reserve (chunks.size() + num);
        }catch (...) {
            return false;
        }
        for (size_t i=0; i<num; ++i)
            chunks.push_back (buffer.get() + i*chunk_size);

        buffers.emplace_front (std::move(buffer));

        return true;
    }



}
