#
#  Copyright (C) 2003 - 2005 Tomasz Kojm <tkojm@clamav.net>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.

# FIXME: check automake for 'and' (&&)
if BUILD_CLAMD
if HAVE_MILTER

sbin_PROGRAMS = clamav-milter

clamav_milter_SOURCES = \
    $(top_srcdir)/shared/cfgparser.c \
    $(top_srcdir)/shared/cfgparser.h \
    $(top_srcdir)/shared/output.c \
    $(top_srcdir)/shared/output.h \
    $(top_srcdir)/shared/getopt.c \
    $(top_srcdir)/shared/getopt.h \
    $(top_srcdir)/shared/misc.c \
    $(top_srcdir)/shared/misc.h \
    $(top_srcdir)/shared/network.c \
    $(top_srcdir)/shared/network.h \
    clamav-milter.c

man_MANS = $(top_builddir)/docs/man/clamav-milter.8

endif
endif

LIBS = $(top_builddir)/libclamav/libclamav.la @CLAMAV_MILTER_LIBS@ @THREAD_LIBS@
AM_CPPFLAGS = -I$(top_srcdir)/clamd -I$(top_srcdir)/libclamav -I$(top_srcdir)/shared -I$(top_srcdir)
EXTRA_DIST = clamav-milter.c INSTALL
