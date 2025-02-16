#
# Copyright (C) 2021,2022,2024,2025 Dan Arrhenius <dan@ultramarin.se>
#
# This file is part of libiomultiplex
#
# libiomultiplex is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published
# by the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
lib_LTLIBRARIES = libiomultiplex.la

AM_CPPFLAGS = -I$(srcdir) -D_GNU_SOURCE -DSYSCONFDIR='"${sysconfdir}"' -DLOCALSTATEDIR='"${localstatedir}"'
AM_LDFLAGS = -lresolv
AM_CXXFLAGS = -Wall
AM_LDFLAGS += -version-info @LIBRARY_VERSION@

if HAVE_OPENSSL
AM_CXXFLAGS += $(openssl_CFLAGS)
AM_LDFLAGS += $(openssl_LIBS)
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = iomultiplex.pc

# Generated source and header files
BUILT_SOURCES =

#CLEANFILES = $(BUILT_SOURCES)


# Source files
libiomultiplex_la_SOURCES  =
libiomultiplex_la_SOURCES += iomultiplex/utils.cpp
libiomultiplex_la_SOURCES += iomultiplex/log.cpp
libiomultiplex_la_SOURCES += iomultiplex/sock_addr.cpp
libiomultiplex_la_SOURCES += iomultiplex/ip_addr.cpp
libiomultiplex_la_SOURCES += iomultiplex/ux_addr.cpp
libiomultiplex_la_SOURCES += iomultiplex/buffer_pool.cpp
libiomultiplex_la_SOURCES += iomultiplex/resolver.cpp
libiomultiplex_la_SOURCES += iomultiplex/poll_descriptors.cpp
libiomultiplex_la_SOURCES += iomultiplex/connection.cpp
libiomultiplex_la_SOURCES += iomultiplex/iohandler_poll.cpp
libiomultiplex_la_SOURCES += iomultiplex/iohandler_epoll.cpp
libiomultiplex_la_SOURCES += iomultiplex/fd_connection.cpp
libiomultiplex_la_SOURCES += iomultiplex/file_connection.cpp
libiomultiplex_la_SOURCES += iomultiplex/file_notifier.cpp
libiomultiplex_la_SOURCES += iomultiplex/termios_cfg.cpp
libiomultiplex_la_SOURCES += iomultiplex/serial_connection.cpp
libiomultiplex_la_SOURCES += iomultiplex/timer_connection.cpp
libiomultiplex_la_SOURCES += iomultiplex/socket_connection.cpp
libiomultiplex_la_SOURCES += iomultiplex/timer_set.cpp
libiomultiplex_la_SOURCES += iomultiplex/adapter.cpp
libiomultiplex_la_SOURCES += iomultiplex/chunk_adapter.cpp
if HAVE_OPENSSL
libiomultiplex_la_SOURCES += iomultiplex/x509_t.cpp
libiomultiplex_la_SOURCES += iomultiplex/tls_adapter.cpp
endif

# Header files
libiomultiplexdir = $(includedir)
nobase_libiomultiplex_HEADERS =
nobase_libiomultiplex_HEADERS += iomultiplex.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/types.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/utils.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/log.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/sock_addr.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/ip_addr.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/ux_addr.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/buffer_pool.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/resolver.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/io_result_t.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/poll_descriptors.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/connection.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/iohandler_base.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/iohandler_poll.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/iohandler_epoll.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/fd_connection.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/file_connection.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/file_notifier.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/termios_cfg.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/serial_connection.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/timer_connection.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/socket_connection.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/timer_set.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/adapter.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/chunk_adapter.hpp
if HAVE_OPENSSL
nobase_libiomultiplex_HEADERS += iomultiplex/x509_t.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/tls_config.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/tls_adapter.hpp
endif

# Header files that is not to be installed
noinst_HEADERS =
