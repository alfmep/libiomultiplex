/*
 * Copyright (C) 2021-2023,2025 Dan Arrhenius <dan@ultramarin.se>
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
#include <iomultiplex/types.hpp>
#include <iomultiplex/timer_set.hpp>


namespace iomultiplex {

    // Tuple index constants
    static constexpr int idx_id      = 0;
    static constexpr int idx_timeout = 1;
    static constexpr int idx_repeat  = 2;
    static constexpr int idx_cb      = 3;


    static bool compare_timer_entry (const std::tuple<long, struct timespec, unsigned, timer_set::cb_t>& lhs_timespec,
                                     const std::tuple<long, struct timespec, unsigned, timer_set::cb_t>& rhs_timespec)
    {
        const struct timespec& lhs = std::get<idx_timeout> (lhs_timespec);
        const struct timespec& rhs = std::get<idx_timeout> (rhs_timespec);
        if (lhs.tv_sec != rhs.tv_sec)
            return lhs.tv_sec < rhs.tv_sec;
        else
            return lhs.tv_nsec < rhs.tv_nsec;
    };



    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static void inc_abs_time (timespec& abs, unsigned msec)
    {
        while (msec >= 1000) {
            ++abs.tv_sec;
            msec -= 1000;
        }
        abs.tv_nsec += msec * 1000000;
        if (abs.tv_nsec >= 1000000000L) {
            ++abs.tv_sec;
            abs.tv_nsec -= 1000000000L;
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    timer_set::timer_set (iohandler_base& ioh)
        : timer (ioh, CLOCK_BOOTTIME),
          next_id {-1}
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    timer_set::timer_set (timer_set&& ts)
        : timer (std::move(ts.timer)),
          next_id (ts.next_id),
          times (std::move(ts.times))
    {
        ts.next_id = -1;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    timer_set::~timer_set ()
    {
        clear ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    timer_set& timer_set::operator= (timer_set&& rhs)
    {
        if (this != &rhs) {
            timer = std::move (rhs.timer);
            next_id = rhs.next_id;
            times = std::move (rhs.times);
            rhs.next_id = -1;
        }
        return *this;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool timer_set::empty () const
    {
        std::lock_guard<std::mutex> lock (mutex);
        return times.empty ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    long timer_set::set (unsigned timeout, unsigned repeat, cb_t callback)
    {
        // Get the absolute time of the timeout entry
        struct timespec abs;
        clock_gettime (CLOCK_BOOTTIME, &abs);
        inc_abs_time (abs, timeout);

        // Sanity check
        if (!callback)
            return -1;

        std::lock_guard<std::mutex> lock (mutex);

        // Find where in the list to put the timer entry
        auto pos = times.begin ();
        timespec_less_t t_less;
        for (; pos!=times.end(); ++pos) {
            struct timespec& ts = std::get<idx_timeout> (*pos);
            if (t_less(abs, ts))
                break;
        }
        bool activate_timer = pos == times.begin();

        // Create the timer entry
        times.emplace (pos, std::make_tuple(++next_id, abs, repeat, callback));

        // Activate the timer if needed
        if (activate_timer) {
            timer.set_abs (abs, [this, id=next_id](){
                    timer_expired (id);
                });
        }
        return next_id;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void timer_set::cancel (long id)
    {
        std::lock_guard<std::mutex> lock (mutex);

        auto pos = times.begin ();
        for (; pos!=times.end(); ++pos) {
            if (id == std::get<idx_id>(*pos))
                break;
        }
        if (pos == times.end())
            return;

        bool reset_timer = pos == times.begin();
        times.erase (pos);

        if (times.empty()) {
            timer.cancel (true, true, true);
        }
        else if (reset_timer) {
            auto first = times.begin ();
            auto& abs_time = std::get<idx_timeout> (*first);
            auto id = std::get<idx_id> (*first);
            timer.set_abs (abs_time, [this, id](){
                    timer_expired (id);
                });
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void timer_set::clear ()
    {
        std::lock_guard<std::mutex> lock (mutex);
        timer.cancel (true, true, true);
        times.clear ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void timer_set::timer_expired (long id)
    {
        mutex.lock ();

        // Find the timer entry
        auto entry = times.begin ();
        for (; entry!=times.end(); ++entry) {
            if (id == std::get<idx_id>(*entry))
                break;
        }
        if (entry == times.end()) {
            mutex.unlock ();
            return;
        }

        auto callback = std::get<idx_cb> (*entry);

        if (std::get<idx_repeat>(*entry) != 0) {
            inc_abs_time (std::get<idx_timeout>(*entry), std::get<idx_repeat>(*entry));
            times.sort (compare_timer_entry);
        }else{
            // Remove the timer entry
            times.erase (entry);
        }

        // Reactivate the timer if we have more timer entries
        if (!times.empty()) {
            auto first = times.begin ();
            auto& abs_time = std::get<idx_timeout> (*first);
            auto id = std::get<idx_id> (*first);
            timer.set_abs (abs_time, [this, id](){
                    timer_expired (id);
                });
        }

        // Invoke the callback
        mutex.unlock ();
        callback (*this, id);
    }



}
