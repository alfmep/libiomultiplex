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
#include <iomultiplex/SockAddr.hpp>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>


namespace iomultiplex {


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    SockAddr::SockAddr ()
        : sa {0}
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool SockAddr::operator== (const SockAddr& rhs) const
    {
        return (this == &rhs ||
                (size()==rhs.size() && memcmp(data(), rhs.data(), size())==0));
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool SockAddr::operator< (const SockAddr& rhs) const
    {
        if (this != &rhs  &&  family() == rhs.family()) {
            const char* lhs_addr = (const char*) data ();
            const char* rhs_addr = (const char*) rhs.data ();
            for (unsigned i=0; i<size(); ++i) {
                if (lhs_addr[i] < rhs_addr[i])
                    return true;
                else if (lhs_addr[i] != rhs_addr[i])
                    return false;
            }
        }
        return false;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    const struct sockaddr* SockAddr::data () const
    {
        return (struct sockaddr*) &sa;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    sa_family_t SockAddr::family () const
    {
        return ((struct sockaddr&)sa).sa_family;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void SockAddr::clear ()
    {
        char* ptr = (char*) &sa;
        memset (ptr+sizeof(((struct sockaddr&)sa).sa_family), 0,
                sizeof(sa)-sizeof(((struct sockaddr&)sa).sa_family));
    }


}
