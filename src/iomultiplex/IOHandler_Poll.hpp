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
     * An I/O handler using poll to wait for I/O events.
     * This class is responsible for managing the I/O operations
     * of all the connection objects using it.
     *
     * @deprecated Use <code>IOHandler_Epoll</code> instead.
     */
    class [[deprecated("Use iomultiplex::IOHandler_Epoll instead.")]] IOHandler_Poll
        : public iohandler_base
    {
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
         * If a worker thread is running, it is stopped before the destructor
         * returns.
         */
        virtual ~IOHandler_Poll ();

        virtual int run (bool start_worker_thread=false);
        virtual void stop ();
        virtual void cancel (Connection& conn,
                             bool cancel_rx=true,
                             bool cancel_tx=true,
                             bool fast=false);
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

        static std::map<int, unsigned> sigaction_count;
        static std::mutex sigaction_mutex;
    };


}
#endif
