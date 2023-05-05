#
# Copyright (C) 2021-2023 Dan Arrhenius <dan@ultramarin.se>
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

AM_CPPFLAGS = -I$(srcdir)/../src -I../src -D_GNU_SOURCE -DSYSCONFDIR='"${sysconfdir}"' -DLOCALSTATEDIR='"${localstatedir}"' -DRUNSTATEDIR='"${runstatedir}"'
AM_CXXFLAGS = -Wall -pipe -O2 -g -pthread -Wno-psabi
AM_LDFLAGS = -pthread -lresolv

LDADD = -L../src -liomultiplex

if HAVE_OPENSSL
AM_CXXFLAGS += $(openssl_CFLAGS)
AM_LDFLAGS += $(openssl_LIBS)
endif


noinst_bindir =
noinst_bin_PROGRAMS =

noinst_bin_PROGRAMS += sync-file-read
sync_file_read_SOURCES = sync-file-read.cpp

noinst_bin_PROGRAMS += async-file-read
async_file_read_SOURCES = async-file-read.cpp

noinst_bin_PROGRAMS += tls-connect
tls_connect_SOURCES = tls-connect.cpp

noinst_bin_PROGRAMS += echo-tcp-server
echo_tcp_server_SOURCES = echo-tcp-server.cpp

noinst_bin_PROGRAMS += echo-tcp-client
echo_tcp_client_SOURCES = echo-tcp-client.cpp

noinst_bin_PROGRAMS += echo-tls-server
echo_tls_server_SOURCES = echo-tls-server.cpp

noinst_bin_PROGRAMS += echo-tls-client
echo_tls_client_SOURCES = echo-tls-client.cpp

noinst_bin_PROGRAMS += echo-udp-server
echo_udp_server_SOURCES = echo-udp-server.cpp

noinst_bin_PROGRAMS += echo-udp-client
echo_udp_client_SOURCES = echo-udp-client.cpp

noinst_bin_PROGRAMS += echo-dtls-server
echo_dtls_server_SOURCES = echo-dtls-server.cpp

noinst_bin_PROGRAMS += echo-dtls-client
echo_dtls_client_SOURCES = echo-dtls-client.cpp

noinst_bin_PROGRAMS += echo-server
echo_server_SOURCES  = echo-server.cpp

noinst_bin_PROGRAMS += echo-client
echo_client_SOURCES  = echo-client.cpp

noinst_bin_PROGRAMS  += beacon-server
beacon_server_SOURCES = beacon-server.cpp

noinst_bin_PROGRAMS  += beacon-client
beacon_client_SOURCES = beacon-client.cpp

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

#
# Example of a TFTP server and client
#
IOM_TFTP_COMMON_SOURCES  = iom-tftp/tftp.hpp
IOM_TFTP_COMMON_SOURCES += iom-tftp/tftp.cpp
IOM_TFTP_COMMON_SOURCES += iom-tftp/tftp-session.hpp
IOM_TFTP_COMMON_SOURCES += iom-tftp/tftp-session.cpp
# TFTP server
noinst_bin_PROGRAMS += iom-tftp/iom-tftpd
iom_tftp_iom_tftpd_SOURCES  = $(IOM_TFTP_COMMON_SOURCES)
iom_tftp_iom_tftpd_SOURCES += iom-tftp/iom-tftpd-options.hpp
iom_tftp_iom_tftpd_SOURCES += iom-tftp/iom-tftpd-options.cpp
iom_tftp_iom_tftpd_SOURCES += iom-tftp/iom-tftpd.cpp
# TFTP client
noinst_bin_PROGRAMS += iom-tftp/iom-tftp
iom_tftp_iom_tftp_SOURCES  = $(IOM_TFTP_COMMON_SOURCES)
iom_tftp_iom_tftp_SOURCES += iom-tftp/iom-tftp-options.hpp
iom_tftp_iom_tftp_SOURCES += iom-tftp/iom-tftp-options.cpp
iom_tftp_iom_tftp_SOURCES += iom-tftp/iom-tftp.cpp


endif
