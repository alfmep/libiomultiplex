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
#include <iomultiplex/iohandler_base.hpp>
#include <iomultiplex/FileNotifier.hpp>
#include <climits>
#include <cerrno>


namespace iomultiplex {


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    FileNotifier::FileNotifier (iohandler_base& io_handler)
        : FdConnection (io_handler),
          pool (sizeof(inotify_event)+NAME_MAX+1, 4, 16)
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    FileNotifier::FileNotifier (FileNotifier&& fn)
        : FdConnection (std::forward<FileNotifier>(fn)),
          pool (sizeof(inotify_event)+NAME_MAX+1, 4, 16)
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    FileNotifier& FileNotifier::operator= (FileNotifier&& fn)
    {
        return *this;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int FileNotifier::add_watch (const std::string& pathname, uint32_t events, watch_cb callback)
    {
        std::lock_guard<std::mutex> lock (watch_mutex);

        if (fd < 0) {
            fd = inotify_init1 (IN_NONBLOCK);
            if (fd < 0)
                return fd;
        }

        int id = inotify_add_watch (fd, pathname.c_str(), events);
        if (id < 0)
            return -1;

        auto entry = watchers.emplace (std::make_pair(id, pathname));
        auto* buf = pool.get ();
        if (!buf || read(buf, pool.buf_size(), [this, callback](io_result_t& ior)->bool{
                                                   notified (ior, callback);
                                                   return true;
                                               }))
        {
            int errnum = errno;
            inotify_rm_watch (fd, id);
            watchers.erase (entry.first);
            if (buf)
                pool.put (buf);
            else
                errnum = ENOMEM;
            errno = errnum;
            return -1;
        }
        return 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void FileNotifier::remove_watch (const std::string& pathname)
    {
        std::lock_guard<std::mutex> lock (watch_mutex);
        for (auto i=watchers.begin(); i!=watchers.end(); ++i) {
            if (i->second == pathname) {
                inotify_rm_watch (fd, i->first);
                watchers.erase (i);
                break;
            }
        }
        if (watchers.empty())
            close ();
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void FileNotifier::cancel (bool cancel_rx, bool cancel_tx)
    {
        std::lock_guard<std::mutex> lock (watch_mutex);
        watchers.clear ();
        ::close (fd);
        fd = -1;
    }


    //--------------------------------------------------------------------------
    // Called in IOHandler context
    //--------------------------------------------------------------------------
    void FileNotifier::notified (io_result_t& ior, watch_cb callback)
    {
        if (callback  &&  ior.result >= (ssize_t)sizeof(inotify_event)) {
            std::lock_guard<std::mutex> lock (watch_mutex);
            inotify_event& ie = *(reinterpret_cast<inotify_event*>(ior.buf));
            auto entry = watchers.find (ie.wd);
            if (entry != watchers.end()) {
                auto pathname = entry->second;
                watch_mutex.unlock ();
                callback (*this, pathname, ie.mask, ie.cookie, std::string(ie.name));
                watch_mutex.lock ();
            }else{
                inotify_rm_watch (fd, ie.wd);
            }
        }
        if (fd >= 0)
            read (ior.buf, pool.buf_size(), [this, callback](io_result_t& ior)->bool{
                                                notified (ior, callback);
                                                return true;
                                            });
    }


}
