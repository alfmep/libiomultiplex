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
#ifndef IOMULTIPLEX_IOHANDLER_EPOLL_HPP
#define IOMULTIPLEX_IOHANDLER_EPOLL_HPP

#include <iomultiplex/types.hpp>
#include <iomultiplex/Connection.hpp>
#include <iomultiplex/iohandler_base.hpp>
#include <functional>
#include <memory>
#include <atomic>
#include <vector>
#include <thread>
#include <mutex>
#include <list>
#include <map>
#include <set>
#include <ctime>
#include <signal.h>
#include <sys/types.h>
#include <poll.h>
#include <sys/epoll.h>


namespace iomultiplex {


    /**
     * An I/O handler using epoll to wait for I/O events.
     */
    class IOHandler_Epoll : public iohandler_base {
    public:
        /**
         * Constructor.
         * @param signal_num A signal number used internally by the
         *                   I/O handler when <code>epoll_pwait</code>
         *                   needs to be interrupted.<br/>
         *                   Default is <code>SIGRTMIN</code>.
         *                   Change this if the application is using
         *                   this signal for other purposes.
         * @param max_events_hint The maximum number of events that
         *                        epoll handles at a time.
         *                        Must be greater than zero.
         */
        IOHandler_Epoll (const int signal_num=SIGRTMIN, const int max_events_hint=32);

        /**
         * Destructor.
         * Cancels all pending I/O operations and stops the I/O handler.
         * If a worker thread is running, it is stopped before the destructor
         * returns.
         */
        virtual ~IOHandler_Epoll ();

        virtual int run (bool start_worker_thread=false);
        virtual void stop ();
        virtual void cancel (Connection& conn, bool cancel_rx=true, bool cancel_tx=true);
        virtual bool same_context () const;
        virtual void join ();


    protected:
        virtual int queue_io_op (Connection& conn,
                                 void* buf,
                                 size_t size,
                                 io_callback_t cb,
                                 const bool read,
                                 const bool dummy_operation,
                                 unsigned timeout);


    private:
        //
        // Internal types and methods
        //

        // State of the I/O handler when method run() is called
        enum class state_t {
            stopped,  // I/O handler is stopped (default, before and after run())
            starting, // I/O handler is starting up
            running,  // I/O handler is up and running
            stopping  // I/O handler is shutting down
        };

        class ioop_t; // A single I/O operation
        using ioop_list_t = std::list<std::shared_ptr<ioop_t>>; // A list of I/O operations
        using ops_t = std::pair<ioop_list_t,  // read operation queue
                                ioop_list_t>; // write operation queue

        // All I/O operations belonging to a specific file descriptor
        using fd_ops_map_t = std::map<int, ops_t>;

        // All I/O operations for each timeout
        using timeout_map_t = std::multimap<struct timespec, ioop_t&, timespec_less_t>;


        int ctl_fd;         // Control file descriptor (used by epoll)
        int ctl_signal;     // Signal used to interrupt epoll_pwait() when timeout needs to be recalculated
        int ctl_max_events; // Max events handled by epoll at a time

        sigset_t orig_sigmask;  // Original signal mask before running the I/O handler
        sigset_t epoll_sigmask; // Signal mask used during epoll_pwait

        state_t state;         // run state
        std::atomic_bool quit; // Flag used to quit the run() method.

        std::thread worker;               // Worker thread (optional usage)
        volatile pid_t worker_tid;        // Thread id of the worker thread (if any)
        volatile pid_t worker_tgid;       // Thread group id of the worker thread (if any)
        static __thread pid_t caller_tid; // Thread id of the thread calling methods in this class

        std::mutex ops_mutex;      // Lock used when modifying state regarding I/O operations
        fd_ops_map_t ops_map;      // I/O queues for each file descriptor
        timeout_map_t timeout_map; // Sorted timeout values mapping I/O operations
        std::pair<int, bool> fd_map_entry_removed; // Items in ops_map erased while processing I/O
        int currently_handled_fd; // The current file descriptor being processed

        std::set<int> rx_cancel_map; // RX file descriptors that are being cancelled
        std::set<int> tx_cancel_map; // TX file descriptors that are being cancelled


        int initialize_ctl_signal ();
        void restore_ctl_signal ();
        int start_running (bool start_worker_thread,
                           std::unique_lock<std::mutex>& lock);
        void end_running ();

        int queue_io_op_sanity_check (const int fd, const bool read);
        int next_timeout ();
        void call_ioop_cb (ioop_t& ioop, const ssize_t result, const int errnum);
        void handle_timeout (struct timespec& now);
        void io_dispatch (struct epoll_event* events, int num_events);
        void handle_event (int fd, bool read, uint32_t error_flags);
        void interrupt_epoll ();

        void handle_cancelled_ops ();
        void cancel_while_stopped (Connection& conn, bool rx, bool tx);


        // Control signals used by all instances of this class.
        // Iterator: first=signal, second.first=usage_count, second.second=original_sa_handler
        using sigaction_map_t = std::map<int, std::pair<unsigned, struct sigaction>>;

        // Multiple instances may use the same control signal.
        // Keep track of the number each signal is used so we
        // don't remove the signal action when we shouldn't.
        static sigaction_map_t sigaction_map;
        static std::mutex sigaction_mutex;
        static void initialize_sig_handler (const int ctl_signal);
        static void restore_sig_handler (const int ctl_signal);
    };


}
#endif
