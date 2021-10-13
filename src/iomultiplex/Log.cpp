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
#include <iomultiplex/Log.hpp>
#include <vector>
#include <cstring>


namespace iomultiplex {


    std::mutex       Log::log_mutex;
    std::atomic_uint Log::prio_level {default_prio_level};
    log_callback_t   Log::cb         {default_log_callback};

    static constexpr size_t buf_block_size = 64;


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void Log::set_callback (log_callback_t callback)
    {
        std::lock_guard<std::mutex> lock (log_mutex);
        cb = callback;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void Log::log (unsigned int priority, const char* format, va_list& args)
    {
        static std::vector<char> buf (buf_block_size);

        std::lock_guard<std::mutex> lock (log_mutex);
        if (!cb || !format)
            return;

        int    result;
        size_t buf_size;
        do {
            va_list tmp_args;
            va_copy (tmp_args, args);
            buf_size = buf.size ();
            result = vsnprintf (buf.data(), buf_size, format, tmp_args);
            va_end (tmp_args);

            if (result < 0) {
                snprintf (buf.data(), buf_size, "<invalid log message from libiomultiplex>");
                break;
            }
            else if ((unsigned)result >= buf_size) {
                // Increase buffer size
                buf.resize ((result+buf_block_size) -
                            ((result+buf_block_size) % buf_block_size));
                // Perhaps make a sanity check of the size here ?
            }
        }while ((unsigned)result >= buf_size);

        cb (priority, buf.data());
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void default_log_callback (unsigned int priority, const char* message)
    {
        syslog (static_cast<int>(priority), "%s", message);
    }


}
