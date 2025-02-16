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
#include <iomultiplex/timer_connection.hpp>
#include <iomultiplex/iohandler_base.hpp>
#include <iomultiplex/log.hpp>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/timerfd.h>


namespace iomultiplex {


//#define TRACE_DEBUG

#ifdef TRACE_DEBUG
#  define THIS_FILE "timer_connection.cpp"
#  define TRACE(format, ...) log::debug("[%u] %s:%s:%d: " format, gettid(), THIS_FILE, __FUNCTION__, __LINE__, ## __VA_ARGS__);
#else
#  define TRACE(format, ...)
#endif

#define LOG_PREFIX "timer_connection"


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    timer_connection::timer_connection (iohandler_base& io_handler, int clock_id)
        : fd_connection (io_handler),
          overrun {0},
          cb {nullptr}
    {
        fd = timerfd_create (clock_id, TFD_NONBLOCK);
        if (fd < 0)
            throw std::system_error (errno, std::generic_category());
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    timer_connection::timer_connection (timer_connection&& rhs)
        : fd_connection (std::move(rhs)),
          overrun {rhs.overrun},
          cb {std::move(rhs.cb)}
    {
        rhs.overrun = 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    timer_connection& timer_connection::operator= (timer_connection&& rhs)
    {
        if (this != &rhs) {
            fd_connection::operator= (std::move(rhs));
            overrun = rhs.overrun;
            cb = std::move (rhs.cb);
            rhs.overrun = 0;
        }
        return *this;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void timer_connection::timer_cb (io_result_t& ior, bool repeat)
    {
        if (ior.result <= 0) {
            if (ior.result<0 && ior.errnum != ECANCELED)
                log::warning (LOG_PREFIX " - Timer read error: %s", strerror(ior.errnum));
            return;
        }

        timer_connection& timer = dynamic_cast<timer_connection&> (ior.conn);

        TRACE ("Overrun: %u", (unsigned)timer.overrun);;

        timer.mutex.lock ();
        if (repeat) {
            timer.read (&timer.overrun, sizeof(timer.overrun), [](io_result_t& ior)->bool
                {
                    timer_cb (ior, true);
                    return false;
                });
        }
        auto cb = timer.cb;
        timer.mutex.unlock ();
        if (cb)
            cb ();
    }


    //--------------------------------------------------------------------------
    // timeout and repeat in milliseconds
    //--------------------------------------------------------------------------
    int timer_connection::set (unsigned timeout, unsigned repeat, std::function<void()> callback)
    {
        std::lock_guard<std::mutex> lock (mutex);

        // Sanity check
        errno = 0;
        if (fd == -1) {
            errno = EBADF;
            return -1;
        }

        // Cancel timer if already active
        cancel_impl ();

        if (callback == nullptr)
            return 0; // Don't set the timer without a callback

        struct itimerspec it;
        bool repeating = repeat != 0;

        // Initial timeout
        if (timeout == 0) {
            // Expire as soon as possible
            it.it_value.tv_sec  = 0;
            it.it_value.tv_nsec = 1; // 1 nanosecond, almost immediately.
                                     // A value of zero would disarm the timer.
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
        if (timerfd_settime(fd, 0, &it, nullptr))
            return -1;

        // Callback
        cb = callback;

        // Queue a read operation for the timer expiration
        auto result = read (&overrun, sizeof(overrun), [repeating](io_result_t& ior)->bool{
                timer_cb (ior, repeating);
                return false;
            });
        if (result) {
            auto tmp_errno = errno;
            cancel_impl ();
            errno = tmp_errno;
        }

        return result;
    }


    //--------------------------------------------------------------------------
    // timeout is an absolute time
    // repeat is a relative time
    //--------------------------------------------------------------------------
    int timer_connection::set_abs (const struct timespec& timeout,
                                   const struct timespec& repeat,
                                   std::function<void()> callback)
    {
        std::lock_guard<std::mutex> lock (mutex);

        // Sanity check
        errno = 0;
        if (fd == -1) {
            errno = EBADF;
            return -1;
        }

        // Cancel timer if already active
        cancel_impl ();

        if (callback == nullptr)
            return 0; // Don't set the timer without a callback

        // Callback
        cb = callback;

        // Activate the timer
        itimerspec it;
        it.it_value = timeout;
        it.it_interval = repeat;
        bool repeating = repeat.tv_sec || repeat.tv_nsec;
        if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &it, nullptr))
            return -1;

        // Queue a read operation for the timer expiration
        auto result = read (&overrun, sizeof(overrun), [repeating](io_result_t& ior)->bool{
            // I/O callback context
            timer_cb (ior, repeating);
            return false;
        });
        if (result < 0) {
            auto tmp_errno = errno;
            cancel_impl ();
            errno = tmp_errno;
        }

        return result;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void timer_connection::cancel (bool cancel_rx, bool cancel_tx, bool fast)
    {
        std::lock_guard<std::mutex> lock (mutex);
        cancel_impl ();
    }


    //--------------------------------------------------------------------------
    // mutex is locked !!!
    //--------------------------------------------------------------------------
    void timer_connection::cancel_impl ()
    {
        io_handler().cancel (*this, true, false, true);
        if (fd != -1) {
            struct itimerspec it {0};
            timerfd_settime (fd, 0, &it, nullptr); // Disarm the timer
        }
    }



}
