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
#include <iomultiplex/SerialConnection.hpp>
#include <cstring>
#include <cerrno>


namespace iomultiplex {

    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static speed_t baud_to_speed (int rate)
    {
        switch (rate) {
        case 50:
            return B50;
        case 75:
            return B75;
        case 110:
            return B110;
        case 134:
            return B134;
        case 150:
            return B150;
        case 200:
            return B200;
        case 300:
            return B300;
        case 600:
            return B600;
        case 1200:
            return B1200;
        case 1800:
            return B1800;
        case 2400:
            return B2400;
        case 4800:
            return B4800;
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
        case 230400:
            return B230400;
        default:
            return 0;
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    static int speed_to_baud (int speed)
    {
        switch (speed) {
        case B50:
            return 50;
        case B75:
            return 75;
        case B110:
            return 110;
        case B134:
            return 134;
        case B150:
            return 150;
        case B200:
            return 200;
        case B300:
            return 300;
        case B600:
            return 600;
        case B1200:
            return 1200;
        case B1800:
            return 1800;
        case B2400:
            return 2400;
        case B4800:
            return 4800;
        case B9600:
            return 9600;
        case B19200:
            return 19200;
        case B38400:
            return 38400;
        case B57600:
            return 57600;
        case B115200:
            return 115200;
        case B230400:
            return 230400;
        default:
            return 0;
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    termios_cfg::termios_cfg ()
    {
        memset (this, 0, sizeof(struct termios));
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void termios_cfg::set_raw ()
    {
        cfmakeraw (this);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int termios_cfg::ispeed ()
    {
        return speed_to_baud (cfgetispeed(this));
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int termios_cfg::ispeed (int baud)
    {
        return cfsetispeed (this, baud_to_speed(baud));
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int termios_cfg::ospeed ()
    {
        return speed_to_baud (cfgetospeed(this));
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int termios_cfg::ospeed (int baud)
    {
        return cfsetospeed (this, baud_to_speed(baud));
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int termios_cfg::speed ()
    {
        return speed_to_baud (cfgetospeed(this));
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int termios_cfg::speed (int baud)
    {
        return cfsetspeed (this, baud_to_speed(baud));
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int termios_cfg::data_bits ()
    {
        if (c_cflag & CS8)
            return 8;
        else if (c_cflag & CS7)
            return 7;
        else if (c_cflag & CS6)
            return 6;
        else if (c_cflag & CS5)
            return 5;
        else
            return 0;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int termios_cfg::data_bits (int num_bits)
    {
        int retval = 0;

        c_cflag &= ~CSIZE;
        switch (num_bits) {
        case 5:
            c_cflag |= CS5;
            break;
        case 6:
            c_cflag |= CS6;
            break;
        case 7:
            c_cflag |= CS7;
            break;
        case 8:
            c_cflag |= CS8;
            break;
        default:
            errno = EINVAL;
            retval = -1;
        }
        return retval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int termios_cfg::stop_bits ()
    {
        return (c_cflag & CSTOPB) ? 2 : 1;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int termios_cfg::stop_bits (int num_bits)
    {
        int retval = 0;
        switch (num_bits) {
        case 1:
            c_cflag &= ~CSTOPB;
            break;
        case 2:
            c_cflag |= CSTOPB;
            break;
        default:
            errno = EINVAL;
            retval = -1;
        }
        return retval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    parity_t termios_cfg::parity ()
    {
        if (c_cflag & PARENB) {
            bool mspar = c_cflag & CMSPAR;
            bool odd   = c_cflag & PARODD;
            if (mspar)
                return odd ? odd_parity : even_parity;
            else
                return odd ? mark_parity : space_parity;
        }
        return no_parity;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    int termios_cfg::parity (parity_t p)
    {
        int retval = 0;

        switch (p) {
        case no_parity:
            c_cflag &= ~(PARENB|CMSPAR);
            break;
        case even_parity:
            c_cflag |= PARENB;
            c_cflag &= ~(CMSPAR|PARODD);
            break;
        case odd_parity:
            c_cflag |= (PARENB|PARODD);
            c_cflag &= ~CMSPAR;
            break;
        case space_parity:
            c_cflag |= (PARENB|CMSPAR);
            c_cflag &= ~PARODD;
            break;
        case mark_parity:
            c_cflag |= (PARENB|CMSPAR|PARODD);
            break;
        default:
            errno = EINVAL;
            retval = -1;
        }
        return retval;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool termios_cfg::echo ()
    {
        return (bool)(c_lflag & ECHO);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void termios_cfg::echo (bool e)
    {
        if (e)
            c_lflag |= ECHO;
        else
            c_lflag &= ~ECHO;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool termios_cfg::xonxoff ()
    {
        return (bool)(c_iflag & IXON);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void termios_cfg::xonxoff (bool on)
    {
        if (on)
            c_iflag |= IXON|IXOFF;
        else
            c_iflag &= ~(IXON|IXOFF);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool termios_cfg::rtscts ()
    {
        return (bool)(c_cflag & CRTSCTS);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void termios_cfg::rtscts (bool on)
    {
        if (on)
            c_cflag |= CRTSCTS;
        else
            c_cflag &= ~CRTSCTS;
    }


}
