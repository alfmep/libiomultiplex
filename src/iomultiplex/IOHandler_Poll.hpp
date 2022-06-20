/*
 * Copyright (C) 2021,2022 Dan Arrhenius <dan@ultramarin.se>
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
#ifndef IOMULTIPLEX_IOHANDLER_POLL_HPP
#define IOMULTIPLEX_IOHANDLER_POLL_HPP

#include <iomultiplex/iohandler_base.hpp>
#include <iomultiplex/types.hpp>
#include <iomultiplex/Connection.hpp>
#include <iomultiplex/PollDescriptors.hpp>
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


namespace iomultiplex {


    /**
     * I/O handler.
     * This class is responsible for managing the I/O operations
     * of all the connection objects using it.
     */
    class IOHandler_Poll : public iohandler_base {
    public:
        /**
         * Constructor.
         * @param signal_num The signal number used internally by the IOHandler_Poll.
         *                   Default is SIGRTMIN. Change this if the application
         *                   is using this signal for other purposes.
         */
        IOHandler_Poll (int signal_num=SIGRTMIN);

        /**
         * Destructor.
         * Cancels all pending I/O operations and stops the I/O handling.
         * If a worker thread is running, it is stopped.
         */
        virtual ~IOHandler_Poll ();

        /**
         * Run the I/O handler until stopped with method 'stop' or an error occurrs.
         * This method will block unless a worker thread is requested, in
         * that case this method will return when the worker thread is up and running.
         * @param start_worker_thread If <code>true</code> start a new thread
         *                            to handle the I/O.
         * @return 0 on success, -1 on error and <code>errno</code> is set.
         */
        virtual int run (bool start_worker_thread=false);

        /**
         * Stop the I/O handler.
         * All pending I/O operations are cancelled.
         * \note If the I/O handler is run in a worker thread,
         * the worker thread is signaled to stop and this
         * method returns immediately. To block until the worker
         * thread is stopped, call method <code>join()</code>.
         * @see IOHandler_Poll::join
         */
        virtual void stop ();

        /**
         * Cancel all input and/or output operations for a connection.
         * This will cancel all I/O operations for the
         * specified connection. The pending I/O operations
         * will have a result of -1 and <code>errnum</code>
         * set to <code>ECANCELED</code>.
         */
        virtual void cancel (Connection& conn, bool cancel_rx=true, bool cancel_tx=true);

        /**
         * Check if the I/O handler is running in the same
         * context(i.e. same thread) as the caller.
         * @return <code>true</code> if the I/O handler is
         *         running in the same thread as the caller.
         */
        virtual bool same_context () const;

        /**
         * If the I/O handler has a worker thread running,
         * block until the worker thread is terminated.
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


        int cmd_signal;
        std::atomic_bool quit;
        std::atomic_bool is_running;
        volatile pid_t my_pid;
        state_t state;

        fd_ops_map_t ops_map;
        std::mutex ops_mutex;
        PollDescriptors poll_set;

        sigset_t orig_sigmask;
        struct sigaction orig_sa;

        std::pair<int, bool> fd_map_entry_removed;

        std::thread worker;

        timeout_map_t timeout_map;
        bool next_timeout (struct timespec& timeout);

        int start_running (bool start_worker_thread,
                           std::unique_lock<std::mutex>& lock);
        void end_running ();

        void handle_timeout (struct timespec& now);
        void handle_event (int fd, bool read, short error_flags);
        void io_dispatch ();
        void signal_event ();
    };


}
#endif
