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
#include <iomultiplex/IOHandler_Epoll.hpp>
#include <iomultiplex/io_result_t.hpp>
#include <iomultiplex/Log.hpp>

#include <sstream>
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
#include <sys/epoll.h>


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
#  define TRACE(format, ...) Log::debug("[%u] %s:%s:%d: " format, gettid(), __FILE__, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#  define TRACE(format, ...)
#endif
#ifdef TRACE_DEBUG_SIG
#  define TRACE_SIG(format, ...) Log::debug("[%u] %s:%s:%d: " format, gettid(), __FILE__, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#  define TRACE_SIG(format, ...)
#endif
#ifdef TRACE_DEBUG_POLL
#  define TRACE_POLL(format, ...) Log::debug("[%u] %s:%s:%d: " format, gettid(), __FILE__, __FUNCTION__, __LINE__, ## __VA_ARGS__)
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


#ifdef TRACE_DEBUG
    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static std::string events_to_string (uint32_t events)
    {
        std::stringstream ss;

        bool first_event = true;

        if (events & EPOLLIN) {
            first_event = false;
            ss << "IN";
        }
        if (events & EPOLLOUT) {
            if (!first_event)
                ss << '|';
            first_event = false;
            ss << "OUT";
        }
        if (events & EPOLLPRI) {
            if (!first_event)
                ss << '|';
            first_event = false;
            ss << "PRI";
        }
        if (events & EPOLLERR) {
            if (!first_event)
                ss << '|';
            first_event = false;
            ss << "ERR";
        }
        if (events & EPOLLRDHUP) {
            if (!first_event)
                ss << '|';
            first_event = false;
            ss << "RDHUP";
        }
        if (events & EPOLLHUP) {
            if (!first_event)
                ss << '|';
            first_event = false;
            ss << "HUP";
        }
        return ss.str ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static std::string epoll_op_to_string (int op)
    {
        switch (op) {
        case EPOLL_CTL_ADD:
            return "add";
        case EPOLL_CTL_MOD:
            return "mod";
        case EPOLL_CTL_DEL:
            return "del";
        default:
            return "nop";
        }
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
    // Class IOHandler_Epoll::ioop_t
    //--------------------------------------------------------------------------
    class IOHandler_Epoll::ioop_t : public io_result_t {
    public:
        ioop_t (timeout_map_t& tm, bool read,
                Connection& c, void* b, size_t s, off_t o,
                const io_callback_t& cb,
                unsigned timeout_ms, const bool dummy);

        virtual ~ioop_t ();

        io_callback_t   cb;         /**< Callback to be called when the operation is done. */
        bool            dummy_op;   /**< A dummy operation, don't actually try to read or write anything. */
        struct timespec timeout;    /**< Timeout value. */
        timeout_map_t& timeout_map; /**< A reference to the map holding this instance. */
        timeout_map_t::iterator timeout_map_pos; // Position of this instance in timeout_map

        // Make it easy to erase an ioop_t object from the IOHandler_Epolls containers
        bool is_rx;
        ioop_list_t::iterator ioop_list_pos; // Position in ioop list (tx or rx)
        fd_ops_map_t::iterator ops_map_pos;  // Position in file_desc->ioop_lists map
    };





    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static void ctl_signal_handler (int sig, siginfo_t* si, void* ucontext)
    {
        TRACE_SIG ("Got signal #%d", sig);
        return;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IOHandler_Epoll::IOHandler_Epoll (int signal_num)
        : ctl_fd {-1},
          ctl_signal {signal_num},
          quit {true},
          my_pid {0},
          state {state_t::stopped},
          fd_map_entry_removed {std::make_pair(-1, false)}
    {
        // Block signal 'ctl_signal'
        sigset_t ctl_sigset;
        sigemptyset (&ctl_sigset);
        sigaddset (&ctl_sigset, ctl_signal);
        if (sigprocmask(SIG_BLOCK, &ctl_sigset, &orig_sigmask) < 0) {
            int errnum = errno;
            TRACE ("IOHandler_Epoll: Unable to set signal mask: %s", strerror(errno));
            throw std::system_error (errnum, std::system_category(),
                                     "Unable to set signal mask");
        }

        // Install signal handler used for modifying the poll descriptors
        //
        TRACE_SIG ("Install handler for signal #%d", ctl_signal);
        struct sigaction sa;
        memset (&sa, 0, sizeof(sa));
        sigemptyset (&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = ctl_signal_handler;

        if (sigaction(ctl_signal, &sa, &orig_sa) < 0) {
            int errnum = errno;
            TRACE ("IOHandler_Epoll: Unable to install signal handler: %s", strerror(errno));
            sigprocmask (SIG_SETMASK, &orig_sigmask, nullptr);
            throw std::system_error (errnum, std::system_category(),
                                     "Unable to install signal handler");
        }

        // Create epoll control descriptor
        //
        ctl_fd = epoll_create1 (EPOLL_CLOEXEC);
        if (ctl_fd < 0) {
            throw std::system_error (errno,
                                     std::system_category(),
                                     "epoll_create failed");
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IOHandler_Epoll::~IOHandler_Epoll ()
    {
        stop ();
        join ();
        if (ctl_fd >= 0)
            close (ctl_fd);

        // Reset the command signal handler
        TRACE_SIG ("Uninstall handler for signal #%d", ctl_signal);
        sigaction (ctl_signal, &orig_sa, nullptr);

        // Restore the original signal mask
        sigprocmask (SIG_SETMASK, &orig_sigmask, nullptr);
    }


    //--------------------------------------------------------------------------
    // Assume ops_mutex is locked !!!
    // Return:
    //   -1 - error starting
    //   0  - starting in same thread
    //   1  - starting in new thread
    //--------------------------------------------------------------------------
    int IOHandler_Epoll::start_running (bool start_worker_thread)
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
    int IOHandler_Epoll::run (bool start_worker_thread)
    {
        std::lock_guard<std::mutex> lock (ops_mutex);
        int progress = start_running (start_worker_thread);
        if (progress)
            return progress<0 ? -1 : 0;

        quit = false;
        state = state_t::running;
        my_pid = (pid_t) syscall (SYS_gettid);

        TRACE ("Start processing I/O");

        // Poll until instructed to stop (or poll itself fails)
        //
        int errnum = 0;
        struct epoll_event events[32];
        int max_events = sizeof(events)/sizeof(events[0]);

        while (!quit) {
            struct timespec ts;
            int timeout = next_timeout ();
#ifdef TRACE_DEBUG_POLL
            if (timeout > -1)
                TRACE_POLL ("Poll timeout in %d ms", timeout);
#endif
            ops_mutex.unlock ();
            auto num_events = epoll_pwait (ctl_fd,
                                           events,
                                           max_events,
                                           timeout,
                                           &orig_sigmask);
            ops_mutex.lock ();

            if (timeout > -1)
                clock_gettime (CLOCK_MONOTONIC, &ts);

            if (num_events < 0) {
                if (errno != EINTR) {
                    errnum = errno;
                    quit = true;
                    TRACE ("IOHandler_Epoll: error polling I/O: %s", strerror(errnum));
                }
            }else{
                if (num_events > 0)
                    io_dispatch (events, num_events);
                if (timeout>-1 && !timeout_map.empty())
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
    void IOHandler_Epoll::end_running ()
    {
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
    void IOHandler_Epoll::signal_event ()
    {
        if (my_pid!=0 && my_pid != (pid_t)syscall(SYS_gettid)) {
            TRACE_SIG ("Send signal %d to thread id %u", ctl_signal, (unsigned)my_pid);
            pid_t tgid = getpid ();
            int err = syscall (SYS_tgkill, tgid, my_pid, ctl_signal);
            if (err)
                Log::info ("IOHandler_Epoll: Unable to raise command signal: %s", strerror(err));
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void IOHandler_Epoll::stop ()
    {
        if (!quit.exchange(true)) {
            TRACE ("Quitting I/O handling");
            signal_event ();
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool IOHandler_Epoll::same_context () const
    {
        return !my_pid  ||  my_pid == (pid_t)syscall(SYS_gettid);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void IOHandler_Epoll::join ()
    {
        if (worker.joinable())
            worker.join ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int IOHandler_Epoll::queue_io_op (Connection& conn,
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

        TRACE ("Queue a %s%s operation on file desc %d, %u bytes requested",
               (dummy_operation?"dummy ":""), (read?"Rx":"Tx"), fd, size);

        std::shared_ptr<ioop_t> ioop (std::make_shared<ioop_t>(
                                              timeout_map, read, conn, buf, size,
                                              offset, cb, timeout, dummy_operation));
        bool send_signal {false};

        auto entry = ops_map.find (fd);
        if (entry == ops_map.end())
            entry = ops_map.emplace (fd, std::make_pair(ioop_list_t(),         // Rx list
                                                        ioop_list_t())).first; // Tx list

        auto& op_list    {read ? entry->RX_LIST : entry->TX_LIST};
        auto& other_list {read ? entry->TX_LIST : entry->RX_LIST};
        if (op_list.empty()) {
            int op;
            struct epoll_event event;
            event.data.fd = fd;
            if (other_list.empty()) {
                op = EPOLL_CTL_ADD;
                event.events = read ? EPOLLIN : EPOLLOUT;
            }else{
                op = EPOLL_CTL_MOD;
                event.events = EPOLLIN | EPOLLOUT;
            }
            TRACE_POLL ("epoll_ctl (%s, %d, %s)",
                        epoll_op_to_string(op).c_str(), fd, events_to_string(event.events).c_str());
            epoll_ctl (ctl_fd, op, fd, &event);
            if (timeout != (unsigned)-1) {
                //
                // TODO: Optimize to only re-calculate timeout if new timeout is shorter than current
                //
                // We need to re-calculate the timeout in epoll_pwait, signal it
                send_signal = true;
            }
        }
        op_list.emplace_back (ioop);

        // Save the positions to make it easier to remove an ioop from our lists
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
    void IOHandler_Epoll::cancel (Connection& conn, bool rx, bool tx)
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

        bool rx_ops_removed = false;
        bool tx_ops_removed = false;

        if (rx && !rx_op_list.empty()) {
            // Clear read operations
            TRACE ("Erasing Rx operations for file descriptor %d", fd);
            rx_op_list.clear ();
            rx_ops_removed = true;
        }
        if (tx && !tx_op_list.empty()) {
            // Clear write operations
            TRACE ("Erasing Tx operations for file descriptor %d", fd);
            tx_op_list.clear ();
            tx_ops_removed = true;
        }

        // Nothing needed to be cancelled
        if (!rx_ops_removed && !tx_ops_removed)
            return;

        if (rx_op_list.empty() && tx_op_list.empty()) {
            // All operations are cancelled
            TRACE_POLL ("epoll_ctl (%s, %d, %s)",
                        epoll_op_to_string(EPOLL_CTL_DEL).c_str(), fd, "");
            epoll_ctl (ctl_fd, EPOLL_CTL_DEL, fd, nullptr);
            ops_map.erase (entry);
            if (fd_map_entry_removed.first == fd)
                fd_map_entry_removed.second = true; // fd removed from ops_map
        }else {
            struct epoll_event event;
            event.data.fd = fd;
            if (rx_ops_removed)
                event.events = EPOLLOUT; // RX operations cancelled, we still have TX operations
            if (tx_ops_removed)
                event.events = EPOLLIN;  // TX operations cancelled, we still have RX operations

            TRACE_POLL ("epoll_ctl (%s, %d, %s)",
                        epoll_op_to_string(EPOLL_CTL_MOD).c_str(), fd, events_to_string(event.events).c_str());
            epoll_ctl (ctl_fd, EPOLL_CTL_MOD, fd, &event); // Modify poll events
        }
    }


    //--------------------------------------------------------------------------
    // ops_mutex is locked!
    //--------------------------------------------------------------------------
    int IOHandler_Epoll::next_timeout ()
    {
        if (timeout_map.empty())
            return -1;

        int retval = 0;
        auto& later = timeout_map.begin()->first;

        timespec_less_t less;
        struct timespec now;
        struct timespec timeout;
        clock_gettime (CLOCK_MONOTONIC, &now);
        if (less(now, later)) {
            timeout = later - now;
            retval = timeout.tv_sec * 1000;
            retval += (int)(timeout.tv_nsec / 1000000L);
        }

        return retval;
    }


    //--------------------------------------------------------------------------
    // ops_mutex is locked!
    //--------------------------------------------------------------------------
    void IOHandler_Epoll::handle_timeout (struct timespec& now)
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
            ioop_list_t& op_list = is_rx ? ops_map_pos->RX_LIST : ops_map_pos->TX_LIST;
            ioop_list_t& other_op_list = is_rx ? ops_map_pos->TX_LIST : ops_map_pos->RX_LIST;
            int fd = ops_map_pos->first;

#ifdef TRACE_DEBUG_MISC
            auto diff = now - deadline;
            TRACE ("%s timeout on file descriptor %d by %u.%09u seconds",
                   (is_rx?"Rx":"Tx"), fd,
                   diff.tv_sec, diff.tv_nsec);
#endif
            op_list.erase (op_list_pos);
            if (op_list.empty()) {
                if (other_op_list.empty()) {
                    // No more TX/RX operations
                    TRACE_POLL ("epoll_ctl (%s, %d, %s)",
                                epoll_op_to_string(EPOLL_CTL_DEL).c_str(), fd, "");
                    epoll_ctl (ctl_fd, EPOLL_CTL_DEL, fd, nullptr);
                }else{
                    struct epoll_event event;
                    event.data.fd = fd;
                    event.events = is_rx ? EPOLLOUT : EPOLLIN;
                    TRACE_POLL ("epoll_ctl (%s, %d, %s)",
                                epoll_op_to_string(EPOLL_CTL_MOD).c_str(), fd, events_to_string(event.events).c_str());
                    epoll_ctl (ctl_fd, EPOLL_CTL_MOD, fd, &event);
                }
            }

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
    // ops_mutex is locked!
    //--------------------------------------------------------------------------
    void IOHandler_Epoll::io_dispatch (struct epoll_event* io_events, int num_events)
    {
        TRACE ("Handle I/O");
        for (int i=0; i<num_events; ++i) {
            auto fd = io_events[i].data.fd;
            auto events = io_events[i].events;

            if (fd < 0)
                continue;

            TRACE ("Events on file desc %d: 0x%08x (%s)",
                   fd, events, events_to_string(events).c_str());

            uint32_t err = events & (EPOLLERR|EPOLLHUP);
            if (events & (EPOLLOUT|EPOLLHUP|EPOLLERR))
                handle_event (fd, false, err);
            if (events & (EPOLLIN|EPOLLHUP|EPOLLERR))
                handle_event (fd, true, err);
        }
    }


    //--------------------------------------------------------------------------
    // ops_mutex is locked!
    //--------------------------------------------------------------------------
    void IOHandler_Epoll::handle_event (int fd, bool read, uint32_t error_flags)
    {
        TRACE ("Handle %s in file descriptor %d", (read?"input":"output"), fd);

        auto entry = ops_map.find (fd);
        if (entry == ops_map.end())
            return;

        auto* ioop_list = read ? &(entry->RX_LIST) : &(entry->TX_LIST);
        auto* other_ioop_list = read ? &(entry->TX_LIST) : &(entry->RX_LIST);
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

            // Update poll set if needed
            if (ioop_list->empty()) {
                if (other_ioop_list->empty()) {
                    // No more TX/RX operations
                    TRACE_POLL ("epoll_ctl (%s, %d, %s)",
                                epoll_op_to_string(EPOLL_CTL_DEL).c_str(), fd, "");
                    epoll_ctl (ctl_fd, EPOLL_CTL_DEL, fd, nullptr);
                }else{
                    struct epoll_event event;
                    event.data.fd = fd;
                    event.events = read ? EPOLLOUT : EPOLLIN;
                    TRACE_POLL ("epoll_ctl (%s, %d, %s)",
                                epoll_op_to_string(EPOLL_CTL_MOD).c_str(), fd, events_to_string(event.events).c_str());
                    epoll_ctl (ctl_fd, EPOLL_CTL_MOD, fd, &event);
                }
            }

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
                        other_ioop_list = read ? &(entry->TX_LIST) : &(entry->RX_LIST);
                    }else{
                        ioop_list = nullptr;
                        other_ioop_list = nullptr;
                        done = true;
                    }
                }
                fd_map_entry_removed.first = -1; // Invalidate the fd map check
            }
        }

        if (entry != ops_map.end())
            if (entry->RX_LIST.empty() && entry->TX_LIST.empty())
                ops_map.erase (entry);
    }





    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IOHandler_Epoll::ioop_t::ioop_t (timeout_map_t& tm,
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

        clock_gettime (CLOCK_MONOTONIC, &timeout);
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
    IOHandler_Epoll::ioop_t::~ioop_t ()
    {
        if (timeout_map_pos != timeout_map.end())
            timeout_map.erase (timeout_map_pos);
    }


}
