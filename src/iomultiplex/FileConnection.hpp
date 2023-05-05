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
#ifndef IOMULTIPLEX_FILECONNECTION_HPP
#define IOMULTIPLEX_FILECONNECTION_HPP

#include <iomultiplex/FdConnection.hpp>
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
    class FileConnection : public FdConnection {
    public:

        /**
         * Constructor.
         * @param io_handler This object will manage I/O operations.
         */
        FileConnection (iohandler_base& io_handler);

        /**
         * Move Constructor.
         * @param fc The FileConnection object to move.
         */
        FileConnection (FileConnection&& fc);

        /**
         * Constructor.
         * Opens a file.
         * @param io_handler This object will manage I/O operations.
         * @param filename The name of the file.
         * @param flags These are the same flags as in the <code>open ()</code> system call.
         */
        FileConnection (iohandler_base& io_handler, const std::string& filename, int flags);

        /**
         * Constructor.
         * Opens a file.
         * @param io_handler This object will manage I/O operations.
         * @param filename The name of the file.
         * @param flags These are the same flags as in the <code>open ()</code> system call.
         * @param mode These are the same mode bits as in the <code>open ()</code> system call.
         */
        FileConnection (iohandler_base& io_handler, const std::string& filename, int flags, int mode);

        /**
         * Destructor.
         * Closes the file.
         */
        virtual ~FileConnection () = default;

        /**
         * Move operator.
         * @param fc The FileConnection object to move.
         * @return A reference to this object.
         */
        FileConnection& operator= (FileConnection&& fc);

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
