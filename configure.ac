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
AC_INIT([libiomultiplex], [0.2.2], [dan@ultramarin.se])
AM_INIT_AUTOMAKE([-Wall -Werror foreign dist-bzip2 subdir-objects])

AC_PROG_CC
AC_PROG_CXX
AC_PROG_MKDIR_P
AC_PROG_INSTALL
AM_PROG_AR

LT_INIT()
LT_LANG([C++])

AC_CONFIG_MACRO_DIR([m4])
AM_SILENT_RULES([yes])

#
# Release 0.2.2
#
# Library interface version x:y:z
#                           | | |
#                           | | +- AGE - how many previous versions of CURRENT this interface is compatible with.
#                           | |
#                           | +--- REVISION - Update this number when no interface change has been made.
#                           |
#                           +----- CURRENT - Update this number when an interface change has been made.
#
# Library version (CURRENT:REVISION:AGE)
#
LIBRARY_VERSION=2:2:2
AC_SUBST([LIBRARY_VERSION])

#
# Libraries to be listed in iomultiplex.pc
#
LIBS_IN_PC_FILE=

#
# Required packages to be listed in iomultiplex.pc
#
REQUIRE_IN_PC_FILE=

#
# Defines in the doxygen config file
#
PREDEFINED_IN_DOXYGEN=

#
# Header files excluded from doxygen
#
EXCLUDE_FROM_DOXYGEN=

#
# Include statement for class TlsAdapter
#
TLS_ADAPTER_HEADER_FILES=


#
# Give the user an option to disable OpenSSL (class TlsAdapter)
#
AC_ARG_ENABLE([openssl],
	[AS_HELP_STRING([--disable-openssl],
	[don't enable support for TLS [default=auto]])])
AM_CONDITIONAL([ENABLE_OPENSSL_SET], [test "x$enable_openssl" != "xno"])

AM_COND_IF([ENABLE_OPENSSL_SET],
	[
		#
		# Check for OpenSSL
		#
		PKG_CHECK_MODULES([openssl],
			[openssl],
			[
				REQUIRE_IN_PC_FILE+=" openssl"
				AM_CONDITIONAL([HAVE_OPENSSL], true)
				TLS_ADAPTER_HEADER_FILES="#include <iomultiplex/x509_t.hpp>
#include <iomultiplex/TlsConfig.hpp>
#include <iomultiplex/TlsAdapter.hpp>"
			],
			[
				AC_MSG_WARN(Could not find openssl - libiomultiplex will not have class TlsAdapter);
				AM_CONDITIONAL([HAVE_OPENSSL], false)
				TLS_ADAPTER_HEADER_FILES=""
				EXCLUDE_FROM_DOXYGEN+=" ../src/iomultiplex/TlsConfig.hpp"
				EXCLUDE_FROM_DOXYGEN+=" ../src/iomultiplex/TlsAdapter.hpp"
			])
	],
	[
		AC_MSG_NOTICE(OpenSSL disabled - libiomultiplex will not have class TlsAdapter)
		AM_CONDITIONAL([HAVE_OPENSSL], false)
		TLS_ADAPTER_HEADER_FILES=""
		EXCLUDE_FROM_DOXYGEN+=" ../src/iomultiplex/TlsConfig.hpp"
		EXCLUDE_FROM_DOXYGEN+=" ../src/iomultiplex/TlsAdapter.hpp"
	]
)
AC_SUBST([openssl_CFLAGS])
AC_SUBST([openssl_LIBS])


#
# Give the user an option to not build example applications
#
AC_ARG_ENABLE([examples],
	[AS_HELP_STRING([--enable-examples], [build example applications])],,
	enable_examples=no)
AM_CONDITIONAL([ENABLE_EXAMPLES_SET], [test "x$enable_examples" != "xno"])



#
# All libraries are added
#
AC_SUBST([LIBS_IN_PC_FILE])
AC_SUBST([REQUIRE_IN_PC_FILE])
AC_SUBST([PREDEFINED_IN_DOXYGEN])
AC_SUBST([EXCLUDE_FROM_DOXYGEN])
AC_SUBST([TLS_ADAPTER_HEADER_FILES])


#
# Give the user an option to disable doxygen documentation
#
AC_ARG_ENABLE([doxygen],
	[AS_HELP_STRING([--disable-doxygen],
	[disable doxygen documentation [default=auto]])],,
	enable_doxygen=yes)
AM_CONDITIONAL([ENABLE_DOXYGEN_SET], [test "x$enable_doxygen" != "xno"])


AM_COND_IF([ENABLE_DOXYGEN_SET],
	[
	# Check for doxygen support
	#
	AC_CHECK_PROGS([DOXYGEN], [doxygen])
	if test -z "$DOXYGEN";
	   then AC_MSG_WARN([doxygen not found - continuing without doxygen support])
	fi
	AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$DOXYGEN"])
	AM_COND_IF([HAVE_DOXYGEN], [AC_CONFIG_FILES([doc/doxygen.cfg])])
	],
	[
	# doxygen disabled
	#
	AM_CONDITIONAL([HAVE_DOXYGEN], false)
	]
)


AC_CONFIG_FILES([
	Makefile
	src/Makefile
	src/iomultiplex.hpp
	src/iomultiplex.pc
	examples/Makefile
	doc/Makefile
])

AC_OUTPUT


#
# Print summary of the confiuration
#
AC_MSG_NOTICE([])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([ Configuration summary:])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([ Installation prefix.................. ${prefix}])
AM_COND_IF([HAVE_OPENSSL],
	[AC_MSG_NOTICE([ OpenSSL enabled...................... yes])],
	[AC_MSG_NOTICE([ OpenSSL enabled...................... no (libiomultiplex will not have class TlsAdapter)])]
)
AM_COND_IF([ENABLE_EXAMPLES_SET],
	[AC_MSG_NOTICE([ Build example applications........... yes (example applications are not installed)])],
	[AC_MSG_NOTICE([ Build example applications........... no])]
)
AM_COND_IF([HAVE_DOXYGEN],
	[AC_MSG_NOTICE([ Generate doxygen documentation....... yes])],
	[AC_MSG_NOTICE([ Generate doxygen documentation....... no])]
)
AC_MSG_NOTICE([])
AC_MSG_NOTICE([])
