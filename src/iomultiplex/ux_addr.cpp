/*
 * Copyright (C) 2021,2023,2025 Dan Arrhenius <dan@ultramarin.se>
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
#include <iomultiplex/ux_addr.hpp>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>


namespace iomultiplex {


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ux_addr::ux_addr ()
    {
        memset (&sa, 0, sizeof(sa));
        ((struct sockaddr_un&)sa).sun_family = AF_UNIX;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ux_addr::ux_addr (const std::string& path, const bool abstract)
    {
        memset (&sa, 0, sizeof(sa));
        ((struct sockaddr_un&)sa).sun_family = AF_UNIX;
        if (abstract) {
            ((struct sockaddr_un&)sa).sun_path[0] = '\0';
            strncpy (((struct sockaddr_un&)sa).sun_path+1,
                     path.c_str(),
                     sizeof(((struct sockaddr_un&)sa).sun_path)-1);
        }else{
            strncpy (((struct sockaddr_un&)sa).sun_path,
                     path.c_str(),
                     sizeof(((struct sockaddr_un&)sa).sun_path));
        }
        ((struct sockaddr_un&)sa).sun_path[sizeof(((struct sockaddr_un&)sa).sun_path)-1] = '\0';
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ux_addr::ux_addr (const ux_addr& addr)
    {
        memcpy (&sa, &addr.sa, sizeof(sa));
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    /*
    ux_addr::ux_addr (const struct sockaddr& addr, socklen_t size)
    {
        memcpy (&sa, &addr, size);
    }
    */


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ux_addr::ux_addr (const struct sockaddr_un& addr)
    {
        memcpy (&sa, &addr, sizeof(struct sockaddr_un));
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ux_addr& ux_addr::operator= (const ux_addr& addr)
    {
        if (this != &addr)
            memcpy (&sa, &addr.sa, sizeof(sa));
        return *this;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    ux_addr& ux_addr::operator= (const struct sockaddr_un& addr)
    {
        memset (&sa, 0, sizeof(sa));
        memcpy (&sa, &addr, sizeof(struct sockaddr_un));
        return *this;
    }


    /*
    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool ux_addr::operator== (const ux_addr& addr) const
    {
        return memcmp(&sa, &addr.sa, size()) == 0;
    }
    */


    /*
    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool ux_addr::operator< (const ux_addr& rhs) const
    {
        const char* lhs_addr = (const char*) &sa.sa;
        const char* rhs_addr = (const char*) &rhs.sa.sa;
        for (unsigned i=0; i<sizeof(sa.sa); ++i) {
            if (lhs_addr[i] < rhs_addr[i])
                return true;
            else if (lhs_addr[i] != rhs_addr[i])
                return false;
        }
        return false;
    }
    */


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    size_t ux_addr::size () const
    {
        return sizeof (struct sockaddr_un);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    const std::string ux_addr::path () const
    {
        if (is_abstract())
            return std::string (((struct sockaddr_un&)sa).sun_path+1);
        else
            return std::string (((struct sockaddr_un&)sa).sun_path);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void ux_addr::path (const std::string& path, const bool abstract)
    {
        memset (((struct sockaddr_un&)sa).sun_path, 0, sizeof(((struct sockaddr_un&)sa).sun_path));
        if (abstract) {
            // In an abstract address path, the first byte is '\0'.
            //((struct sockaddr_un&)sa).sun_path[0] = '\0'; // Already '\0' from memset()
            strncpy (((struct sockaddr_un&)sa).sun_path+1, path.c_str(), sizeof(((struct sockaddr_un&)sa).sun_path)-2);
        }else{
            strncpy (((struct sockaddr_un&)sa).sun_path, path.c_str(), sizeof(((struct sockaddr_un&)sa).sun_path)-1);
        }
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool ux_addr::is_abstract () const
    {
        return ((struct sockaddr_un&)sa).sun_path[0] == '\0';
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::shared_ptr<sock_addr> ux_addr::clone () const
    {
        return std::make_shared<ux_addr> (*this);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::string ux_addr::to_string () const
    {
        const char* path = ((struct sockaddr_un&)sa).sun_path;
        if (is_abstract() && path[1]!='\0')
            return std::string("[") + std::string(path+1) + std::string("]");
        else
            return std::string (path);
    }


}
