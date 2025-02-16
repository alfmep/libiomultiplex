/*
 * Copyright (C) 2021,2022,2025 Dan Arrhenius <dan@ultramarin.se>
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
#ifndef IOMULTIPLEX_POLL_DESCRIPTORS_HPP
#define IOMULTIPLEX_POLL_DESCRIPTORS_HPP

#include <string>
#include <vector>
#include <limits>
#include <tuple>
#include <list>
#include <poll.h>


namespace iomultiplex {


    /**
     * A vector of poll descriptors used by poll() and ppoll().
     * The vector is sorted by file descriptor value. Invalid
     * file descriptors (a negative number) are sorted last
     * in the vector.
     */
    class poll_descriptors {
    public:

        /**
         * Constructor.
         */
        poll_descriptors ();

        /**
         * Destructor.
         */
        ~poll_descriptors () = default;

        /**
         * Return a pointer to an array of <code>struct pollfd</code>.
         * The size of the array is obtained by the method <code>size()</code>.
         * @return A pointer to an array of <code>struct pollfd</code>.
         */
        inline struct pollfd* data () {
            return fd_vect.data ();
        }

        /**
         * Return the number of active poll descriptors in the vector.
         * An active poll descriptor is a poll descriptor
         * with a file descriptor value >= 0.
         * @return The number of active poll descriptors in the vector.
         */
        inline size_t size () const {
            return num_active;
        }

        /**
         * Return the number of poll descriptors in the vector, including both active and inactive.
         * @return The total number of poll descriptors in the vector.
         */
        inline size_t capacity () const {
            return fd_vect.size();
        }

        /**
         * Activate a descriptor in the vector.
         * @param fd The file descriptor to activate.
         * @param events The events to listen on.
         *               Typically a combination of
         *               <code>POLLIN</code> and
         *               <code>POLLOUT</code>.
         * @param merge If true, the current event mask is OR'ed with
         *              the the event mask in <code>events</code>.
         *              If false, the current event mask is replaced
         *              the the event mask in <code>events</code>.
         * @return <code>true</code> if activated, <code>false</code>
         *         if not(invalid file descriptor or already activated with the same events).
         */
        bool activate (int fd, short events, bool merge=false);

        /**
         * Deactivate a descriptor in the vector.
         * @param fd The file descriptor to deactivate.
         * @param events The events to deactivate.
         * @return <code>true</code> if deactivated, <code>false</code> if not(already inactive or invalid file descriptor).
         */
        bool deactivate (int fd, short events=-1);

        /**
         * Deactivate all descriptors in the vector.
         */
        void clear ();


        /**
         * Schedule an operation to activate a descriptor in the vector.
         * The activation is made when a call to commit() is made.
         * @param fd The file descriptor to activate.
         * @param events The events to listen on.
         *               Typically a combination of
         *               <code>POLLIN</code> and
         *               <code>POLLOUT</code>.
         * @param merge If true, the current event mask is OR'ed with
         *              the the event mask in <code>events</code>.
         *              If false, the current event mask is replaced
         *              the the event mask in <code>events</code>.
         * @see commit()
         */
        void schedule_activate (int fd, short events, bool merge=false);

        /**
         * Schedule an operation to deactivate a descriptor in the vector.
         * The deactivation is made when a call to commit() is made.
         * @param fd The file descriptor to deactivate.
         * @param events The events to deactivate.
         * @see commit()
         */
        void schedule_deactivate (int fd, short events=-1);

        /**
         * Commit all activations and deactivations made by calls to
         * schedule_activate() and schedule_deactivate().
         */
        void commit ();


    private:
        std::vector<struct pollfd> fd_vect;
        size_t num_active;

        using op_t = std::tuple<bool,  // activate=true, deactivate=false
                                int,   // file descriptor
                                short, // event mask
                                bool>; // true=merge events, false=overwrite events
        std::list<op_t> commit_list;
    };


}


#endif
