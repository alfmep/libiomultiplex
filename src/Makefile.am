#
# Copyright (C) 2021,2022,2024 Dan Arrhenius <dan@ultramarin.se>
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
libiomultiplex_la_SOURCES += iomultiplex/Log.cpp
libiomultiplex_la_SOURCES += iomultiplex/SockAddr.cpp
libiomultiplex_la_SOURCES += iomultiplex/IpAddr.cpp
libiomultiplex_la_SOURCES += iomultiplex/UxAddr.cpp
libiomultiplex_la_SOURCES += iomultiplex/BufferPool.cpp
libiomultiplex_la_SOURCES += iomultiplex/Resolver.cpp
libiomultiplex_la_SOURCES += iomultiplex/PollDescriptors.cpp
libiomultiplex_la_SOURCES += iomultiplex/Connection.cpp
libiomultiplex_la_SOURCES += iomultiplex/IOHandler_Poll.cpp
libiomultiplex_la_SOURCES += iomultiplex/IOHandler_Epoll.cpp
libiomultiplex_la_SOURCES += iomultiplex/FdConnection.cpp
libiomultiplex_la_SOURCES += iomultiplex/FileConnection.cpp
libiomultiplex_la_SOURCES += iomultiplex/FileNotifier.cpp
libiomultiplex_la_SOURCES += iomultiplex/termios_cfg.cpp
libiomultiplex_la_SOURCES += iomultiplex/SerialConnection.cpp
libiomultiplex_la_SOURCES += iomultiplex/TimerConnection.cpp
libiomultiplex_la_SOURCES += iomultiplex/SocketConnection.cpp
libiomultiplex_la_SOURCES += iomultiplex/TimerSet.cpp
libiomultiplex_la_SOURCES += iomultiplex/Adapter.cpp
libiomultiplex_la_SOURCES += iomultiplex/ChunkAdapter.cpp
if HAVE_OPENSSL
libiomultiplex_la_SOURCES += iomultiplex/x509_t.cpp
libiomultiplex_la_SOURCES += iomultiplex/TlsAdapter.cpp
endif

# Header files
libiomultiplexdir = $(includedir)
nobase_libiomultiplex_HEADERS =
nobase_libiomultiplex_HEADERS += iomultiplex.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/types.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/utils.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/Log.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/SockAddr.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/IpAddr.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/UxAddr.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/BufferPool.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/Resolver.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/io_result_t.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/PollDescriptors.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/Connection.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/iohandler_base.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/IOHandler_Poll.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/IOHandler_Epoll.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/FdConnection.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/FileConnection.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/FileNotifier.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/termios_cfg.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/SerialConnection.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/TimerConnection.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/SocketConnection.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/TimerSet.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/Adapter.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/ChunkAdapter.hpp
if HAVE_OPENSSL
nobase_libiomultiplex_HEADERS += iomultiplex/x509_t.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/TlsConfig.hpp
nobase_libiomultiplex_HEADERS += iomultiplex/TlsAdapter.hpp
endif

# Header files that is not to be installed
noinst_HEADERS =
