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
         */
        IOHandler_Epoll (int signal_num=SIGRTMIN);

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
         * If the I/O handler is run in a worker thread,
         * wait for the worker thread to terminate.
         * If not, return immediately.
         */
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
        class ioop_t;
        enum class state_t {
            stopped,
            starting,
            running,
            stopping
        };

        using ioop_list_t = std::list<std::shared_ptr<ioop_t> >;
        using ops_t = std::pair<ioop_list_t,  // read operation queue
                                ioop_list_t>; // write operation queue
        using fd_ops_map_t = std::map<int, ops_t>;

        using timeout_map_t = std::multimap<struct timespec, ioop_t&, timespec_less_t>;


        int ctl_fd;
        int ctl_signal;
        std::atomic_bool quit;
        volatile pid_t worker_pid;
        static __thread pid_t caller_pid;
        state_t state;

        sigset_t orig_sigmask;    // Original signal mask before this object was created
        struct sigaction orig_sa; // Original signal handler for signal 'ctl_signal'

        fd_ops_map_t ops_map;
        std::mutex ops_mutex;
        std::pair<int, bool> fd_map_entry_removed;

        std::thread worker;
        int currently_handled_fd;

        timeout_map_t timeout_map;
        int next_timeout ();

        int start_running (bool start_worker_thread,
                           std::unique_lock<std::mutex>& lock);
        void end_running ();

        void handle_timeout (struct timespec& now);
        void io_dispatch (struct epoll_event* events, int num_events);
        void handle_event (int fd, bool read, uint32_t error_flags);
        void signal_event ();

        static std::map<int, unsigned> sigaction_count;
        static std::mutex sigaction_mutex;
    };


}
#endif
