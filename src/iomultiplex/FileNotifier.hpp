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
#ifndef IOMULTIPLEX_FILENOTIFIER_HPP
#define IOMULTIPLEX_FILENOTIFIER_HPP

#include <iomultiplex/FdConnection.hpp>
#include <iomultiplex/BufferPool.hpp>
#include <functional>
#include <string>
#include <mutex>
#include <map>
#include <cstdint>
#include <sys/inotify.h>


namespace iomultiplex {

    // Forward declaration
    class iohandler_base;

    /**
     * File I/O connection.
     * This class is used for reading/writing files.
     */
    class FileNotifier : public FdConnection {
    public:
        /**
         * Callback for watched files/directoies.
         * @param fn The FileNotifier object handling the notification.
         * @param pathname The pathname as specified in method add_watch().
         * @param event The event(s) triggering the watch callback.
         * @param cookie Unique cookie associating related events.
         * @param name If a directory is watched, this is the name
         *        of a file in that directory that caused the event.
         */
        using watch_cb = std::function<void (FileNotifier& fn,
                                             const std::string& pathname,
                                             uint32_t event,
                                             uint32_t cookie,
                                             const std::string& name)>;

        /**
         * Constructor.
         * @param io_handler This object will manage I/O operations.
         */
        FileNotifier (iohandler_base& io_handler);

        /**
         * Move Constructor.
         * @param fn The FileNotifier object to move.
         */
        FileNotifier (FileNotifier&& fn);

        /**
         * Destructor.
         * Closes the file.
         */
        virtual ~FileNotifier () = default;

        /**
         * Move operator.
         * @param fn The FileNotifier object to move.
         * @return A reference to this object.
         */
        FileNotifier& operator= (FileNotifier&& fn);

        /**
         * Add a file watcher.
         * @param pathname The name of the file/directory to watch.
         * @param events Events to watch.
         * @param callback A callback to be called when the watch is triggered.
         * @return 0 on success, -1 on failure and <code>errno</code> is set.
         */
        int add_watch (const std::string& pathname, uint32_t events, watch_cb callback);

        /**
         * Remove a file watcher.
         * @param pathname The name of the file/directory that is watched.
         */
        void remove_watch (const std::string& pathname);

        /**
         * Cancel all file watchers.
         * @param cancel_rx This parameter is ignored.
         * @param cancel_tx This parameter is ignored.
         * @param fast This parameter is ignored.
         */
        virtual void cancel (bool cancel_rx,
                             bool cancel_tx,
                             bool fast);


    private:
        FileNotifier () = delete;
        FileNotifier (const FileNotifier& fc) = delete;
        FileNotifier& operator= (const FileNotifier& fc) = delete;

        std::mutex watch_mutex;
        std::map<int, std::string> watchers;
        BufferPool pool;
        void notified (io_result_t& ior, watch_cb callback);
    };


}
#endif
