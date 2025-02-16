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
#ifndef IOMULTIPLEX_EXAMPLES_OBFUSCATE_ADAPTER_HPP
#define IOMULTIPLEX_EXAMPLES_OBFUSCATE_ADAPTER_HPP

#include <iomultiplex/adapter.hpp>
#include <iomultiplex/connection.hpp>
#include <memory>


namespace adapter_test {


    /**
     * An I/O adapter that obfuscates/de-obfuscates data using the glibc funtion memfrob.
     *
     * @see <a href=https://www.gnu.org/software/libc/manual/html_node/Obfuscating-Data.html
     *       rel="noopener noreferrer" target="_blank">The GNU C Library - 5.13 Obfuscating Data</a>
     */
    class obfuscate_adapter : public iomultiplex::adapter {
    public:
        obfuscate_adapter (iomultiplex::connection& conn, bool close_on_destruct=false);
        obfuscate_adapter (std::shared_ptr<iomultiplex::connection> conn_ptr);
        virtual ~obfuscate_adapter () = default;

        virtual ssize_t do_read (void* buf, size_t size, int& errnum);
        virtual ssize_t do_write (const void* buf, size_t size, int& errnum);

    private:
        std::unique_ptr<char> wbuf;
        size_t wbuf_size;
    };


}


#endif
