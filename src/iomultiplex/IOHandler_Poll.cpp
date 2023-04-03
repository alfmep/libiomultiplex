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
#include <iomultiplex/IOHandler_Poll.hpp>
#include <iomultiplex/io_result_t.hpp>
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
#include <sys/types.h>
#include <sys/socket.h>


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


#ifdef RX_LIST
#  undef RX_LIST
#endif
#  ifdef TX_LIST
#undef TX_LIST
#endif
#define RX_LIST second.first
#define TX_LIST second.second


namespace iomultiplex {


    std::map<int, unsigned> IOHandler_Poll::sigaction_count;
    std::mutex IOHandler_Poll::sigaction_mutex;


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
    // Class IOHandler_Poll::ioop_t
    //--------------------------------------------------------------------------
    class IOHandler_Poll::ioop_t : public io_result_t {
    public:
        ioop_t (timeout_map_t& tm, bool read,
                Connection& c, void* b, size_t s,
                const io_callback_t& cb,
                unsigned timeout_ms, const bool dummy);

        virtual ~ioop_t ();

        io_callback_t   cb;         /**< Callback to be called when the operation is done. */
        bool            dummy_op;   /**< A dummy operation, don't actually try to read or write anything. */
        struct timespec abs_timeout;/**< Timeout value in absolute time. */
        timeout_map_t& timeout_map;
        timeout_map_t::iterator timeout_map_pos; // Position in timeout_map

        // Make it easy to erase an ioop_t object from the IOHandler_Poll's containers
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
    IOHandler_Poll::IOHandler_Poll (int signal_num)
        : cmd_signal {signal_num},
          quit {true},
          my_pid {0},
          state {state_t::stopped},
          fd_map_entry_removed {std::make_pair(-1, false)}
    {
        state = state_t::stopped;

        std::lock_guard<std::mutex> sig_lock (sigaction_mutex);
        auto tmp_entry = sigaction_count.try_emplace(cmd_signal, 0).first;
        if (tmp_entry->second++ == 0) {
            // Block signal 'cmd_signal'
            sigset_t cmd_sigset;
            sigemptyset (&cmd_sigset);
            sigaddset (&cmd_sigset, cmd_signal);
            if (sigprocmask(SIG_BLOCK, &cmd_sigset, &orig_sigmask) < 0) {
                int errnum = errno;
                sigaction_count.erase (tmp_entry);
                TRACE ("IOHandler_Poll: Unable to set signal mask: %s", strerror(errnum));
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
                TRACE ("IOHandler_Poll: Unable to install signal handler: %s", strerror(errnum));
                sigaction_count.erase (tmp_entry);
                sigprocmask (SIG_SETMASK, &orig_sigmask, nullptr);
                throw std::system_error (errnum, std::generic_category(),
                                         "Unable to install signal handler");
            }
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IOHandler_Poll::~IOHandler_Poll ()
    {
        stop ();
        join ();

        std::lock_guard<std::mutex> sig_lock (sigaction_mutex);

        auto tmp_entry = sigaction_count.find (cmd_signal);
        if (tmp_entry!=sigaction_count.end() && --tmp_entry->second == 0) {
            sigaction_count.erase (tmp_entry);
            // Reset the command signal handler
            TRACE_SIG ("Uninstall handler for signal #%d", cmd_signal);
            sigaction (cmd_signal, &orig_sa, nullptr);

            // Restore the original signal mask
            sigprocmask (SIG_SETMASK, &orig_sigmask, nullptr);
        }
    }


    //--------------------------------------------------------------------------
    // Assume ops_mutex is locked !!!
    // Return:
    //   -1 - error starting
    //   0  - starting in same thread
    //   1  - starting in new thread
    //--------------------------------------------------------------------------
    int IOHandler_Poll::start_running (bool start_worker_thread,
                                       std::unique_lock<std::mutex>& ops_lock)
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

            ops_lock.unlock ();
            while (my_pid == 0)
                ; // Busy-wait until the thread has started
            errno = 0;
            return 1; // Starting I/O handler in a new thread
        }

        return 0; // Starting I/O handler in this thread
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int IOHandler_Poll::run (bool start_worker_thread)
    {
        std::unique_lock<std::mutex> ops_lock (ops_mutex);
        int progress = start_running (start_worker_thread, ops_lock);
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
            ops_lock.unlock ();
            int result = ppoll (poll_set.data(),
                                poll_set.size(),
                                have_timeout ? &ts : nullptr,
                                &orig_sigmask);
            ops_lock.lock ();

            if (have_timeout)
                clock_gettime (CLOCK_BOOTTIME, &ts);
            TRACE_POLL ("Poll descriptors after poll : %s", dump_poll_content(poll_set).c_str());

            if (result < 0) {
                if (errno != EINTR) {
                    errnum = errno;
                    quit = true;
                    TRACE_POLL ("IOHandler_Poll: error polling I/O: %s", strerror(errnum));
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
    void IOHandler_Poll::end_running ()
    {
        poll_set.clear ();
        timeout_map.clear ();

        for (auto& entry : ops_map) {
            for (auto& ioop : entry.RX_LIST) {
                // Cancel RX operations
                if (ioop->cb) {
                    ioop->result = -1;
                    ioop->errnum = ECANCELED;
                    ops_mutex.unlock ();
                    ioop->cb (*ioop);
                    ops_mutex.lock ();
                }
            }
            entry.RX_LIST.clear ();

            for (auto& ioop : entry.TX_LIST) {
                // Cancel TX operations
                if (ioop->cb) {
                    ioop->result = -1;
                    ioop->errnum = ECANCELED;
                    ops_mutex.unlock ();
                    ioop->cb (*ioop);
                    ops_mutex.lock ();
                }
            }
            entry.TX_LIST.clear ();
        }
        ops_map.clear ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void IOHandler_Poll::signal_event ()
    {
        if (my_pid!=0 && my_pid != (pid_t)syscall(SYS_gettid)) {
            TRACE_SIG ("Send signal %d to thread id %u", cmd_signal, (unsigned)my_pid);
            pid_t tgid = getpid ();
            int err = syscall (SYS_tgkill, tgid, my_pid, cmd_signal);
            if (err)
                Log::info ("IOHandler_Poll: Unable to raise command signal: %s", strerror(err));
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void IOHandler_Poll::stop ()
    {
        if (!quit.exchange(true)) {
            TRACE ("Quitting I/O handling");
            signal_event ();
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool IOHandler_Poll::same_context () const
    {
        return !my_pid  ||  my_pid == (pid_t)syscall(SYS_gettid);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void IOHandler_Poll::join ()
    {
        if (worker.joinable())
            worker.join ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int IOHandler_Poll::queue_io_op (Connection& conn,
                                void* buf,
                                size_t size,
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

        TRACE ("Queue a %s operation on file desc %d, %u bytes requested",
               (read?"Rx":"Tx"), fd, size);

        std::shared_ptr<ioop_t> ioop (std::make_shared<ioop_t>(
                                              timeout_map, read, conn, buf, size,
                                              cb, timeout, dummy_operation));
        bool send_signal {false};

        auto entry = ops_map.find (fd);
        if (entry == ops_map.end())
            entry = ops_map.emplace (fd, std::make_pair(ioop_list_t(),         // Rx list
                                                        ioop_list_t())).first; // Tx list

        auto& op_list {read ? entry->RX_LIST : entry->TX_LIST};
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
    void IOHandler_Poll::cancel (Connection& conn, bool rx, bool tx, bool fast)
    {
        if (!rx && !tx)
            return;

        auto fd = conn.handle ();
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

        auto& rx_op_list {entry->RX_LIST};
        auto& tx_op_list {entry->TX_LIST};

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
    void IOHandler_Poll::io_dispatch ()
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
    void IOHandler_Poll::handle_event (int fd, bool read, short error_flags)
    {
        TRACE ("Handle %s in file descriptor %d", (read?"input":"output"), fd);

        auto entry = ops_map.find (fd);
        if (entry == ops_map.end())
            return;

        auto* ioop_list = read ? &(entry->RX_LIST) : &(entry->TX_LIST);
        bool done {false};
        TRACE ("File descriptor %d have %d %s operation(s)", fd, ioop_list->size(), (read?"input":"output"));
        while (!quit && !done && !ioop_list->empty()) {
            TRACE ("Handle %s operation on %d", (read?"input":"output"), fd);

            auto ioop = ioop_list->front ();

            if (error_flags) {
                if (error_flags == POLLNVAL) {
                    ioop->errnum = EBADF;
                }else{
                    socklen_t len = sizeof (ioop->errnum);
                    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&ioop->errnum, &len))
                        ioop->errnum = EIO;
                }
                ioop->result = ioop->errnum ? -1 : 0;
                done = true;
            }
            else if (ioop->dummy_op) {
                // Dummy operation, don't read or write anything
                ioop->result = 0;
                ioop->errnum = 0;
            }
            else {
                // Read or write using the connection object
                if (read)
                    ioop->result = ioop->conn.do_read (ioop->buf, ioop->size, ioop->errnum);
                else
                    ioop->result = ioop->conn.do_write (ioop->buf, ioop->size, ioop->errnum);
                if (ioop->result < 0)
                    done = true;
                TRACE ("Result of %s operation on %d: %ld %s",
                       (read?"input":"output"), fd, ioop->result,
                       (ioop->result<0?strerror(ioop->errnum):""));
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
                        ioop_list = read ? &(entry->RX_LIST) : &(entry->TX_LIST);
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
            if (entry->RX_LIST.empty() && entry->TX_LIST.empty())
                ops_map.erase (entry);
    }


    //--------------------------------------------------------------------------
    // ops_mutex is locked!
    //--------------------------------------------------------------------------
    bool IOHandler_Poll::next_timeout (struct timespec& timeout)
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
    void IOHandler_Poll::handle_timeout (struct timespec& now)
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
                                -1,
                                ETIMEDOUT,
                                ioop.timeout);

            // Remove the I/O operation from the queue
            bool is_rx = ioop.is_rx;
            fd_ops_map_t::iterator ops_map_pos = ioop.ops_map_pos;
            ioop_list_t::iterator op_list_pos = ioop.ioop_list_pos;
            ioop_list_t& op_list = is_rx ? ops_map_pos->RX_LIST : ops_map_pos->TX_LIST;
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

            if (ops_map_pos->RX_LIST.empty() && ops_map_pos->TX_LIST.empty())
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
    IOHandler_Poll::ioop_t::ioop_t (timeout_map_t& tm,
                                    bool read,
                                    Connection& c,
                                    void* b,
                                    size_t s,
                                    const io_callback_t& callback,
                                    unsigned timeout_ms,
                                    const bool dummy)
        : io_result_t (c, b, s, 0, 0, timeout_ms),
          cb {callback},
          dummy_op {dummy},
          timeout_map {tm},
          is_rx {read}
    {
        if (timeout_ms == (unsigned)-1) {
            abs_timeout.tv_sec = 0;
            abs_timeout.tv_nsec = 0;
            timeout_map_pos = timeout_map.end ();
            return;
        }

        clock_gettime (CLOCK_BOOTTIME, &abs_timeout);
        while (timeout_ms >= 1000) {
            ++abs_timeout.tv_sec;
            timeout_ms -= 1000;
        }
        abs_timeout.tv_nsec += timeout_ms * 1000000;
        if (abs_timeout.tv_nsec >= 1000000000L) {
            ++abs_timeout.tv_sec;
            abs_timeout.tv_nsec -= 1000000000L;
        }
        timeout_map_pos = timeout_map.emplace (abs_timeout, *this);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IOHandler_Poll::ioop_t::~ioop_t ()
    {
        if (timeout_map_pos != timeout_map.end())
            timeout_map.erase (timeout_map_pos);
    }


}
