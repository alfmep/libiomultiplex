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
#ifndef IOMULTIPLEX_SERIALCONNECTION_HPP
#define IOMULTIPLEX_SERIALCONNECTION_HPP

#include <iomultiplex/FdConnection.hpp>
#include <iomultiplex/termios_cfg.hpp>
#include <memory>
#include <atomic>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


namespace iomultiplex {

    // Forward declaration
    class iohandler_base;


    /**
     * Serial I/O connection.
     * Sending and receiving using a serial device.
     */
    class SerialConnection : public FdConnection {
    public:
        /**
         * Constructor.
         * @param io_handler This object will manage I/O operations for this connection.
         */
        SerialConnection (iohandler_base& io_handler);

        /**
         * Constructor.
         * @param io_handler This object will manage I/O operations for this connection.
         * @param file_descriptor An open file descriptor to a serial device.
         */
        SerialConnection (iohandler_base& io_handler, int file_descriptor);

        /**
         * Constructor.
         * @param io_handler This object will manage I/O operations for this connection.
         * @param device_filename The filename of the serial device to open.
         * @param baud_rate Baud rate.
         * @param data_bits Number of data bits.
         * @param parity If parity should be used.
         * @param stop_bits Number of stop bits.
         */
        SerialConnection (iohandler_base& io_handler,
                          const std::string& device_filename,
                          int baud_rate=115200,
                          int data_bits=8,
                          parity_t parity=no_parity,
                          int stop_bits=1);

        /**
         * Move Constructor.
         * Don't move SerialConnection objects that has queued read/write
         * operations since the iohandler_base object stores pointers to
         * SerialConnection objects.<br/>
         * All read/write operations that has been queued
         * by the object to be moved will be cancelled.
         * @param sc The SerialConnection object to move.
         */
        SerialConnection (SerialConnection&& sc);

        /**
         * Destructor.
         * All pending I/O operations will be cancelled and the connection will be closed.
         */
        virtual ~SerialConnection () = default;

        /**
         * Move operator.
         * Don't move SerialConnection objects that has queued read/write
         * operations since the iohandler_base object stores pointers to
         * SerialConnection objects.<br/>
         * All read/write operations that has been queued by either
         * this object or the object to be moved will be cancelled.
         * @param sc The SerialConnection object to move.
         * @return A reference to this object.
         */
        SerialConnection& operator= (SerialConnection&& sc);

        /**
         * Open a serial device.
         * @param device_filename The filename of the serial device to open.
         * @param baud_rate Baud rate.
         * @param data_bits Number of data bits.
         * @param parity If parity should be used.
         * @param stop_bits Number of stop bits.
         * @return 0 on success, -1 on failure and <code>errno</code> is set.
         */
        int open (const std::string& device_filename,
                  int baud_rate=115200,
                  int data_bits=8,
                  parity_t parity=no_parity,
                  int stop_bits=1);

        /**
         * Get the terminal attributes of the serial device.
         * @param cfg A poiter to a termios_cfg structure to
         *            store the terminal attributes.
         * @return 0 on success, -1 on failure and <code>errno</code> is set.
         */
        int get_cfg (termios_cfg& cfg);

        /**
         * Set the terminal attributes of the serial device.
         * @param cfg A poiter to a termios_cfg structure from
         *            where to get the terminal attributes.
         * @return 0 on success, -1 on failure and <code>errno</code> is set.
         */
        int set_cfg (const termios_cfg& cfg);


    private:
        SerialConnection () = delete;
        SerialConnection (const SerialConnection& fc) = delete;
        SerialConnection& operator= (const SerialConnection& fc) = delete;

        std::string name;
    };


}
#endif
