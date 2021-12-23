#
# Copyright (C) 2021 Dan Arrhenius <dan@ultramarin.se>
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

if ENABLE_EXAMPLES_SET

AM_CPPFLAGS = -I$(srcdir)/../src -I../src -DSYSCONFDIR='"${sysconfdir}"' -DLOCALSTATEDIR='"${localstatedir}"'
AM_CXXFLAGS = -Wall -pipe -O2 -g -pthread
AM_LDFLAGS = -pthread -lresolv

LDADD = -L../src -liomultiplex


noinst_bindir =
noinst_bin_PROGRAMS =

noinst_bin_PROGRAMS += echo-server
echo_server_SOURCES  = echo-server.cpp

noinst_bin_PROGRAMS += echo-client
echo_client_SOURCES  = echo-client.cpp

noinst_bin_PROGRAMS  += beacon-server
beacon_server_SOURCES = beacon-server.cpp

noinst_bin_PROGRAMS  += adapter-test
adapter_test_SOURCES  = adapter-test.cpp
adapter_test_SOURCES += ObfuscateAdapter.cpp
adapter_test_SOURCES += ObfuscateAdapter.hpp
adapter_test_SOURCES += ShuffleAdapter.cpp
adapter_test_SOURCES += ShuffleAdapter.hpp
adapter_test_SOURCES += CaseAdapter.cpp
adapter_test_SOURCES += CaseAdapter.hpp
adapter_test_SOURCES += RobberAdapter.cpp
adapter_test_SOURCES += RobberAdapter.hpp
adapter_test_SOURCES += ReverseAdapter.cpp
adapter_test_SOURCES += ReverseAdapter.hpp

noinst_bin_PROGRAMS += utftpd
utftpd_SOURCES =  utftpd.cpp
utftpd_SOURCES += utftpd.hpp
utftpd_SOURCES += utftp-common.hpp
utftpd_SOURCES += utftpd-options.cpp
utftpd_SOURCES += utftpd-options.hpp
utftpd_SOURCES += logging.cpp
utftpd_SOURCES += logging.hpp


endif
