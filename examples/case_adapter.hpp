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
#ifndef IOMULTIPLEX_EXAMPLES_CASE_ADAPTER_HPP
#define IOMULTIPLEX_EXAMPLES_CASE_ADAPTER_HPP

#include <iomultiplex/adapter.hpp>
#include <iomultiplex/connection.hpp>
#include <memory>


namespace adapter_test {


    /**
     * An I/O adapter that changes the case of each read/written character.
     */
    class case_adapter : public iomultiplex::adapter {
    public:
        static constexpr const int randomcase = 0; // Randomize character case
        static constexpr const int uppercase  = 1; // Set upper case
        static constexpr const int lowercase  = 2; // Set lower case

        case_adapter (iomultiplex::connection& conn,
                      bool close_on_destruct=false,
                      int new_case=randomcase);
        case_adapter (std::shared_ptr<iomultiplex::connection> conn_ptr,
                      int new_case=randomcase);
        virtual ~case_adapter () = default;

        virtual ssize_t do_read (void* buf, size_t size, int& errnum);
        virtual ssize_t do_write (const void* buf, size_t size, int& errnum);

    private:
        int mode;
        std::unique_ptr<char> wbuf;
        size_t wbuf_size;
    };


}


#endif
