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
#ifndef IOMULTIPLEX_UTILS_HPP
#define IOMULTIPLEX_UTILS_HPP

#include <iomultiplex/iohandler_base.hpp>
#include <iomultiplex/connection.hpp>
#include <iomultiplex/fd_connection.hpp>
#include <utility>


namespace iomultiplex {


    /**
     *
     */
    std::pair<fd_connection, fd_connection> make_pipe (iohandler_base& ioh, int flags=0);

}
#endif
