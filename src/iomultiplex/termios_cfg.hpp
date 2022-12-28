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
#ifndef IOMULTIPLEX_TERMIOS_CFG_HPP
#define IOMULTIPLEX_TERMIOS_CFG_HPP

#include <termios.h>


namespace iomultiplex {


    /**
     * Serial parity setting.
     */
    enum parity_t : int {
        no_parity = 0,  /**< No parity. */
        even_parity,    /**< Even parity. */
        odd_parity,     /**< Odd parity. */
        space_parity,   /**< Space parity. The parity bit is always set to zero. */
        mark_parity     /**< Mark parity. The parity bit is always set to one. */
    };


    /**
     * Termios configuration.
     */
    class termios_cfg : public termios {
    public:
        /**
         * Default constructor.
         */
        termios_cfg ();

        /**
         * Destructor.
         */
        ~termios_cfg () = default;

        /**
         * Set <i>raw</i> mode.
         */
        void set_raw ();

        /**
         * Get the input baud rate.
         * @return The input baud rate.
         */
        int ispeed ();

        /**
         * Set the input baud rate.
         * @param rate The input baud rate.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        int ispeed (int rate);

        /**
         * Get the output baud rate.
         * @return The output baud rate.
         */
        int ospeed ();

        /**
         * Set the output baud rate.
         * @param rate The output baud rate.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        int ospeed (int rate);

        /**
         * Get the baud rate.
         * @return The baud rate.
         */
        int speed ();

        /**
         * Set the I/O baud rate.
         * @param rate The I/O baud rate.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        int speed (int rate);

        /**
         * Get the number of data bits.
         * @return The number of data bits.
         */
        int data_bits ();

        /**
         * Set the number of data bits.
         * @param num_bits The number of data bits.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        int data_bits (int num_bits);

        /**
         * Get the number of stop bits.
         * @return The number of stop bits.
         */
        int stop_bits ();

        /**
         * Set the number of stop bits.
         * @param num_bits The number of stop bits.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        int stop_bits (int num_bits);

        /**
         * Get parity.
         * @return The parity.
         */
        parity_t parity ();

        /**
         * Set parity.
         * @param p The parity.
         * @return 0 on success. -1 on error and <code>errno</code> is set.
         */
        int parity (parity_t p);

        /**
         * Check if input characters are echoed.
         * @return <code>true</code> If input characters are echoed.
         */
        bool echo ();

        /**
         * Echo or not echo input characters.
         * @param e <code>true</code> to echo input characters.
         */
        void echo (bool e);

        /**
         * Get XON/XOFF flow control on output.
         * @return <code>true</code> If XON/XOFF flow control on output.
         */
        bool xonxoff ();

        /**
         * Set XON/XOFF flow control on output.
         * @param on <code>true</code> for XON/XOFF flow control.
         */
        void xonxoff (bool on);

        /**
         * Get RTS/CTS (hardware) flow control.
         * @return <code>true</code> If RTS/CTS (hardware) flow control.
         */
        bool rtscts ();

        /**
         * Set RTS/CTS (hardware) flow control.
         * @param on <code>true</code> for RTS/CTS flow control.
         */
        void rtscts (bool on);
    };


}
#endif
