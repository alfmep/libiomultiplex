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
#include <iomultiplex/TimerConnection.hpp>
#include <iomultiplex/iohandler_base.hpp>
#include <iomultiplex/Log.hpp>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/timerfd.h>


namespace iomultiplex {


#define LOG_PREFIX "TimerConnection"


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    TimerConnection::TimerConnection (iohandler_base& io_handler, int clock_id)
        : FdConnection (io_handler),
          overrun {0},
          cb {nullptr}
    {
        fd = timerfd_create (clock_id, TFD_NONBLOCK);
        if (fd < 0)
            throw std::system_error (errno, std::generic_category());
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    TimerConnection::TimerConnection (TimerConnection&& rhs)
        : FdConnection (std::move(rhs)),
          overrun {rhs.overrun},
          cb {std::move(rhs.cb)}
    {
        rhs.overrun = 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    TimerConnection& TimerConnection::operator= (TimerConnection&& rhs)
    {
        if (this != &rhs) {
            FdConnection::operator= (std::move(rhs));
            overrun = rhs.overrun;
            cb = std::move (rhs.cb);
            rhs.overrun = 0;
        }
        return *this;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void TimerConnection::timer_cb (io_result_t& ior, bool repeat)
    {
        if (ior.result <= 0) {
            if (ior.result<0 && ior.errnum != ECANCELED)
                Log::warning (LOG_PREFIX " - Timer read error: %s", strerror(ior.errnum));
            return;
        }

        TimerConnection& timer = dynamic_cast<TimerConnection&> (ior.conn);
        if (repeat) {
            timer.read (&timer.overrun, sizeof(timer.overrun), [](io_result_t& ior)->bool
            {
                timer_cb (ior, true);
                return false;
            });
        }
        if (timer.cb)
            timer.cb ();
    }


    //--------------------------------------------------------------------------
    // timeout and repeat in milliseconds
    //--------------------------------------------------------------------------
    int TimerConnection::set (unsigned timeout, unsigned repeat, std::function<void()> callback)
    {
        // Cancel pending read operation
        cancel (true, false);

        if (callback == nullptr)
            return 0;

        struct itimerspec it;
        bool repeating = repeat != 0;

        // Initial timeout
        if (timeout == 0) {
            // Expire as soon as possible
            it.it_value.tv_sec  = 0;
            it.it_value.tv_nsec = 1; // 1 nanosecond, almost immediately
        }else{
            it.it_value.tv_sec = timeout / 1000;
            timeout = timeout % 1000;
            it.it_value.tv_nsec = timeout * 1000000;
        }
        // Repeat
        if (repeat) {
            it.it_interval.tv_sec = repeat / 1000;
            repeat = repeat % 1000;
            it.it_interval.tv_nsec = repeat * 1000000;
        }else{
            it.it_interval.tv_sec = 0;
            it.it_interval.tv_nsec = 0;
        }

        // Activate the timer
        int result = timerfd_settime (fd, 0, &it, nullptr);
        if (result < 0)
            return -1;

        // Callback
        cb = callback;

        // Queue a read operation for the timer expiration
        read (&overrun, sizeof(overrun), [repeating](io_result_t& ior)->bool{
                timer_cb (ior, repeating);
                return false;
            });

        return 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int TimerConnection::set (const struct timespec& timeout,
                              std::function<void()> callback)
    {
        // Cancel pending read operation
        io_handler().cancel (*this, true, false);

        if (callback == nullptr)
            return 0;

        // Callback
        cb = callback;

        // Activate the timer
        itimerspec it;
        it.it_value = timeout;
        it.it_interval = {0, 0};
        if (timerfd_settime (fd, TFD_TIMER_ABSTIME, &it, nullptr))
            return -1;

        // Queue a read operation for the timer expiration
        read (&overrun, sizeof(overrun), [](io_result_t& ior)->bool{
                timer_cb (ior, false);
                return false;
            });

        return 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void TimerConnection::cancel (bool cancel_rx, bool cancel_tx)
    {
        io_handler().cancel (*this);
        struct itimerspec it {0};
        timerfd_settime (fd, 0, &it, nullptr); // Disarm the timer
    }



}
