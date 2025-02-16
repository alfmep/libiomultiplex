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
#ifndef IOMULTIPLEX_FILE_CONNECTION_HPP
#define IOMULTIPLEX_FILE_CONNECTION_HPP

#include <iomultiplex/fd_connection.hpp>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


namespace iomultiplex {

    // Forward declaration
    class iohandler_base;

    /**
     * File I/O connection.
     * This class is used for reading/writing files.
     */
    class file_connection : public fd_connection {
    public:

        /**
         * Constructor.
         * @param io_handler This object will manage I/O operations.
         */
        file_connection (iohandler_base& io_handler);

        /**
         * Move Constructor.
         * @param fc The file_connection object to move.
         */
        file_connection (file_connection&& fc);

        /**
         * Constructor.
         * Opens a file.
         * @param io_handler This object will manage I/O operations.
         * @param filename The name of the file.
         * @param flags These are the same flags as in the <code>open ()</code> system call.
         */
        file_connection (iohandler_base& io_handler, const std::string& filename, int flags);

        /**
         * Constructor.
         * Opens a file.
         * @param io_handler This object will manage I/O operations.
         * @param filename The name of the file.
         * @param flags These are the same flags as in the <code>open ()</code> system call.
         * @param mode These are the same mode bits as in the <code>open ()</code> system call.
         */
        file_connection (iohandler_base& io_handler, const std::string& filename, int flags, int mode);

        /**
         * Destructor.
         * Closes the file.
         */
        virtual ~file_connection () = default;

        /**
         * Move operator.
         * @param fc The file_connection object to move.
         * @return A reference to this object.
         */
        file_connection& operator= (file_connection&& fc);

        /**
         * Open a file.
         * @param filename The name of the file.
         * @param flags These are the same flags as in the <code>open ()</code> system call.
         * @return 0 on success, -1 on failure and <code>errno</code> is set.
         */
        int open (const std::string& filename, int flags);

        /**
         * Open a file.
         * @param filename The name of the file.
         * @param flags These are the same flags as in the <code>open ()</code> system call.
         * @param mode These are the same mode bits as in the <code>open ()</code> system call.
         * @return 0 on success, -1 on failure and <code>errno</code> is set.
         */
        int open (const std::string& filename, int flags, int mode);

        /**
         * Return the name of the file.
         * @return A filename.
         */
        const std::string& filename () const;


    private:
        std::string name;
    };


}
#endif
