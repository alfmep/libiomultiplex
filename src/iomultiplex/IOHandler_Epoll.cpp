/*
 * Copyright (C) 2021-2025 Dan Arrhenius <dan@ultramarin.se>
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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>


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
#ifdef TX_LIST
#  undef TX_LIST
#endif
#define RX_LIST second.first
#define TX_LIST second.second



namespace iomultiplex {


    IOHandler_Epoll::sigaction_map_t IOHandler_Epoll::sigaction_map;
    std::mutex IOHandler_Epoll::sigaction_mutex;


    constexpr static pid_t invalid_pid = (pid_t) -1;

    __thread pid_t IOHandler_Epoll::caller_tid {invalid_pid};


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
        if (events & EPOLLET) {
            if (!first_event)
                ss << '|';
            first_event = false;
            ss << "ET";
        }
        if (events & EPOLLONESHOT) {
            if (!first_event)
                ss << '|';
            first_event = false;
            ss << "ONESHOT";
        }
        if (events & EPOLLWAKEUP) {
            if (!first_event)
                ss << '|';
            first_event = false;
            ss << "WAKEUP";
        }
#ifdef EPOLLEXCLUSIVE
        if (events & EPOLLEXCLUSIVE) {
            if (!first_event)
                ss << '|';
            first_event = false;
            ss << "EXCLUSIVE";
        }
#endif
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
                Connection& c, void* b, size_t s,
                const io_callback_t& cb,
                unsigned timeout_ms, const bool dummy);

        virtual ~ioop_t ();

        io_callback_t   cb;         // Callback to be called when the operation is done.
        struct timespec abs_timeout;// Timeout value in absolute time.
        timeout_map_t& timeout_map; // A reference to the timeout map holding this instance.
        timeout_map_t::iterator timeout_map_pos; // Position of this instance in timeout_map

        // Make it easy to erase an ioop_t object from the IOHandler_Epolls containers
        ioop_list_t::iterator ioop_list_pos; // Position in ioop list (tx list or rx list)
        fd_ops_map_t::iterator ops_map_pos;  // Position in file_desc->ioop_lists map

        bool dummy_op; // A dummy operation, don't actually try to read or write anything.
        bool is_rx;    // if true, an RX operation. If false, a TX operation.
    };





    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static void ctl_signal_handler (int sig, siginfo_t* si, void* ucontext)
    {
        TRACE_SIG ("Got signal #%d", sig);
        return;
    }


    //--------------------------------------------------------------------------
    // Initialize the control signal handler used for interrupting epoll_pwait().
    //--------------------------------------------------------------------------
    void IOHandler_Epoll::initialize_sig_handler (const int ctl_signal)
    {
        std::lock_guard<std::mutex> sig_lock (sigaction_mutex);

        struct sigaction new_sa;
        memset (&new_sa, 0, sizeof(new_sa));
        auto sa_map_entry = sigaction_map.try_emplace(ctl_signal, std::make_pair(0, new_sa)).first;

        if (sa_map_entry->second.first++ == 0) {
            struct sigaction& orig_sa = sa_map_entry->second.second;

            // Install signal handler used for interrupting epoll_pwait().
            //
            TRACE_SIG ("Install handler for signal #%d", ctl_signal);
            struct sigaction sa;
            memset (&sa, 0, sizeof(sa));
            sigemptyset (&sa.sa_mask);
            sa.sa_flags = SA_SIGINFO;
            sa.sa_sigaction = ctl_signal_handler;

            if (sigaction(sa_map_entry->first, &sa, &orig_sa) < 0) {
                throw std::system_error (errno, std::system_category(),
                                         "Unable to install signal handler");
            }
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void IOHandler_Epoll::restore_sig_handler (const int ctl_signal)
    {
        std::lock_guard<std::mutex> sig_lock (sigaction_mutex);

        auto sa_map_entry = sigaction_map.find (ctl_signal);
        if (sa_map_entry!=sigaction_map.end() && --sa_map_entry->second.first == 0) {
            // Restore the original signal handler
            auto& orig_sa = sa_map_entry->second.second;
            sigaction (ctl_signal, &orig_sa, nullptr);
            sigaction_map.erase (sa_map_entry);
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int IOHandler_Epoll::initialize_ctl_signal ()
    {
        // First, Get the original signal mask
        sigprocmask (SIG_BLOCK, nullptr, &orig_sigmask);
        // Then, make sure the control signal is unblocked during epoll_pwait
        memcpy (&epoll_sigmask, &orig_sigmask, sizeof(sigset_t));
        sigdelset (&epoll_sigmask, ctl_signal);

        // Now block the control signal used for interrupting epoll_pwait().
        sigset_t ctl_sigset;
        sigemptyset (&ctl_sigset);
        sigaddset (&ctl_sigset, ctl_signal);
        return sigprocmask (SIG_BLOCK, &ctl_sigset, nullptr);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void IOHandler_Epoll::restore_ctl_signal ()
    {
        sigprocmask (SIG_BLOCK, &orig_sigmask, nullptr);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IOHandler_Epoll::IOHandler_Epoll (const int signal_num,
                                      const int max_events_hint)
        : ctl_fd {-1},
          ctl_signal {signal_num},
          ctl_max_events {max_events_hint},
          state {state_t::stopped},
          quit {true},
          worker_tid {invalid_pid},
          worker_tgid {invalid_pid},
          fd_map_entry_removed {std::make_pair(-1, false)},
          currently_handled_fd {-1}
    {
        if (ctl_max_events <= 0)
            throw std::system_error (EINVAL, std::system_category(),
                                     "Invalid value to parameter max_events_hint");

        // Create the epoll control descriptor
        //
        ctl_fd = epoll_create1 (EPOLL_CLOEXEC);
        if (ctl_fd < 0) {
            throw std::system_error (errno,
                                     std::system_category(),
                                     "epoll_create failed");
        }

        initialize_sig_handler (ctl_signal);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IOHandler_Epoll::~IOHandler_Epoll ()
    {
        stop ();
        join ();
        if (ctl_fd >= 0)
            close (ctl_fd);
        restore_sig_handler (ctl_signal);
    }


    //--------------------------------------------------------------------------
    // ops_mutex is locked
    // Return:
    //  -1 - error starting
    //   0 - starting in same thread
    //   1 - starting in new thread
    //--------------------------------------------------------------------------
    int IOHandler_Epoll::start_running (bool start_worker_thread,
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
                // Call run() from the worker thread
                run (false);
            });

            ops_lock.unlock ();
            while (worker_tid == invalid_pid)
                ; // Busy-wait until the worker thread has found its thread id
            errno = 0;
            return 1; // Started I/O handler in a worker thread
        }

        return 0; // Started I/O handler in this thread
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int IOHandler_Epoll::run (bool start_worker_thread)
    {
        std::unique_lock<std::mutex> ops_lock (ops_mutex);

        int progress = start_running (start_worker_thread, ops_lock);
        if (progress) {
            // progress == -1: Error, return -1 and errno is set
            // progress ==  1: a worker thread was created in start_running(),
            //                 return 0 (success)
            return progress<0 ? -1 : 0;
        }
        // progress == 0: run() is called either without a worker thread,
        //                or from the worker thread created in start_running()

        quit = false;
        state = state_t::running;
        worker_tgid = getpid ();
        worker_tid = (pid_t) syscall (SYS_gettid);
        initialize_ctl_signal ();

        TRACE ("Start processing I/O");

        // Poll until instructed to stop (or poll itself fails)
        //
        int errnum = 0;
        struct epoll_event events[ctl_max_events];

        while (!quit) {
            int timeout = next_timeout ();
            TRACE_POLL ("start epoll_pwait, timeout value: %d", timeout);

            ops_lock.unlock ();
            auto num_events = epoll_pwait (ctl_fd,
                                           events,
                                           ctl_max_events,
                                           timeout,
                                           &epoll_sigmask);
            ops_lock.lock ();

            TRACE_POLL ("epoll_pwait result: %d", num_events);

            // I/O operations might have been
            // cancelled while epoll_wait() slept
            handle_cancelled_ops ();

            if (num_events < 0) {
                if (errno != EINTR) {
                    // epoll_pwait() failed !!!
                    errnum = errno;
                    quit = true;
                    TRACE ("IOHandler_Epoll: error polling I/O: %s", strerror(errnum));
                }else{
                    ; // epoll_pwait() was interrupted to recalculate the timeout
                }
            }
            else if (num_events > 0) {
                io_dispatch (events, num_events);
                // I/O operations might have been
                // cancelled in io_dispatch()
                handle_cancelled_ops ();
            }
            else if (!timeout_map.empty()) {
                struct timespec ts;
                clock_gettime (CLOCK_MONOTONIC, &ts);
                handle_timeout (ts);
                // I/O operations might have been
                // cancelled in handle_timeout()
                handle_cancelled_ops ();
            }
        }
        state = state_t::stopping;

        // clean up
        end_running ();
        restore_ctl_signal ();
        TRACE ("Finished processing I/O ");
        worker_tgid = invalid_pid;
        worker_tid = invalid_pid;
        errno = errnum;
        state = state_t::stopped;
        return errno==0 ? 0 : -1;
    }


    //--------------------------------------------------------------------------
    // Assume ops_mutex is locked !!!
    //--------------------------------------------------------------------------
    void IOHandler_Epoll::call_ioop_cb (ioop_t& ioop, const ssize_t result, const int errnum)
    {
        if (ioop.cb) {
            ioop.result = result;
            ioop.errnum = errnum;
            ops_mutex.unlock ();
            ioop.cb (ioop);
            ops_mutex.lock ();
        }
    }


    //--------------------------------------------------------------------------
    // Assume ops_mutex is locked !!!
    //--------------------------------------------------------------------------
    void IOHandler_Epoll::end_running ()
    {
        for (auto& entry : ops_map) {
            // Cancel RX operations
            for (auto& ioop : entry.RX_LIST)
                call_ioop_cb (*ioop, -1, ECANCELED);
            entry.RX_LIST.clear ();

            // Cancel TX operations
            for (auto& ioop : entry.TX_LIST)
                call_ioop_cb (*ioop, -1, ECANCELED);
            entry.TX_LIST.clear ();
        }

        rx_cancel_map.clear ();
        tx_cancel_map.clear ();
        ops_map.clear ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void IOHandler_Epoll::interrupt_epoll ()
    {
        if (worker_tid != invalid_pid) {
            TRACE_SIG ("Send signal %d to thread id %u", ctl_signal, (unsigned)worker_tid);
            int err = syscall (SYS_tgkill, worker_tgid, worker_tid, ctl_signal);
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
            if (!same_context())
                interrupt_epoll ();
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool IOHandler_Epoll::same_context () const
    {
        if (worker_tid == invalid_pid) {
            return true;
        }else{
            if (caller_tid == invalid_pid)
                caller_tid = (pid_t) syscall (SYS_gettid);
            return worker_tid == caller_tid;
        }
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
    static bool is_fd_a_file (int fd)
    {
        struct stat s;
        return fstat(fd, &s) == 0;
    }


    //--------------------------------------------------------------------------
    // ops_mutex is locked
    //--------------------------------------------------------------------------
    int IOHandler_Epoll::queue_io_op_sanity_check (const int fd, const bool read)
    {
        if (fd < 0) {
            // We can't queue an I/O operation with an invalid file descriptor
            errno = EBADF;
            return -1;
        }
        if (state == state_t::stopping) {
            // We can't queue an I/O operation while the I/O handler is shutting down
            errno = ECANCELED;
            return -1;
        }
        if (read && rx_cancel_map.find(fd)!=rx_cancel_map.end()) {
            // RX operations are being cancelled
            errno = ECANCELED;
            return -1;
        }
        if (!read && tx_cancel_map.find(fd)!=tx_cancel_map.end()) {
            // TX operations are being cancelled
            errno = ECANCELED;
            return -1;
        }
        return 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int IOHandler_Epoll::queue_io_op (Connection& conn,
                                void* buf,
                                size_t size,
                                io_callback_t cb,
                                const bool read,
                                const bool dummy_operation,
                                unsigned timeout)
    {
        std::lock_guard<std::mutex> lock (ops_mutex);

        int fd = conn.handle ();
        if (queue_io_op_sanity_check(fd, read))
            return -1;

        TRACE ("Queue a %s%s operation on file desc %d, %u bytes requested",
               (dummy_operation?"dummy ":""), (read?"Rx":"Tx"), fd, size);

        std::shared_ptr<ioop_t> ioop (std::make_shared<ioop_t>(
                                              timeout_map, read, conn, buf, size,
                                              cb, timeout, dummy_operation));

        bool send_signal {false};
        bool new_ops_map_entry {false};

        auto entry = ops_map.find (fd);
        if (entry == ops_map.end()) {
            entry = ops_map.emplace (fd, std::make_pair(ioop_list_t(),         // Rx list
                                                        ioop_list_t())).first; // Tx list
            new_ops_map_entry = true;
        }

        auto& op_list {read ? entry->RX_LIST : entry->TX_LIST};

        bool is_same_context = same_context ();
        if (fd!=currently_handled_fd || !is_same_context || state!=state_t::running) {
            if (op_list.empty()) {
                auto& other_op_list {read ? entry->TX_LIST : entry->RX_LIST};
                int op;
                struct epoll_event event;
                event.data.fd = fd;
                if (other_op_list.empty()) {
                    op = EPOLL_CTL_ADD;
                    event.events = read ? EPOLLIN : EPOLLOUT;
                }else{
                    op = EPOLL_CTL_MOD;
                    event.events = EPOLLIN | EPOLLOUT;
                }
                TRACE_POLL ("epoll_ctl (%s, %d, %s)",
                            epoll_op_to_string(op).c_str(), fd, events_to_string(event.events).c_str());
                if (epoll_ctl(ctl_fd, op, fd, &event) &&
                    (op != EPOLL_CTL_ADD || errno != EEXIST)) // Ignore EEXIST on EPOLL_CTL_ADD
                {
                    int errnum = errno; // save errno
                    if (is_fd_a_file(fd)) {
                        Log::warning ("Can't use epoll with regular files, fd: %d", fd);
                    }else{
                        Log::warning ("epoll_ctl(%s, %d, %s) failed: %s",
                                      epoll_op_to_string(op).c_str(),
                                      fd,
                                      events_to_string(event.events).c_str(),
                                      strerror(errnum));
                    }
                    if (new_ops_map_entry)
                        ops_map.erase (entry);

                    errno = errnum; // restore errno
                    return -1;
                }
            }
            if (timeout != (unsigned)-1) {
                if (ioop->timeout_map_pos == ioop->timeout_map.begin()) {
                    // We need to re-calculate the timeout in epoll_pwait, signal it
                    send_signal = true;
                }
            }
        }
        op_list.emplace_back (ioop);

        // Save the positions to make it easier to remove an ioop from our lists
        ioop->ops_map_pos = entry;
        ioop->ioop_list_pos = op_list.end ();
        --ioop->ioop_list_pos;

        if (send_signal && !is_same_context)
            interrupt_epoll (); // Interrupt call to epoll_pwait in order to recalculate timeout

        errno = 0;
        return 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void IOHandler_Epoll::cancel (Connection& conn,
                                  bool rx,
                                  bool tx,
                                  bool fast)
    {
        if (!rx && !tx)
            return;

        TRACE ("Cancel%s%s%s for fd %d",
               (rx ? " RX" : ""),
               (tx ? " TX" : ""),
               (fast ? " fast" : ""),
               conn.handle());

        auto fd = conn.handle ();
        if (fd < 0)
            return; // Invalid file handle

        std::unique_lock<std::mutex> lock (ops_mutex);

        if (state == state_t::stopping)
            return; // I/O handler stopping and cleaning up

        auto io_ops = ops_map.find (fd);
        if (io_ops == ops_map.end())
            return; // No I/O operations found for this file descriptor

        auto& rx_op_list {io_ops->RX_LIST};
        auto& tx_op_list {io_ops->TX_LIST};

        if (rx && rx_op_list.empty())
            rx = false;
        if (tx && tx_op_list.empty())
            tx = false;

        if (!rx && !rx)
            return; // No operations left to cancel

        if (fast) {
            //
            // Cancel all I/O operatons directly
            // without calling any of the operations
            // callback functions.
            //
            if (rx) {
                rx_cancel_map.erase (fd);
                rx_op_list.clear ();
            }
            if (tx) {
                tx_cancel_map.erase (fd);
                tx_op_list.clear ();
            }

            // Update the epoll events for this file descriptor
            //
            if (rx_op_list.empty() && tx_op_list.empty()) {
                // No more I/O operations for this file descriptor
                TRACE_POLL ("epoll_ctl (%s, %d, nullptr)",
                            epoll_op_to_string(EPOLL_CTL_DEL).c_str(), fd);
                epoll_ctl (ctl_fd, EPOLL_CTL_DEL, fd, nullptr);
                ops_map.erase (io_ops);
                if (fd_map_entry_removed.first == fd)
                    fd_map_entry_removed.second = true; // fd removed from ops_map
            }else{
                struct epoll_event event;
                event.data.fd = fd;
                if (rx)
                    event.events = EPOLLOUT; // RX operations cancelled, we still have TX operations
                if (tx)
                    event.events = EPOLLIN;  // TX operations cancelled, we still have RX operations
                TRACE_POLL ("epoll_ctl (%s, %d, %s)",
                            epoll_op_to_string(EPOLL_CTL_MOD).c_str(), fd, events_to_string(event.events).c_str());
                epoll_ctl (ctl_fd, EPOLL_CTL_MOD, fd, &event); // Modify poll events
            }
        }else{
            //
            // Cancel all I/O operations in an ordelry fashion,
            // and call the operations callback functions.
            // Until all operations for this connection are
            // cancelled, new operations of the same type
            // that are cancelled are allowed.
            //
            if (rx && rx_cancel_map.emplace(fd).second==false)
                rx = false;
            if (tx && tx_cancel_map.emplace(fd).second==false)
                tx = false;

            if (rx || tx) {
                if (state == state_t::stopped) {
                    cancel_while_stopped (conn, rx, tx);
                }else if (!same_context()) {
                    // Interrupt epoll_pwait() to handle cancelled I/O operations
                    interrupt_epoll ();
                }
            }
        }
    }


    //--------------------------------------------------------------------------
    // ops_mutex is locked!
    //--------------------------------------------------------------------------
    void IOHandler_Epoll::cancel_while_stopped (Connection& conn, bool rx, bool tx)
    {
        auto fd = conn.handle ();
        auto entry = ops_map.find (fd);

        if (rx) {
            // Cancel RX operations
            for (auto& ioop : entry->RX_LIST)
                call_ioop_cb (*ioop, -1, ECANCELED);
            entry->RX_LIST.clear ();
            rx_cancel_map.erase (fd);
        }
        if (tx) {
            // Cancel TX operations
            for (auto& ioop : entry->TX_LIST)
                call_ioop_cb (*ioop, -1, ECANCELED);
            entry->TX_LIST.clear ();
            tx_cancel_map.erase (fd);
        }

        if (entry->RX_LIST.empty() && entry->TX_LIST.empty()) {
            // All operations for this file descriptor are cancelled
            TRACE_POLL ("epoll_ctl (%s, %d, nullptr)",
                        epoll_op_to_string(EPOLL_CTL_DEL).c_str(), fd);
            epoll_ctl (ctl_fd, EPOLL_CTL_DEL, fd, nullptr);
            ops_map.erase (entry);
        }else{
            struct epoll_event event;
            event.data.fd = fd;
            if (rx)
                event.events = EPOLLOUT; // RX operations cancelled, we still have TX operations
            if (tx)
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
    // Worker context.
    // epoll_pwait() not running
    //--------------------------------------------------------------------------
    void IOHandler_Epoll::handle_cancelled_ops ()
    {
        static constexpr const int rx_op = 0;
        static constexpr const int tx_op = 1;
        std::set<int>* cancel_map[2] = {&rx_cancel_map, &tx_cancel_map};

        while (!cancel_map[rx_op]->empty() || !cancel_map[tx_op]->empty()) {
            for (int op_type=rx_op; op_type<=tx_op; ++op_type) {
                while (!cancel_map[op_type]->empty()) {
                    auto fd_entry = cancel_map[op_type]->begin ();
                    int fd = *fd_entry;

                    auto ops_map_entry = ops_map.find (fd);
                    if (ops_map_entry == ops_map.end()) {
                        cancel_map[op_type]->erase (fd_entry);
                        continue;
                    }

                    auto& ioop_list = op_type==rx_op ? ops_map_entry->RX_LIST : ops_map_entry->TX_LIST;
                    auto& other_ioop_list = op_type==rx_op ? ops_map_entry->TX_LIST : ops_map_entry->RX_LIST;
                    bool ops_cancelled = false;
                    for (auto& ioop : ioop_list) {
                        ops_cancelled = true;
                        // Callbacks can't add operations for this fd since it's cancelling
                        call_ioop_cb (*ioop, -1, ECANCELED);
                    }

                    // Only modify epoll events if operations were actually removed
                    if (ops_cancelled == false) {
                        cancel_map[op_type]->erase (fd_entry);
                        continue;
                    }

                    ioop_list.clear ();
                    if (other_ioop_list.empty()) {
                        // Neither RX nor TX operations left for this file descriptor
                        TRACE_POLL ("epoll_ctl (%s, %d, nullptr)",
                                    epoll_op_to_string(EPOLL_CTL_DEL).c_str(), fd);
                        epoll_ctl (ctl_fd, EPOLL_CTL_DEL, fd, nullptr);
                        ops_map.erase (ops_map_entry);
                    }else{
                        // Only RX or TX operations left for this file desccriptor
                        struct epoll_event event;
                        event.data.fd = fd;
                        event.events = op_type==rx_op ? EPOLLOUT : EPOLLIN;
                        TRACE_POLL ("epoll_ctl (%s, %d, %s)",
                                    epoll_op_to_string(EPOLL_CTL_MOD).c_str(),
                                    fd, events_to_string(event.events).c_str());
                        epoll_ctl (ctl_fd, EPOLL_CTL_MOD, fd, &event);
                    }
                    cancel_map[op_type]->erase (fd_entry);
                }
            }
        }
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
                                -1,
                                ETIMEDOUT,
                                ioop.timeout);

            bool is_rx = ioop.is_rx;
            fd_ops_map_t::iterator ops_map_pos = ioop.ops_map_pos;
            ioop_list_t::iterator op_list_pos = ioop.ioop_list_pos;
            ioop_list_t& op_list = is_rx ? ops_map_pos->RX_LIST : ops_map_pos->TX_LIST;

            int fd = ops_map_pos->first;

            // Get the current epoll events for this file descriptor
            uint32_t current_epoll_events = 0;
            if (!ops_map_pos->RX_LIST.empty())
                current_epoll_events = EPOLLIN;
            if (!ops_map_pos->TX_LIST.empty())
                current_epoll_events |= EPOLLOUT;

#ifdef TRACE_DEBUG_MISC
            auto diff = now - deadline;
            TRACE ("%s timeout on file descriptor %d by %u.%09u seconds",
                   (is_rx?"Rx":"Tx"), fd,
                   diff.tv_sec, diff.tv_nsec);
#endif

            // Remove the I/O operation from the queue
            // (its destructor will remove the entry from the timeout mep)
            op_list.erase (op_list_pos);
            if (ops_map_pos->RX_LIST.empty() && ops_map_pos->TX_LIST.empty())
                ops_map.erase (ops_map_pos);

            currently_handled_fd = fd;
            if (callback) {
                // Call the callback
                ops_mutex.unlock ();
                callback (result);
                ops_mutex.lock ();
            }
            currently_handled_fd = -1;

            // Update epoll events, if needed
            //
            uint32_t new_epoll_events = 0;
            auto ops_map_entry = ops_map.find (fd);
            if (ops_map_entry != ops_map.end()) {
                if (!ops_map_entry->RX_LIST.empty())
                    new_epoll_events = EPOLLIN;
                if (!ops_map_entry->TX_LIST.empty())
                    new_epoll_events |= EPOLLOUT;
            }
            if (new_epoll_events != current_epoll_events) {
                if (new_epoll_events == 0) {
                    // No more TX/RX operations
                    TRACE_POLL ("epoll_ctl (%s, %d, nullptr)",
                                epoll_op_to_string(EPOLL_CTL_DEL).c_str(), fd);
                    epoll_ctl (ctl_fd, EPOLL_CTL_DEL, fd, nullptr);
                }else{
                    struct epoll_event event;
                    event.data.fd = fd;
                    event.events = new_epoll_events;
                    TRACE_POLL ("epoll_ctl (%s, %d, %s)",
                                epoll_op_to_string(EPOLL_CTL_MOD).c_str(), fd, events_to_string(event.events).c_str());
                    epoll_ctl (ctl_fd, EPOLL_CTL_MOD, fd, &event);
                }
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

            currently_handled_fd = fd;
            uint32_t rxtx = events & (EPOLLOUT|EPOLLIN);
            uint32_t err  = events & (EPOLLERR|EPOLLHUP);
            if (rxtx == 0) {
                // Error condition without specific direction
                handle_event (fd, false, err); // Write
                handle_event (fd, true,  err); // Read
            }else{
                if (rxtx & EPOLLOUT)
                    handle_event (fd, false, err);
                if (rxtx & EPOLLIN)
                    handle_event (fd, true, err);
            }
            currently_handled_fd = -1;
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

        uint32_t current_epoll_events = 0;
        if (!entry->RX_LIST.empty())
            current_epoll_events = EPOLLIN;
        if (!entry->TX_LIST.empty())
            current_epoll_events |= EPOLLOUT;

        auto* ioop_list = read ? &(entry->RX_LIST) : &(entry->TX_LIST);
        auto* cancel_map = read ? &tx_cancel_map : &tx_cancel_map;
        bool done {false};
        TRACE ("File descriptor %d have %d %s operation(s)", fd, ioop_list->size(), (read?"input":"output"));
        while (!quit && !done && !ioop_list->empty()) {
            TRACE ("Handle %s operation on %d", (read?"input":"output"), fd);

            auto ioop = ioop_list->front ();

            if (error_flags) {
                socklen_t len = sizeof (ioop->errnum);
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&ioop->errnum, &len))
                    ioop->errnum = EIO;
                ioop->result = ioop->errnum ? -1 : 0;
                done = true;
            }
            else if (ioop->dummy_op) {
                // Dummy operation, don't read or write anything
                ioop->result = 0;
                ioop->errnum = 0;
            }
            else{
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
                // File descriptor not ready, abort here and continue polling
                ioop->result = 0;
                ioop->errnum = 0;
                done = true;
                break;
            }

            // Remove this operation from the I/O operation queue
            ioop_list->pop_front ();

            if (ioop->cb == nullptr) {
                // No I/O callback, done
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

            if (!done && !!ioop_list->empty()) {
                // Before handling the next operation, check if a callback cancelled operations
                if (cancel_map->find(fd) != cancel_map->end())
                    done = true;
            }
        }

        uint32_t new_epoll_events = 0;

        if (entry != ops_map.end()) {
            if (!entry->RX_LIST.empty())
                new_epoll_events = EPOLLIN;
            if (!entry->TX_LIST.empty())
                new_epoll_events |= EPOLLOUT;

            if (new_epoll_events == 0)
                ops_map.erase (entry);
        }
        if (new_epoll_events != current_epoll_events) {
            if (new_epoll_events == 0) {
                // No more TX/RX operations
                TRACE_POLL ("epoll_ctl (%s, %d, nullptr)",
                            epoll_op_to_string(EPOLL_CTL_DEL).c_str(), fd);
                epoll_ctl (ctl_fd, EPOLL_CTL_DEL, fd, nullptr);
            }else{
                struct epoll_event event;
                event.data.fd = fd;
                event.events = new_epoll_events;
                TRACE_POLL ("epoll_ctl (%s, %d, %s)",
                            epoll_op_to_string(EPOLL_CTL_MOD).c_str(), fd, events_to_string(event.events).c_str());
                epoll_ctl (ctl_fd, EPOLL_CTL_MOD, fd, &event);
            }
        }
    }





    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IOHandler_Epoll::ioop_t::ioop_t (timeout_map_t& tm,
                                     bool read,
                                     Connection& c,
                                     void* b,
                                     size_t s,
                                     const io_callback_t& callback,
                                     unsigned timeout_ms,
                                     const bool dummy)
        : io_result_t (c, b, s, 0, 0, timeout_ms),
          cb {callback},
          timeout_map {tm},
          dummy_op {dummy},
          is_rx {read}
    {
        if (timeout_ms == (unsigned)-1) {
            abs_timeout.tv_sec = 0;
            abs_timeout.tv_nsec = 0;
            timeout_map_pos = timeout_map.end ();
            return;
        }

        clock_gettime (CLOCK_MONOTONIC, &abs_timeout);
        while (timeout_ms >= 1000) {
            ++abs_timeout.tv_sec;
            timeout_ms -= 1000;
        }
        abs_timeout.tv_nsec += timeout_ms * 1'000'000;
        if (abs_timeout.tv_nsec >= 1'000'000'000L) {
            ++abs_timeout.tv_sec;
            abs_timeout.tv_nsec -= 1'000'000'000L;
        }
        timeout_map_pos = timeout_map.emplace (abs_timeout, *this);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    IOHandler_Epoll::ioop_t::~ioop_t ()
    {
        if (timeout_map_pos != timeout_map.end())
            timeout_map.erase (timeout_map_pos);
    }


}
