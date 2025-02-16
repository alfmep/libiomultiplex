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
#ifndef IOMULTIPLEX_EXAMPLES_ROBBER_ADAPTER_HPP
#define IOMULTIPLEX_EXAMPLES_ROBBER_ADAPTER_HPP

#include <iomultiplex/adapter.hpp>
#include <iomultiplex/connection.hpp>
#include <memory>


namespace adapter_test {


    /**
     * The Robber Language.
     * Every consonant character is doubled, and an o is inserted in-between.
     *
     * @see <a href=https://en.wikipedia.org/wiki/R%C3%B6varspr%C3%A5ket
     *       rel="noopener noreferrer" target="_blank">Rövarspråket</a>
     */
    class robber_adapter : public iomultiplex::adapter {
    public:
        robber_adapter (iomultiplex::connection& conn, bool close_on_destruct=false);
        robber_adapter (std::shared_ptr<iomultiplex::connection> conn_ptr);
        virtual ~robber_adapter () = default;

        virtual ssize_t do_write (const void* buf, size_t size, int& errnum);

    private:
        std::unique_ptr<char> wbuf;
        size_t wbuf_size;
    };


}


#endif
