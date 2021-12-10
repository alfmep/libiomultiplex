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
#include <iomultiplex.hpp>
#include <cstring>


namespace iomultiplex {


    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    std::pair<FdConnection, FdConnection> make_pipe (iohandler_base& ioh, int flags)
    {
        int fd[2];
        if (pipe(fd) == 0) {
            int use_flags = O_NONBLOCK | flags;
            if (fcntl(fd[0], F_SETFD, use_flags) ||
                fcntl(fd[1], F_SETFD, use_flags))
            {
                close (fd[0]);
                close (fd[1]);
            }
        }else{
            fd[0] = fd[1] = -1;
        }
        return std::make_pair(FdConnection(ioh, fd[0]), FdConnection(ioh, fd[1]));
    }


}
