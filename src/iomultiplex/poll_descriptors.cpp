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
#include <iomultiplex/poll_descriptors.hpp>
#include <iomultiplex/log.hpp>
#include <algorithm>


namespace iomultiplex {


    static auto compare_pollfd = [](const struct pollfd& lhs,
                                    const struct pollfd& rhs)->bool
    {
        // Invalid file descriptors (-1) will
        // be placed last when sorted.
        return (unsigned)lhs.fd < (unsigned)rhs.fd;
    };


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static inline void sort (std::vector<struct pollfd>& fd_vect)
    {
        std::sort (fd_vect.begin(), fd_vect.end(), compare_pollfd);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    poll_descriptors::poll_descriptors ()
        : num_active {0}
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool poll_descriptors::activate (int fd, short events, bool merge)
    {
        // Sanity check
        if (fd < 0)
            return false;

        if (events == 0) {
            if (merge == false)
                deactivate (fd);
            return true;
        }

        events |= POLLHUP | POLLERR | POLLNVAL;
        struct pollfd key {fd, events, 0};

        auto end = fd_vect.begin() + num_active;

        // Check if the file descriptor is already active.
        //
        auto pos = std::lower_bound (fd_vect.begin(), end, key, compare_pollfd);
        if (pos != end  &&  pos->fd == fd) {
            short new_events = merge ? pos->events|events : events;
            // The file descriptor is already active,
            // update the event masks.
            if (pos->events == new_events) {
                // No change in the event mask.
                // Return false to indicate that
                // no change was made in the
                // poll descriptor set.
                return false;
            }
            pos->events = new_events;
            pos->revents &= pos->events;
            return true;
        }

        // Not active, activate it
        //
        ++num_active;

        if (end == fd_vect.end()) {
            // All descriptors in the fd_vect are
            // active, the vector needs to grow.
            fd_vect.push_back (key);
            if (pos != end) {
                // The file descriptor was pushed back at
                // the vector, but there is at least one
                // file descriptor with a higher value.
                // The vector needs to be sorted.
                sort (fd_vect);
            }
        }
        else if (pos->fd == -1) {
            // If the position is at the end of the activated descriptors,
            // it is already sorted after activating the new descriptor.
            pos->fd = fd;
            pos->events = events;
            pos->revents = 0;
        }else{
            // Put it at the end of the active descriptors and sort the fd_vect
            end->fd = fd;
            end->events = events;
            end->revents = 0;
            sort (fd_vect);
        }

        return true;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool poll_descriptors::deactivate (int fd, short events)
    {
        // Sanity check
        if (num_active==0 || fd<0)
            return false;

        struct pollfd key {.fd=fd};
        auto end = fd_vect.begin() + num_active;

        auto pos = std::lower_bound (fd_vect.begin(), end, key, compare_pollfd);
        if (pos == end  ||  pos->fd != fd)
            return false;

        pos->events &= ~events;
        pos->revents &= ~events;

        if (pos->events==0  ||  pos->events==(POLLHUP|POLLERR|POLLNVAL)) {
            pos->fd = -1;
            if (--num_active > 0)
                sort (fd_vect);
        }

        return true;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void poll_descriptors::clear ()
    {
        fd_vect.clear ();
        num_active = 0;
        commit_list.clear ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void poll_descriptors::schedule_activate (int fd, short events, bool merge)
    {
        commit_list.emplace_back (true, fd, events, merge);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void poll_descriptors::schedule_deactivate (int fd, short events)
    {
        commit_list.emplace_back (false, fd, events, false);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void poll_descriptors::commit ()
    {
        for (auto& op : commit_list) {
            if (std::get<0>(op)) {
                activate (std::get<1>(op),  // fd
                          std::get<2>(op),  // events
                          std::get<3>(op)); // merge?
            }else{
                deactivate (std::get<1>(op),  // fd
                            std::get<2>(op)); // events
            }
        }
        commit_list.clear ();
    }


}
