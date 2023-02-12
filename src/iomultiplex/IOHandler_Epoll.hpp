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
     * I/O handler.
     */
    class IOHandler_Epoll : public iohandler_base {
    public:
        /**
         * Constructor.
         * @param signal_num The signal number used internally by the iohandler_base.
         *                   Default is SIGRTMIN. Change this if the application
         *                   is using this signal for other purposes.
         * @param max_events_hint The maximum number of events that epoll handles at a time.
         *                        Must be greater than zero.
         */
        IOHandler_Epoll (const int signal_num=SIGRTMIN, const int max_events_hint=32);

        /**
         * Destructor.
         */
        virtual ~IOHandler_Epoll ();

        /**
         * Run the I/O handler until stopped with method 'stop' or an error occurrs.
         * @param start_worker_thread If <code>true</code> start a new thread
         *                            to handle the I/O.
         * @return 0 on success. -1 on failure and <code>errno</code> is set.
         */
        virtual int run (bool start_worker_thread=false);

        /**
         * Stop the I/O handler.
         * If the I/O handler is run in a worker thread,
         * the worker thread is signaled to stop and then
         * this method returns. To wait for the worker thread
         * to be stopped, call method join.
         * @see iohandler_base::join
         */
        virtual void stop ();

        /**
         * Cancel all input and/or output operations for a connection.
         * This will cancel all I/O operations for the
         * specified connection. The pending I/O operations
         * will have a result of -1 and <code>errnum</code>
         * set to <code>ECANCELED</code>.
         * @param conn The connection for which to cancel RX/TX operations.
         * @param cancel_rx If <code>true</code> (default),
         *                  cancel all RX operations.
         * @param cancel_tx If <code>true</code> (default),
         *                  cancel all TX operations.
         */
        virtual void cancel (Connection& conn,
                             bool cancel_rx=true,
                             bool cancel_tx=true);

        /**
         * Check if the I/O handler is running in the same
         * context(i.e. same thread) as the caller.
         * @return <code>true</code> if the calling thread is
         *         running in the same thread as the I/O handler.
         */
        virtual bool same_context () const;

        /**
         * If the I/O handler has a worker thread running,
         * block until the worker thread is terminated.
         * If not, return immediately.
         * \note If the I/O handler has a worker thread
         *       running, and this method is called from
         *       within that thread, it will cause a deadlock
         *       and the application will probably terminate
         *       with an error.
         */
        virtual void join ();


    protected:
        /**
         * Queue a single I/O operation.
         * @param conn The connection perforimg the I/O.
         * @param buf A pointer to a data buffer being operated upon.
         * @param size The number of bytes to read/receive or write/send.
         * @param cb A callback that is called when the I/O
         *           operation has a result.
         * @param read <code>true</code> for an input operation,
         *             <code>true</code> for an output operation.
         * @param dummy_operation If <code>true</code>, no data
         *                        is actually read or written.
         * @param timeout Timeout in milliseconds for the I/O operation.
         *                Use -1 for no timeout.
         */
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
