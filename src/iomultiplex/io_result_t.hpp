/*
 * Copyright (C) 2021,2023,2025 Dan Arrhenius <dan@ultramarin.se>
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
#ifndef IOMULTIPLEX_IO_RESULT_T_HPP
#define IOMULTIPLEX_IO_RESULT_T_HPP

#include <cstddef>
#include <unistd.h>


namespace iomultiplex {

    // Forward declaration.
    class connection;

    /**
     * Input/output operation result.
     * This contains the result of an I/O operation.
     */
    class io_result_t {
    public:
        connection& conn;   /**< The connection that requested the read/write operation. */
        void*       buf;    /**< The buffer that was read to/written from. */
        size_t      size;   /**< The requested number of bytes to read/write. */
        ssize_t     result; /**< The number of bytes that was read/written, or -1 on error.
                             *   A value of 0 is allowed, normally meaning the
                             *   end of the file/stream was encountered.<br/>
                             *   On a timeout, <code>result</code> is -1 and
                             *   <code>errnum</code> is set to ETIMEDOUT.
                             */
        int         errnum; /**< The value of <code>errno</code> after the read/write operation. */
        unsigned    timeout;/**< The original timeout value in milliseconds. If -1, no timeout was set. */

        io_result_t (connection& c, void* b, size_t s, ssize_t r, int e, unsigned t)
            : conn {c},
              buf {b},
              size {s},
              result {r},
              errnum {e},
              timeout {t}
            {
            }

        virtual ~io_result_t () = default;
    };

}


#endif
