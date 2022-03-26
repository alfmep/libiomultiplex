/*
 * Copyright (C) 2021,2022 Dan Arrhenius <dan@ultramarin.se>
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
#ifndef IOMULTIPLEX_TYPES_HPP
#define IOMULTIPLEX_TYPES_HPP

#include <functional>
#include <ctime>


namespace iomultiplex {


    // Forward declaration
    class io_result_t;


    /**
     * I/O callback.
     * @param ior I/O operation result.
     * @return <code>true</code> if the I/O handler should continue to try handling
     *         I/O operations on this connection before waiting for new events.
     */
    using io_callback_t = std::function<bool (io_result_t& ior)>;


    /**
     * Functor for comparing objects of type <code>struct timespec</code>.
     */
    struct timespec_less_t {
        /**
         * Compare two <code>struct timespec</code> objects to see if one is less than the other (lhs<rhs).
         * @param lhs The <code>struct timespec</code> on the left hand side of the comparison.
         * @param rhs The <code>struct timespec</code> on the right hand side of the comparison.
         * @return <code>true</code> if <code>lhs</code> is less than <code>rhs</code>.
         */
        bool operator() (const struct timespec& lhs, const struct timespec& rhs) const {
            if (lhs.tv_sec != rhs.tv_sec)
                return lhs.tv_sec < rhs.tv_sec;
            else
                return lhs.tv_nsec < rhs.tv_nsec;
        }
    };



}
#endif
