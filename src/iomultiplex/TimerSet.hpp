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
#ifndef IOMULTIPLEX_TIMERSET_HPP
#define IOMULTIPLEX_TIMERSET_HPP

#include <iomultiplex/types.hpp>
#include <iomultiplex/iohandler_base.hpp>
#include <iomultiplex/TimerConnection.hpp>
#include <functional>
#include <mutex>
#include <tuple>
#include <list>


namespace iomultiplex {


    /**
     * A set of timeouts.
     */
    class TimerSet {
    public:
        /**
         * Callback that is called when timer entries expire.
         * @param ts The TimerSet object owning the timer.
         * @param timer_id The id of the timer entry that has expired.
         *                 There is no need to call <code>cancel</code>
         *                 with this id inside the callback, the timer
         *                 entry is automatically removed when it expires.
         */
        using cb_t = std::function<void(TimerSet& ts, long timer_id)>;

        /**
         * Constructor.
         * @param ioh An iohandler_base object.
         */
        TimerSet (iohandler_base& ioh);

        /**
         * Move constructor.
         * @param ts The TimerSet object to move.
         */
        TimerSet (TimerSet&& ts);

        /**
         * Destructor.
         */
        ~TimerSet ();

        /**
         * Move operator.
         * @param rhs The TimerSet object to move.
         * @return A reference to this object.
         */
        TimerSet& operator= (TimerSet&& rhs);

        /**
         * Check if the timer set is empty.
         * @return true if the timer set has no active timers.
         */
        bool empty () const;

        /**
         * Add a timeout entry.
         * When the timeout expires, the callback is called
         * and the timer entry in the set is removed.
         * @param timeout A timeout value in milliseconds.
         * @param callback The callback to call when the timer expires.
         * @return A timer id that can be used to cancel the timer.
         */
        long set (unsigned timeout, cb_t callback) {
            return set (timeout, 0, callback);
        }

        /**
         * Add a timeout entry.
         * When the timeout expires, the callback is called
         * and the repeat value is used to set the timer again.
         * @param timeout A timeout value in milliseconds.
         * @param repeat A repeat timeout value in milliseconds.
         *               After the initial timeout, the callback
         *               will be called with an interval of
         *               <code>repeat</code> milliseconds.
         *               This will be repeated until method
         *               <code>cancel()</code> is called
         *               or the TimerSet object is destroyed.
         *               <br/>
         *               <b>Note:</b> If the repeat value is 0,
         *               this method behaves as if method
         *               <code>add(timeout, callback)</code> is
         *               called. The timeout entry is removed
         *               from the set when the initial timeout
         *               occurs.
         * @param callback The callback to call when the timer expires.
         * @return A timer id that can be used to cancel the timer.
         */
        long set (unsigned timeout, unsigned repeat, cb_t callback);

        /**
         * Cancel and remove a specific timer entry.
         * @param id The ID of the timer entry to cancel.
         *           If the id doesn't exist in the set,
         *           nothing is done.
         */
        void cancel (long id);

        /**
         * Cancel and remove all timer entries.
         */
        void clear ();


    private:
        using timer_entry_t = std::tuple<long, struct timespec, unsigned, cb_t>;
        using timers_t = std::list<timer_entry_t>;

        void timer_expired (long id);
        TimerConnection timer;
        mutable std::mutex mutex;
        long next_id;
        timers_t times; // Sorted on itimerspec
    };



}
#endif
