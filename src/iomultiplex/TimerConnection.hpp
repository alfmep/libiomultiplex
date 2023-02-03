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
#ifndef IOMULTIPLEX_TIMERCONNECTION_HPP
#define IOMULTIPLEX_TIMERCONNECTION_HPP

#include <iomultiplex/FdConnection.hpp>
#include <functional>
#include <cstdint>


namespace iomultiplex {

    // Forward declaration
    class iohandler_base;

    /**
     * A timer that deliveres timer expiration notifications via a callback.
     * @see TimerSet
     */
    class TimerConnection : public FdConnection {
    public:

        /**
         * Creates a timer object.
         * @param io_handler An iohandler_base object responsible for the I/O handling.
         * @param clock_id The clock to use.
         */
        TimerConnection (iohandler_base& io_handler, int clock_id=CLOCK_BOOTTIME);

        /**
         * Move constructor.
         * @param rhs The TimerConnection object to move.
         */
        TimerConnection (TimerConnection&& rhs);

        /**
         * Destructor.
         */
        virtual ~TimerConnection () = default;

        /**
         * Move operator.
         * @param rhs The TimerConnection object to move.
         * @return A reference to this object.
         */
        TimerConnection& operator= (TimerConnection&& rhs);

        /**
         * Activate the timer.
         * @param timeout_ms Timeout in milliseconds.
         *                   If 0, the timeout will expire as soon as possible.
         * @param callback The callback function to call when the time expires.
         * @return 0 on success.
         *         If the timer can't be activated, -1 is returned
         *         and <code>errno</code> is set.
         */
        int set (unsigned timeout_ms, std::function<void()> callback) {
            return set (timeout_ms, 0, callback);
        }

        /**
         * Activate the timer.
         * @param timeout_ms Initial timeout in milliseconds.
         *                   If 0, the timeout will expire as soon as possible.
         * @param repeat_ms If set to other than 0, after the initial timeout
         *                  has expired, the callback will repeatedly be called
         *                  with an interval of the number of milliseconds in
         *                  this parameter until cancelled or closed.
         * @param callback The callback function to call when the timer expires.
         * @return 0 on success.
         *         If the timer can't be activated, -1 is returned
         *         and <code>errno</code> is set.
         */
        int set (unsigned timeout_ms, unsigned repeat_ms, std::function<void()> callback);

        /**
         * Activate the timer with an absolute time in the future.
         * @param timeout The absolute time of the timeout.
         * @param callback The callback function to call when the timer expires.
         * @return 0 on success.
         *         If the timer can't be activated, -1 is returned
         *         and <code>errno</code> is set.
         */
        int set (const struct timespec& timeout, std::function<void()> callback);

        /**
         * Cancel the timer.
         * De-activate the timer if activated.
         * @param cancel_rx Not used.
         * @param cancel_tx Not used.
         */
        virtual void cancel (bool cancel_rx=true, bool cancel_tx=true);


    private:
        uint64_t overrun;
        std::function<void()> cb;
        static void timer_cb (io_result_t& ior, bool repeat);
    };


}
#endif
