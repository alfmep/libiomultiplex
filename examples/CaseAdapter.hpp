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
#ifndef IOMULTIPLEX_EXAMPLES_CASEADAPTER_HPP
#define IOMULTIPLEX_EXAMPLES_CASEADAPTER_HPP

#include <iomultiplex/Adapter.hpp>
#include <iomultiplex/Connection.hpp>
#include <memory>


namespace adapter_test {


    /**
     * An I/O adapter that changes the case of each read/written character.
     */
    class CaseAdapter : public iomultiplex::Adapter {
    public:
        static constexpr const int randomcase = 0; // Randomize character case
        static constexpr const int uppercase  = 1; // Set upper case
        static constexpr const int lowercase  = 2; // Set lower case

        CaseAdapter (iomultiplex::Connection& conn,
                     bool close_on_destruct=false,
                     int new_case=randomcase);
        CaseAdapter (std::shared_ptr<iomultiplex::Connection> conn_ptr,
                     int new_case=randomcase);
        virtual ~CaseAdapter () = default;

        virtual ssize_t do_read (void* buf, size_t size, int& errnum);
        virtual ssize_t do_write (const void* buf, size_t size, int& errnum);

    private:
        int mode;
        std::unique_ptr<char> wbuf;
        size_t wbuf_size;
    };


}


#endif
