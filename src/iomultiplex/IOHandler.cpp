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
#include <iomultiplex/IOHandler.hpp>
#include <iomultiplex/Log.hpp>

#include <vector>
#include <exception>
#include <system_error>
#include <cstring>
#include <cerrno>
#include <signal.h>
#include <poll.h>
#include <unistd.h>
#include <syscall.h>
#include <sys/timerfd.h>


//#define TRACE_DEBUG

#ifdef TRACE_DEBUG
#  define TRACE_DEBUG_MISC
#  define TRACE_DEBUG_SIG
#  define TRACE_DEBUG_POLL
#  include <iostream>
#  include <sstream>
#  include <iomanip>
#endif
#ifdef TRACE_DEBUG_MISC
#  define TRACE(format, ...) Log::debug("%s:%s:%d: " format, __FILE__, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#  define TRACE(format, ...)
#endif
#ifdef TRACE_DEBUG_SIG
#  define TRACE_SIG(format, ...) Log::debug("%s:%s:%d: " format, __FILE__, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#  define TRACE_SIG(format, ...)
#endif
#ifdef TRACE_DEBUG_POLL
#  define TRACE_POLL(format, ...) Log::debug("%s:%s:%d: " format, __FILE__, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#  define TRACE_POLL(format, ...)
#endif



namespace iomultiplex {



#ifdef TRACE_DEBUG
    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static std::string events_to_string (short events, bool trunc)
    {
        std::stringstream ss;

        bool first_event = true;

        if (events & POLLIN) {
            first_event = false;
            ss << "IN";
        }
        if (events & POLLPRI) {
            if (!first_event)
                ss << '|';
            first_event = false;
            ss << "PRI";
        }
        if (events & POLLOUT) {
            if (!first_event)
                ss << '|';
            first_event = false;
            ss << "OUT";
        }
        if (events & POLLRDHUP) {
            if (!first_event)
                ss << '|';
            first_event = false;
            ss << "RDHUP";
        }
        if (!trunc) {
            if (events & POLLERR) {
                if (!first_event)
                    ss << '|';
                first_event = false;
                ss << "ERR";
            }
            if (events & POLLHUP) {
                if (!first_event)
                    ss << '|';
                first_event = false;
                ss << "HUP";
            }
            if (events & POLLNVAL) {
                if (!first_event)
                    ss << '|';
                first_event = false;
                ss << "NVAL";
            }
        }
        return ss.str ();
    }


    static std::string dump_poll_content (PollDescriptors& l)
    {
        std::stringstream ss;
        ss << '(' << l.size() << " of " << l.capacity() << " active){";
        ss  << std::setfill('0');

        bool first {true};
        struct pollfd* pfd = l.data();

        for (size_t i=0; i<l.capacity(); ++i) {
            //for (auto& pfd : fd_list) {
            if (!first)
                ss << ',';
            first = false;
            // fd
            ss << '{' << std::dec << std::setw(0) << pfd[i].fd << ',';
            // events
            ss << events_to_string(pfd[i].events, true) << ',';
            // revents
            ss << events_to_string(pfd[i].revents, false) << '}';
        }
        ss << '}';

        return ss.str ();
    }
#endif



    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static inline struct timespec operator- (const struct timespec& lhs, const struct timespec& rhs)
    {
        struct timespec res;
        res.tv_sec = lhs.tv_sec - rhs.tv_sec;

        if (rhs.tv_nsec > lhs.tv_nsec) {
            --res.tv_sec;
            res.tv_nsec = (lhs.tv_nsec + 1000000000L) - rhs.tv_nsec;
        }else{
            res.tv_nsec = lhs.tv_nsec - rhs.tv_nsec;
        }

        return res;
    }



    //--------------------------------------------------------------------------
    // Class IOHandler::ioop_t
    //--------------------------------------------------------------------------
    class IOHandler::ioop_t : public io_result_t {
    public:
        ioop_t (timeout_map_t& tm, bool read,
                Connection& c, void* b, size_t s, off_t o,
                const io_callback_t& cb,
                unsigned timeout_ms, const bool dummy);

        virtual ~ioop_t ();

        io_callback_t   cb;         /**< Callback to be called when the operation is done. */
        bool            dummy_op;   /**< A dummy operation, don't actually try to read or write anything. */
        struct timespec timeout;    /**< Timeout value. */
        timeout_map_t& timeout_map;
        timeout_map_t::iterator timeout_map_pos; // Position in timeout_map

        // Make it easy to erase an ioop_t object from the IOHandler's containers
        bool is_rx;
        ioop_list_t::iterator ioop_list_pos; // Position in ioop list (tx or rx)
        fd_ops_map_t::iterator ops_map_pos;  // Position in file_desc->ioop_lists map
    };





    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static void cmd_signal_handler (int sig, siginfo_t* si, void* ucontext)
    {
        TRACE_SIG ("Got signal #%d", sig);
        return;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IOHandler::IOHandler (int signal_num)
        : cmd_signal {signal_num},
          quit {true},
          my_pid {0},
          state {state_t::stopped},
          fd_map_entry_removed {std::make_pair(-1, false)}
    {
        state = state_t::stopped;

        // Block signal 'cmd_signal'
        sigset_t cmd_sigset;
        sigemptyset (&cmd_sigset);
        sigaddset (&cmd_sigset, cmd_signal);
        if (sigprocmask(SIG_BLOCK, &cmd_sigset, &orig_sigmask) < 0) {
            int errnum = errno;
            Log::debug ("IOHandler: Unable to set signal mask: %s", strerror(errno));
            throw std::system_error (errnum, std::generic_category(),
                                     "Unable to set signal mask");
        }

        // Install signal handler used for modifying the poll descriptors
        //
        TRACE_SIG ("Install handler for signal #%d", cmd_signal);
        struct sigaction sa;
        memset (&sa, 0, sizeof(sa));
        sigemptyset (&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = cmd_signal_handler;

        if (sigaction(cmd_signal, &sa, &orig_sa) < 0) {
            int errnum = errno;
            Log::debug ("IOHandler: Unable to install signal handler: %s", strerror(errno));
            sigprocmask (SIG_SETMASK, &orig_sigmask, nullptr);
            throw std::system_error (errnum, std::generic_category(),
                                     "Unable to install signal handler");
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IOHandler::~IOHandler ()
    {
        stop ();
        join ();

        // Reset the command signal handler
        TRACE_SIG ("Uninstall handler for signal #%d", cmd_signal);
        sigaction (cmd_signal, &orig_sa, nullptr);

        // Reset the signal mask
        sigprocmask (SIG_SETMASK, &orig_sigmask, nullptr);
    }


    //--------------------------------------------------------------------------
    // Assume ops_mutex is locked !!!
    // Return:
    //   -1 - error starting
    //   0  - starting in same thread
    //   1  - starting in new thread
    //--------------------------------------------------------------------------
    int IOHandler::start_running (bool start_worker_thread)
    {
        if (state != state_t::stopped) {
            // Can't start an I/O handler that isn't stopped
            errno = EINPROGRESS;
            return -1; // Error starting I/O handler
        }
        state = state_t::starting;

        if (start_worker_thread) {
            if (worker.joinable()) {
                // Worker thread already active
                errno = EINPROGRESS;
                return -1; // Error starting I/O handler
            }
            state = state_t::stopped;
            worker = std::thread ([this](){
                    // Run this method in another thread
                    run (false);
                });

            ops_mutex.unlock ();
            while (my_pid == 0)
                ; // Busy-wait until the thread has started
            errno = 0;
            return 1; // Starting I/O handler in a new thread
        }

        return 0; // Starting I/O handler in this thread
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int IOHandler::run (bool start_worker_thread)
    {
        std::lock_guard<std::mutex> lock (ops_mutex);
        int progress = start_running (start_worker_thread);
        if (progress)
            return progress<0 ? -1 : 0;

        my_pid = (pid_t) syscall (SYS_gettid);
        quit = false;
        state = state_t::running;

        TRACE ("Start processing I/O");

        // Poll until instructed to stop (or poll itself fails)
        //
        int errnum = 0;
        while (!quit) {
            struct timespec ts;
            bool have_timeout {false};

            poll_set.commit ();
            TRACE_POLL ("Poll descriptors before poll: %s", dump_poll_content(poll_set).c_str());
            have_timeout = next_timeout (ts);
#ifdef TRACE_DEBUG_POLL
            if (have_timeout)
                TRACE_POLL ("Poll timeout set to: %u.%09u", ts.tv_sec, ts.tv_nsec);
#endif
            ops_mutex.unlock ();
            int result = ppoll (poll_set.data(),
                                poll_set.size(),
                                have_timeout ? &ts : nullptr,
                                &orig_sigmask);
            ops_mutex.lock ();

            if (have_timeout)
                clock_gettime (CLOCK_BOOTTIME, &ts);
            TRACE_POLL ("Poll descriptors after poll : %s", dump_poll_content(poll_set).c_str());

            if (result < 0) {
                if (errno != EINTR) {
                    errnum = errno;
                    quit = true;
                    Log::debug ("IOHandler: error polling I/O: %s", strerror(errnum));
                }
            }else{
                if (result > 0)
                    io_dispatch ();
                if (have_timeout && !timeout_map.empty())
                    handle_timeout (ts);
            }
        }
        state = state_t::stopping;

        // clean up
        end_running ();
        TRACE ("Finished processing I/O ");
        my_pid = 0;
        errno = errnum;
        state = state_t::stopped;
        return errno==0 ? 0 : -1;
    }


    //--------------------------------------------------------------------------
    // Assume ops_mutex is locked !!!
    //--------------------------------------------------------------------------
    void IOHandler::end_running ()
    {
        poll_set.clear ();
        timeout_map.clear ();

        for (auto& entry : ops_map) {
            for (auto& ioop : entry.second.first) {
                // Cancel RX operations
                if (ioop->cb) {
                    ioop->result = -1;
                    ioop->errnum = ECANCELED;
                    ops_mutex.unlock ();
                    ioop->cb (*ioop);
                    ops_mutex.lock ();
                }
            }
            entry.second.first.clear ();

            for (auto& ioop : entry.second.second) {
                // Cancel TX operations
                if (ioop->cb) {
                    ioop->result = -1;
                    ioop->errnum = ECANCELED;
                    ops_mutex.unlock ();
                    ioop->cb (*ioop);
                    ops_mutex.lock ();
                }
            }
            entry.second.second.clear ();
        }
        ops_map.clear ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void IOHandler::signal_event ()
    {
        if (my_pid!=0 && my_pid != (pid_t)syscall(SYS_gettid)) {
            TRACE_SIG ("Send signal %d to thread id %u", cmd_signal, (unsigned)my_pid);
            pid_t tgid = getpid ();
            int err = syscall (SYS_tgkill, tgid, my_pid, cmd_signal);
            if (err)
                Log::debug ("IOHandler: Unable to raise command signal: %s", strerror(err));
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void IOHandler::stop ()
    {
        if (!quit.exchange(true)) {
            TRACE ("Quitting I/O handling");
            signal_event ();
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool IOHandler::same_context () const
    {
        return !my_pid  ||  my_pid == (pid_t)syscall(SYS_gettid);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void IOHandler::join ()
    {
        if (worker.joinable())
            worker.join ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int IOHandler::queue_io_op (Connection& conn,
                                void* buf,
                                size_t size,
                                off_t offset,
                                io_callback_t cb,
                                const bool read,
                                const bool dummy_operation,
                                unsigned timeout)
    {
        std::lock_guard<std::mutex> lock (ops_mutex);

        int fd = conn.handle ();
        if (fd < 0) {
            errno = EBADF;
            return -1;
        }
        if (state == state_t::stopping) {
            errno = ECANCELED;
            return -1;
        }

        TRACE ("Queue a %s operation on file desc %d, %u bytest requested",
               (read?"Rx":"Tx"), fd, size);

        std::shared_ptr<ioop_t> ioop (std::make_shared<ioop_t>(
                                              timeout_map, read, conn, buf, size,
                                              offset, cb, timeout, dummy_operation));
        bool send_signal {false};

        auto entry = ops_map.find (fd);
        if (entry == ops_map.end())
            entry = ops_map.emplace (fd, std::make_pair(ioop_list_t(),         // Rx list
                                                        ioop_list_t())).first; // Tx list

        auto& op_list {read ? entry->second.first : entry->second.second};
        if (op_list.empty()) {
            poll_set.schedule_activate (fd, (read ? POLLIN : POLLOUT), true);
            send_signal = true;
        }
        op_list.emplace_back (ioop);

        // Save the positions to make it easier to remove an ioop from out lists
        ioop->ops_map_pos = entry;
        ioop->ioop_list_pos = op_list.end ();
        --ioop->ioop_list_pos;

        if (send_signal)
            signal_event ();

        errno = 0;
        return 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void IOHandler::cancel_impl (int fd, bool rx, bool tx)
    {
        if (!rx && !tx)
            return;

        if (fd < 0)
            return; // Invalid file handle

        std::lock_guard<std::mutex> lock (ops_mutex);
        if (state == state_t::stopping)
            return; // I/O handler stopping and cleaning up

        TRACE ("Cancel %s%s%s for file descriptor %d",
               (tx?"Tx":""),
               ((tx && rx)?" and ":""),
               (rx?"Rx":""),
               fd);

        auto entry = ops_map.find (fd);
        if (entry == ops_map.end())
            return; // No I/O operations found

        auto& rx_op_list {entry->second.first};
        auto& tx_op_list {entry->second.second};

        bool send_signal {false};

        if (rx && !rx_op_list.empty()) {
            // Clear read operations
            TRACE ("Erasing Rx operations for file descriptor %d", fd);
            rx_op_list.clear ();
            poll_set.schedule_deactivate (fd, POLLIN);
            send_signal = true;
        }
        if (tx && !tx_op_list.empty()) {
            // Clear write operations
            TRACE ("Erasing Tx operations for file descriptor %d", fd);
            tx_op_list.clear ();
            poll_set.schedule_deactivate (fd, POLLOUT);
            send_signal = true;
        }

        if (rx_op_list.empty() && tx_op_list.empty()) {
            ops_map.erase (entry);
            if (fd_map_entry_removed.first == fd)
                fd_map_entry_removed.second = true; // fd removed from ops_map
        }

        if (send_signal)
            signal_event ();
    }


    //--------------------------------------------------------------------------
    // ops_mutex is locked!
    //--------------------------------------------------------------------------
    void IOHandler::io_dispatch ()
    {
        TRACE ("Handle I/O");
        auto* desc = poll_set.data ();
        size_t size = poll_set.size ();

        for (size_t i=0; i<size; ++i) {
            if (desc[i].fd == -1)
                continue;

            TRACE ("(%d) Events on file desc %d: %04x",
                   i, desc[i].fd, (unsigned)desc[i].revents);

            short err = desc[i].revents & (POLLERR|POLLHUP|POLLNVAL);
            if (err) {
                TRACE ("Error event on file desc %d: %s %s %s",
                       desc[i].fd,
                       ((desc[i].revents & POLLERR)?"POLLERR":""),
                       ((desc[i].revents & POLLHUP)?"POLLHUP":""),
                       ((desc[i].revents & POLLNVAL)?"POLLNVAL":""));
                if (desc[i].events & POLLOUT) {
                    // Write
                    TRACE ("File desc %d have error on write", desc[i].fd);
                    handle_event (desc[i].fd, false, err);
                }
                if (desc[i].events & POLLIN) {
                    // Read
                    TRACE ("File desc %d have error on read", desc[i].fd);
                    handle_event (desc[i].fd, true, err);
                }
            }else{
                if (desc[i].revents & POLLOUT) {
                    // Write
                    TRACE ("File desc %d is ready for write", desc[i].fd);
                    handle_event (desc[i].fd, false, err);
                }
                if (desc[i].revents & POLLIN) {
                    // Read
                    TRACE ("File desc %d is ready for read", desc[i].fd);
                    handle_event (desc[i].fd, true, err);
                }
            }
            desc[i].revents = 0;
        }
    }


    //--------------------------------------------------------------------------
    // ops_mutex is locked!
    //--------------------------------------------------------------------------
    void IOHandler::handle_event (int fd, bool read, short error_flags)
    {
        TRACE ("Handle %s in file descriptor %d", (read?"input":"output"), fd);

        auto entry = ops_map.find (fd);
        if (entry == ops_map.end())
            return;

        auto* ioop_list = read ? &(entry->second.first) : &(entry->second.second);
        bool done {false};
        TRACE ("File descriptor %d have %d %s operation(s)", fd, ioop_list->size(), (read?"input":"output"));
        while (!quit && !done && !ioop_list->empty()) {

            auto ioop = ioop_list->front ();

            if (ioop->dummy_op) {
                // Dummy operation, don't read or write anything
                ioop->result = 0;
                ioop->errnum = 0;
                done = true;
            }else{
                // Read or write using the connection object
                if (read)
                    ioop->result = ioop->conn.do_read (ioop->buf, ioop->size, ioop->offset, ioop->errnum);
                else
                    ioop->result = ioop->conn.do_write (ioop->buf, ioop->size, ioop->offset, ioop->errnum);
                if (ioop->result < 0)
                    done = true;
            }

            if (ioop->errnum == EAGAIN) {
                ioop->result = 0;
                ioop->errnum = 0;
                done = true;
                break;
            }

            ioop_list->pop_front ();

            if (ioop->cb == nullptr) {
                done = true;
            }else{
                fd_map_entry_removed.first = fd;
                fd_map_entry_removed.second = false;
                ops_mutex.unlock ();
                if (!ioop->cb(*ioop))
                    done = true;
                ops_mutex.lock ();

                // The callback may have invalidated the local variables 'entry' and 'ioop_list'
                // by calling method 'cancel' and then perhaps 'read'/'write'.
                if (fd_map_entry_removed.second == true) {
                    // fd map entry is invalid
                    entry = ops_map.find (fd);
                    if (entry != ops_map.end()) {
                        ioop_list = read ? &(entry->second.first) : &(entry->second.second);
                    }else{
                        ioop_list = nullptr;
                        done = true;
                    }
                }
                fd_map_entry_removed.first = -1; // Invalidate the fd map check
            }
        }

        // Update poll set if needed
        if (ioop_list==nullptr || ioop_list->empty())
            poll_set.schedule_deactivate (fd, (read ? POLLIN : POLLOUT));

        if (entry != ops_map.end())
            if (entry->second.first.empty() && entry->second.second.empty())
                ops_map.erase (entry);
    }


    //--------------------------------------------------------------------------
    // ops_mutex is locked!
    //--------------------------------------------------------------------------
    bool IOHandler::next_timeout (struct timespec& timeout)
    {
        if (timeout_map.empty())
            return false;

        auto& later = timeout_map.begin()->first;

        timespec_less_t less;
        struct timespec now;
        clock_gettime (CLOCK_BOOTTIME, &now);
        if (less(now, later)) {
            timeout = later - now;
        }else{
            timeout.tv_sec = 0;
            timeout.tv_nsec = 0;
        }

        return true;
    }


    //--------------------------------------------------------------------------
    // ops_mutex is locked!
    //--------------------------------------------------------------------------
    void IOHandler::handle_timeout (struct timespec& now)
    {
        timespec_less_t less;

        while (!timeout_map.empty()) {
            auto entry = timeout_map.begin ();
            auto& deadline = entry->first;

            if (less(now, deadline)) {
                // Timeout not reached
                break;
            }

            // We have a timeout !

            ioop_t& ioop = entry->second;
            auto callback = ioop.cb;
            io_result_t result (ioop.conn,
                                ioop.buf,
                                ioop.size,
                                ioop.offset,
                                -1,
                                ETIMEDOUT);

            // Remove the I/O operation from the queue
            bool is_rx = ioop.is_rx;
            fd_ops_map_t::iterator ops_map_pos = ioop.ops_map_pos;
            ioop_list_t::iterator op_list_pos = ioop.ioop_list_pos;
            ioop_list_t& op_list = is_rx ? ops_map_pos->second.first : ops_map_pos->second.first;
            int fd = ops_map_pos->first;

#ifdef TRACE_DEBUG_MISC
            auto diff = now - deadline;
            TRACE ("%s timeout on file descriptor %d by %u.%09u seconds",
                   (is_rx?"Rx":"Tx"), fd,
                   diff.tv_sec, diff.tv_nsec);
#endif
            op_list.erase (op_list_pos);
            if (op_list.empty())
                poll_set.schedule_deactivate (fd, (is_rx ? POLLIN : POLLOUT));

            if (ops_map_pos->second.first.empty() && ops_map_pos->second.second.empty())
                ops_map.erase (ops_map_pos);

            if (callback) {
                // Call the callback
                ops_mutex.unlock ();
                callback (result);
                ops_mutex.lock ();
            }
        }
    }






    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IOHandler::ioop_t::ioop_t (timeout_map_t& tm,
                               bool read,
                               Connection& c,
                               void* b,
                               size_t s,
                               off_t o,
                               const io_callback_t& callback,
                               unsigned timeout_ms,
                               const bool dummy)
        : io_result_t (c, b, s, o, 0, 0),
          cb {callback},
          dummy_op {dummy},
          timeout_map {tm},
          is_rx {read}
    {
        if (timeout_ms == (unsigned)-1) {
            timeout.tv_sec = 0;
            timeout.tv_nsec = 0;
            timeout_map_pos = timeout_map.end ();
            return;
        }

        clock_gettime (CLOCK_BOOTTIME, &timeout);
        while (timeout_ms >= 1000) {
            ++timeout.tv_sec;
            timeout_ms -= 1000;
        }
        timeout.tv_nsec += timeout_ms * 1000000;
        if (timeout.tv_nsec >= 1000000000L) {
            ++timeout.tv_sec;
            timeout.tv_nsec -= 1000000000L;
        }
        timeout_map_pos = timeout_map.emplace (timeout, *this);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IOHandler::ioop_t::~ioop_t ()
    {
        if (timeout_map_pos != timeout_map.end())
            timeout_map.erase (timeout_map_pos);
    }


}
