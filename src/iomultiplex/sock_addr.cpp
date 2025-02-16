/*
 * Copyright (C) 2021,2025 Dan Arrhenius <dan@ultramarin.se>
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
#include <iomultiplex/sock_addr.hpp>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>


namespace iomultiplex {


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    sock_addr::sock_addr ()
        : sa {0}
    {
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool sock_addr::operator== (const sock_addr& rhs) const
    {
        return (this == &rhs ||
                (size()==rhs.size() && memcmp(data(), rhs.data(), size())==0));
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool sock_addr::operator< (const sock_addr& rhs) const
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
    const struct sockaddr* sock_addr::data () const
    {
        return (struct sockaddr*) &sa;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    sa_family_t sock_addr::family () const
    {
        return ((struct sockaddr&)sa).sa_family;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void sock_addr::clear ()
    {
        char* ptr = (char*) &sa;
        memset (ptr+sizeof(((struct sockaddr&)sa).sa_family), 0,
                sizeof(sa)-sizeof(((struct sockaddr&)sa).sa_family));
    }


}
