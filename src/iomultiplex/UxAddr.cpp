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
#include <iomultiplex/UxAddr.hpp>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>


namespace iomultiplex {


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    UxAddr::UxAddr ()
    {
        memset (&sa, 0, sizeof(sa));
        ((struct sockaddr_un&)sa).sun_family = AF_UNIX;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    UxAddr::UxAddr (const std::string& path, const bool abstract)
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
    UxAddr::UxAddr (const UxAddr& addr)
    {
        memcpy (&sa, &addr.sa, sizeof(sa));
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    /*
    UxAddr::UxAddr (const struct sockaddr& addr, socklen_t size)
    {
        memcpy (&sa, &addr, size);
    }
    */


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    UxAddr::UxAddr (const struct sockaddr_un& addr)
    {
        memcpy (&sa, &addr, sizeof(struct sockaddr_un));
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    UxAddr& UxAddr::operator= (const UxAddr& addr)
    {
        if (this != &addr)
            memcpy (&sa, &addr.sa, sizeof(sa));
        return *this;
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    UxAddr& UxAddr::operator= (const struct sockaddr_un& addr)
    {
        memset (&sa, 0, sizeof(sa));
        memcpy (&sa, &addr, sizeof(struct sockaddr_un));
        return *this;
    }


    /*
    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool UxAddr::operator== (const UxAddr& addr) const
    {
        return memcmp(&sa, &addr.sa, size()) == 0;
    }
    */


    /*
    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool UxAddr::operator< (const UxAddr& rhs) const
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
    size_t UxAddr::size () const
    {
        return sizeof (struct sockaddr_un);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    const std::string UxAddr::path () const
    {
        if (is_abstract())
            return std::string (((struct sockaddr_un&)sa).sun_path+1);
        else
            return std::string (((struct sockaddr_un&)sa).sun_path);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    void UxAddr::path (const std::string& path, const bool abstract)
    {
        memset (((struct sockaddr_un&)sa).sun_path, 0, sizeof(((struct sockaddr_un&)sa).sun_path));
        if (abstract) {
            ((struct sockaddr_un&)sa).sun_path[0] = '\0';
            strncpy (((struct sockaddr_un&)sa).sun_path+1, path.c_str(), sizeof(((struct sockaddr_un&)sa).sun_path)-1);
        }else{
            strncpy (((struct sockaddr_un&)sa).sun_path, path.c_str(), sizeof(((struct sockaddr_un&)sa).sun_path));
        }
        ((struct sockaddr_un&)sa).sun_path[sizeof(((struct sockaddr_un&)sa).sun_path)-1] = '\0';
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    bool UxAddr::is_abstract () const
    {
        return ((struct sockaddr_un&)sa).sun_path[0] == '\0';
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::shared_ptr<SockAddr> UxAddr::clone () const
    {
        return std::make_shared<UxAddr> (*this);
    }


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::string UxAddr::to_string () const
    {
        const char* path = ((struct sockaddr_un&)sa).sun_path;
        if (is_abstract() && path[1]!='\0')
            return std::string("[") + std::string(path+1) + std::string("]");
        else
            return std::string (path);
    }


}
