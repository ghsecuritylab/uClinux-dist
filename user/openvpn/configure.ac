dnl  OpenVPN -- An application to securely tunnel IP networks
dnl             over a single UDP port, with support for SSL/TLS-based
dnl             session authentication and key exchange,
dnl             packet encryption, packet authentication, and
dnl             packet compression.
dnl
dnl  Copyright (C) 2002-2005 OpenVPN Solutions LLC <info@openvpn.net>
dnl
dnl  This program is free software; you can redistribute it and/or modify
dnl  it under the terms of the GNU General Public License as published by
dnl  the Free Software Foundation; either version 2 of the License, or
dnl  (at your option) any later version.
dnl
dnl  This program is distributed in the hope that it will be useful,
dnl  but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl  GNU General Public License for more details.
dnl
dnl  You should have received a copy of the GNU General Public License
dnl  along with this program (see the file COPYING included with this
dnl  distribution); if not, write to the Free Software Foundation, Inc.,
dnl  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.50)

m4_include(version.m4)
AC_INIT([OpenVPN], [PRODUCT_VERSION], [openvpn-users@lists.sourceforge.net], [openvpn])
AM_CONFIG_HEADER(config.h)
AC_CONFIG_SRCDIR(syshead.h)

AC_ARG_ENABLE(lzo,
   [  --disable-lzo           Disable LZO compression support],
   [LZO="$enableval"],
   [LZO="yes"]
)

AC_ARG_ENABLE(crypto,
   [  --disable-crypto        Disable OpenSSL crypto support],
   [CRYPTO="$enableval"],
   [CRYPTO="yes"]
)

AC_ARG_ENABLE(ssl,
   [  --disable-ssl           Disable OpenSSL SSL support for TLS-based key exchange],
   [SSL="$enableval"],
   [SSL="yes"]
)

AC_ARG_ENABLE(multi,
   [  --disable-multi         Disable client/server support (--mode server + client mode)],
   [MULTI="$enableval"],
   [MULTI="yes"]
)

AC_ARG_ENABLE(server,
   [  --disable-server        Disable server support only (but retain client support)],
   [MULTI_SERVER="$enableval"],
   [MULTI_SERVER="yes"]
)

AC_ARG_ENABLE(plugins,
   [  --disable-plugins       Disable plug-in support],
   [PLUGINS="$enableval"],
   [PLUGINS="yes"]
)

AC_ARG_ENABLE(management,
   [  --disable-management    Disable management server support],
   [MANAGEMENT="$enableval"],
   [MANAGEMENT="yes"]
)

AC_ARG_ENABLE(pkcs11,
   [  --disable-pkcs11        Disable pkcs11 support],
   [PKCS11="$enableval"],
   [PKCS11="yes"]
)

AC_ARG_ENABLE(socks,
   [  --disable-socks         Disable Socks support],
   [SOCKS="$enableval"],
   [SOCKS="yes"]
)

AC_ARG_ENABLE(http,
   [  --disable-http          Disable HTTP proxy support],
   [HTTP_PROXY="$enableval"],
   [HTTP_PROXY="yes"]
)

AC_ARG_ENABLE(fragment,
   [  --disable-fragment      Disable internal fragmentation support (--fragment)],
   [FRAGMENT="$enableval"],
   [FRAGMENT="yes"]
)

AC_ARG_ENABLE(multihome,
   [  --disable-multihome     Disable multi-homed UDP server support (--multihome)],
   [MULTIHOME="$enableval"],
   [MULTIHOME="yes"]
)

AC_ARG_ENABLE(port-share,
   [  --disable-port-share    Disable TCP server port-share support (--port-share)],
   [PORT_SHARE="$enableval"],
   [PORT_SHARE="yes"]
)

AC_ARG_ENABLE(debug,
   [  --disable-debug         Disable debugging support (disable gremlin and verb 7+ messages)],
   [DEBUG="$enableval"],
   [DEBUG="yes"]
)

AC_ARG_ENABLE(small,
   [  --enable-small          Enable smaller executable size (disable OCC, usage message, and verb 4 parm list)],
   [SMALL="$enableval"],
   [SMALL="no"]
)

AC_ARG_ENABLE(pthread,
   [  --enable-pthread        Enable pthread support (Experimental for OpenVPN 2.0)],
   [PTHREAD="$enableval"],
   [PTHREAD="no"]
)

AC_ARG_ENABLE(password-save,
   [  --enable-password-save  Allow --askpass and --auth-user-pass passwords to be read from a file],
   [PASSWORD_SAVE="$enableval"],
   [PASSWORD_SAVE="no"]
)

AC_ARG_ENABLE(iproute2,
   [  --enable-iproute2       Enable support for iproute2],
   test $enableval = "yes" && AC_DEFINE(CONFIG_FEATURE_IPROUTE, 1, [enable iproute2 support])   
)

AC_ARG_ENABLE(strict,
   [  --enable-strict         Enable strict compiler warnings (debugging option)],
   [STRICT="$enableval"],
   [STRICT="no"]
)

AC_ARG_ENABLE(pedantic,
   [  --enable-pedantic       Enable pedantic compiler warnings, will not generate a working executable (debugging option)],
   [PEDANTIC="$enableval"],
   [PEDANTIC="no"]
)

AC_ARG_ENABLE(profiling,
   [  --enable-profiling      Enable profiling (debugging option)],
   [PROFILE="$enableval"],
   [PROFILE="no"]
)

AC_ARG_ENABLE(strict-options,
   [  --enable-strict-options Enable strict options check between peers (debugging option)],
   [STRICT_OPTIONS="$enableval"],
   [STRICT_OPTIONS="no"]
)

AC_ARG_WITH(ssl-headers,
   [  --with-ssl-headers=DIR  Crypto/SSL Include files location],
   [CS_HDR_DIR="$withval"]
   [CPPFLAGS="$CPPFLAGS -I$withval"] 
)

AC_ARG_WITH(ssl-lib,
   [  --with-ssl-lib=DIR      Crypto/SSL Library location],
   [LDFLAGS="$LDFLAGS -L$withval"] 
)

AC_ARG_WITH(lzo-headers,
   [  --with-lzo-headers=DIR  LZO Include files location],
   [LZO_HDR_DIR="$withval"]
   [CPPFLAGS="$CPPFLAGS -I$withval"] 
)

AC_ARG_WITH(lzo-lib,
   [  --with-lzo-lib=DIR      LZO Library location],
   [LDFLAGS="$LDFLAGS -L$withval"] 
)

AC_ARG_WITH(ifconfig-path,
   [  --with-ifconfig-path=PATH   Path to ifconfig tool],
   [IFCONFIG="$withval"],
   [AC_PATH_PROG([IFCONFIG], [ifconfig], [ifconfig], [$PATH:/usr/local/sbin:/usr/sbin:/sbin])]
)
AC_DEFINE_UNQUOTED(IFCONFIG_PATH, "$IFCONFIG", [Path to ifconfig tool])

AC_ARG_WITH(iproute-path,
   [  --with-iproute-path=PATH    Path to iproute tool],
   [IPROUTE="$withval"],
   [AC_PATH_PROG([IPROUTE], [ip], [ip], [$PATH:/usr/local/sbin:/usr/sbin:/sbin])]
)
AC_DEFINE_UNQUOTED(IPROUTE_PATH, "$IPROUTE", [Path to iproute tool])


AC_ARG_WITH(route-path,
   [  --with-route-path=PATH  Path to route tool],
   [ROUTE="$withval"],
   [AC_PATH_PROG([ROUTE], [route], [route], [$PATH:/usr/local/sbin:/usr/sbin:/sbin])]
)
AC_DEFINE_UNQUOTED(ROUTE_PATH, "$ROUTE", [Path to route tool])

AC_ARG_WITH(mem-check,
   [  --with-mem-check=TYPE  Build with debug memory checking, TYPE = dmalloc or valgrind],
   [MEMCHECK="$withval"]
)

dnl Guess host type.
AC_CANONICAL_HOST
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE(openvpn, [$PACKAGE_VERSION])

dnl fix search path, to allow compilers to find syshead.h
CPPFLAGS="$CPPFLAGS -I${srcdir}"

dnl check target OS
openvpn_target=$target
if test $target_alias; then
  openvpn_target=$target_alias
fi
AC_DEFINE_UNQUOTED(TARGET_ALIAS, "$openvpn_target", [A string representing our target])
case "$target" in
*linux*)
	AC_DEFINE(TARGET_LINUX, 1, [Are we running on Linux?])
	dnl RH9 SSL headers workaround
        if test -z $CS_HDR_DIR && test "$CRYPTO" = "yes"; then
           CPPFLAGS="$CPPFLAGS $(pkg-config --cflags openssl 2>/dev/null)"
        fi
	;;
*solaris*)
	AC_DEFINE(TARGET_SOLARIS, 1, [Are we running on Solaris?])
	;;
*openbsd*)
	AC_DEFINE(TARGET_OPENBSD, 1, [Are we running on OpenBSD?])
	;;
*freebsd*)
	AC_DEFINE(TARGET_FREEBSD, 1, [Are we running on FreeBSD?])
	;;
*netbsd*)
	AC_DEFINE(TARGET_NETBSD, 1, [Are we running NetBSD?])
	;;
*darwin*)
	dnl some Mac OS X tendering (we use vararg macros...)
	AC_DEFINE(TARGET_DARWIN, 1, [Are we running on Mac OS X?])
	CPPFLAGS="$CPPFLAGS -no-cpp-precomp"
	;;
*mingw*)
	AC_MSG_RESULT([WARNING: configure support for mingw is incomplete])
	AC_MSG_RESULT([WARNING: use makefile.w32 instead])
	OPENVPN_ADD_LIBS(-lgdi32)
	OPENVPN_ADD_LIBS(-lwsock32)
	;;
	
esac

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_GCC_TRADITIONAL

dnl Checks for header files.
AC_HEADER_STDC

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_HEADER_TIME
AX_CPP_VARARG_MACRO_ISO
AX_CPP_VARARG_MACRO_GCC
AX_EMPTY_ARRAY

dnl Check for more header files.
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(sys/time.h sys/socket.h sys/ioctl.h sys/stat.h dnl
		 sys/mman.h fcntl.h sys/file.h stdlib.h stdint.h dnl
		 stdarg.h unistd.h signal.h stdio.h string.h dnl
		 strings.h ctype.h errno.h syslog.h pwd.h grp.h dnl
		 net/if_tun.h stropts.h sys/sockio.h dnl
		 netinet/in.h netinet/in_systm.h dnl
		 netinet/tcp.h arpa/inet.h dnl
		 netdb.h sys/uio.h linux/if_tun.h linux/sockios.h dnl
		 linux/types.h sys/poll.h sys/epoll.h err.h dnl
)
AC_CHECK_HEADERS(net/if.h,,,
		 [#ifdef HAVE_SYS_SOCKET_H 
		  # include <sys/socket.h>
		  #endif
		 ])
AC_CHECK_HEADERS(netinet/ip.h,,,
		 [#ifdef HAVE_SYS_TYPES_H
		  # include <sys/types.h>
		  #endif
		  #ifdef HAVE_NETINET_IN_H
		  # include <netinet/in.h>
		  #endif
		  #ifdef HAVE_NETINET_IN_SYSTM_H
		  # include <netinet/in_systm.h>
		  #endif
		 ])
AC_CHECK_HEADERS(netinet/if_ether.h,,,
		 [#ifdef HAVE_SYS_TYPES_H
		  # include <sys/types.h>
		  #endif
		  #ifdef HAVE_SYS_SOCKET_H
		  # include <sys/socket.h>
		  #endif
		  #ifdef HAVE_NETINET_IN_H
		  # include <netinet/in.h>
		  #endif
		 ])
AC_CHECK_HEADERS(resolv.h,,,
		 [#ifdef HAVE_NETINET_IN_H 
		  # include <netinet/in.h>
		  #endif
		 ])
AC_CHECK_HEADERS(linux/errqueue.h,,,
	[#ifdef HAVE_LINUX_TYPES_H
	 # include <linux/types.h>
	 #endif
	])
AC_CACHE_SAVE

dnl check that in_addr_t is defined
AC_CHECK_TYPE(
	[in_addr_t],
	[],
	[AC_DEFINE(in_addr_t, uint32_t, [Some systems don't define in_addr_t])],
	[#include "syshead.h"])

dnl check for basic types
AC_CHECK_TYPE(
	[uint8_t],
	[],
	[AC_DEFINE(uint8_t, unsigned char, [8-bit unsigned type])],
	[#include "syshead.h"])
AC_CHECK_TYPE(
	[uint16_t],
	[],
	[AC_DEFINE(uint16_t, unsigned char, [16-bit unsigned type])],
	[#include "syshead.h"])
AC_CHECK_TYPE(
	[uint32_t],
	[],
	[AC_DEFINE(uint32_t, unsigned long, [32-bit unsigned type])],
	[#include "syshead.h"])

dnl check for IPv6 types
AC_CHECK_TYPE(
	[struct tun_pi],
	[AC_DEFINE(HAVE_TUN_PI, 1, [struct tun_pi needed for IPv6 support])],
	[],
	[#include "syshead.h"])
AC_CHECK_TYPE(
	[struct iphdr],
	[AC_DEFINE(HAVE_IPHDR, 1, [struct iphdr needed for IPv6 support])],
	[],
	[#include "syshead.h"])
AC_CHECK_TYPE(
	[struct iovec],
	[AC_DEFINE(HAVE_IOVEC, 1, [struct iovec needed for IPv6 support])],
	[],
	[#include "syshead.h"])

dnl check for extended socket error types
AC_CHECK_TYPE(
	[struct sock_extended_err],
	[AC_DEFINE(HAVE_SOCK_EXTENDED_ERR, 1, [struct sock_extended_err needed for extended socket error support])],
	[],
	[#include "syshead.h"])
AC_CHECK_TYPE(
	[struct msghdr],
	[AC_DEFINE(HAVE_MSGHDR, 1, [struct msghdr needed for extended socket error support])],
	[],
	[#include "syshead.h"])
AC_CHECK_TYPE(
	[struct cmsghdr],
	[AC_DEFINE(HAVE_CMSGHDR, 1, [struct cmsghdr needed for extended socket error support])],
	[],
	[#include "syshead.h"])
AC_CHECK_TYPE(
	[struct in_pktinfo],
	[AC_DEFINE(HAVE_IN_PKTINFO, 1, [struct in_pktinfo needed for IP_PKTINFO support])],
	[],
	[#include "syshead.h"])

AC_CHECK_SIZEOF(unsigned int)
AC_CHECK_SIZEOF(unsigned long)

AC_CACHE_SAVE

dnl check for other types
TYPE_SOCKLEN_T
AC_TYPE_SIGNAL

dnl Check for libsocket
AC_SEARCH_LIBS(socket, socket)

dnl Check for libnsl
AC_SEARCH_LIBS(inet_ntoa, nsl)

dnl Check for libresolv
AC_SEARCH_LIBS(gethostbyname, resolv nsl)

dnl optional library functions
AC_FUNC_FORK
AC_CHECK_FUNCS(daemon chroot getpwnam setuid nice system getpid dup dup2 dnl
	       getpass strerror syslog openlog mlockall getgrnam setgid dnl
	       setgroups stat flock readv writev setsockopt getsockopt dnl
	       setsid chdir gettimeofday putenv getpeername unlink dnl
               poll chsize ftruncate sendmsg recvmsg getsockname)
AC_CACHE_SAVE

dnl Required library functions
AC_FUNC_MEMCMP

AC_CHECK_FUNCS(socket recv recvfrom send sendto listen dnl
	       accept connect bind select gethostbyname dnl
               inet_ntoa time ctime memset vsnprintf strdup, [],
	       [AC_MSG_ERROR([Required library function not found])])

dnl
dnl check libraries
dnl

dnl Checking for a working epoll
AC_CHECKING([for working epoll implementation])
OLDLDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS -Wl,--fatal-warnings"
AC_CHECK_FUNCS(epoll_create, AC_DEFINE([HAVE_EPOLL_CREATE], 1, []))
LDFLAGS="$OLDLDFLAGS"

dnl
dnl check for valgrind tool
dnl

if test "$MEMCHECK" = "valgrind"; then
   AC_CHECKING([for valgrind tool and Header files])
   AC_CHECK_HEADER(valgrind/memcheck.h, 
        [
	    AC_DEFINE(USE_VALGRIND, 1, [Use valgrind memory debugging library])
	    CFLAGS="-g -fno-inline"
	],
        [AC_MSG_ERROR([valgrind headers not found.])]
   )
fi

dnl
dnl check for pthread library
dnl

if test "$PTHREAD" = "yes"; then
  AC_CHECKING([for pthread support])
  AC_MSG_RESULT([********* WARNING: pthread support is experimental for OpenVPN 2.0])
  ACX_PTHREAD(
        [
	    case "$target" in
	    *openbsd*)
		AC_MSG_RESULT([WARNING: pthread support on OpenBSD is unstable!])
		CFLAGS="$CFLAGS -pthread"
		;;
	    esac
	    LIBS="$PTHREAD_LIBS $LIBS"
	    CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
	    CC="$PTHREAD_CC"
	    AC_DEFINE(USE_PTHREAD, 1, [Use pthread-based multithreading])
	],
	[
	    AC_MSG_RESULT([I don't know how to build with pthread support on this platform.])
	    AC_MSG_ERROR([try ./configure --disable-pthread])
	])
fi

dnl
dnl check for dmalloc library
dnl

if test "$MEMCHECK" = "dmalloc"; then
   AC_CHECKING([for dmalloc Library and Header files])
   AC_CHECK_HEADER(dmalloc.h, 
        [AC_CHECK_LIB(dmalloc, malloc,
	    [
               if test "$PTHREAD" = "yes"; then
                   OPENVPN_ADD_LIBS(-ldmallocth)
               else
                   OPENVPN_ADD_LIBS(-ldmalloc)
               fi
	       AC_DEFINE(DMALLOC, 1, [Use dmalloc memory debugging library])
	    ],
	    [AC_MSG_ERROR([dmalloc library not found.])]
	)],
        [AC_MSG_ERROR([dmalloc headers not found.])]
   )
fi

dnl
dnl Check for dlopen -- first try libc then libdl.
dnl
if test "$PLUGINS" = "yes"; then
   AC_CHECKING([for libdl Library and Header files])
   AC_CHECK_HEADER(dlfcn.h,
        [AC_CHECK_FUNC(dlopen,
            [AC_DEFINE(USE_LIBDL, 1, [Use libdl for dynamic library loading])],
            [AC_CHECK_LIB(dl, dlopen,
	        [
                   OPENVPN_ADD_LIBS(-ldl)
	           AC_DEFINE(USE_LIBDL, 1, [Use libdl for dynamic library loading])
	        ],
	        [AC_MSG_RESULT([libdl library not found.])]
	    )],
        )],
        [AC_MSG_RESULT([libdl headers not found.])]
   )
fi

dnl
dnl check for LZO library
dnl

if test "$LZO" = "yes"; then
   LZO_H=""
   AC_CHECKING([for LZO Library and Header files])
   AC_CHECK_HEADER(lzo/lzo1x.h,
	[ LZO_H="2"
	  lzolibs="lzo2 lzo"
	  AC_DEFINE(LZO_HEADER_DIR, 1, [Use lzo/ directory prefix for LZO header files (for LZO 2.0)])
	],
	[ AC_CHECK_HEADER(lzo1x.h, [ LZO_H="1" ; lzolibs=lzo ]) ]
   )

   if test -n "$LZO_H"; then
     havelzolib=0
     for i in $lzolibs ; do
	if test $havelzolib = 1 ; then break ; fi
	AC_CHECK_LIB($i, lzo1x_1_15_compress,
          [
	    OPENVPN_ADD_LIBS(-l$i)
	    AC_DEFINE(USE_LZO, 1, [Use LZO compression library])
	    AC_DEFINE_UNQUOTED(LZO_VERSION_NUM, "$LZO_H", [LZO version number])
	    havelzolib=1
	  ]
        )
     done
     if test $havelzolib = 0 ; then
       AC_MSG_ERROR([LZO headers were found but LZO library was not found])
     fi
   else
     AC_MSG_RESULT([LZO headers were not found])
     AC_MSG_RESULT([LZO library available from http://www.oberhumer.com/opensource/lzo/])
     AC_MSG_ERROR([Or try ./configure --disable-lzo])
   fi
fi

dnl
dnl check for OpenSSL-crypto library
dnl

if test "$CRYPTO" = "yes"; then
   AC_CHECKING([for OpenSSL Crypto Library and Header files])
   AC_CHECK_HEADER(openssl/evp.h,
	[AC_CHECK_LIB(crypto, EVP_CIPHER_CTX_init,
            [
		   AC_CHECKING([that OpenSSL Library is at least version 0.9.6])
		   AC_EGREP_CPP(yes,
		     [
		       #include "openssl/evp.h"
		       #if SSLEAY_VERSION_NUMBER >= 0x00906000L
		         yes
		       #endif
		     ],
		     [
                       AC_DEFINE(USE_CRYPTO, 1, [Use OpenSSL crypto library])
                       OPENVPN_ADD_LIBS(-lcrypto)
                       AC_CHECK_FUNCS(EVP_CIPHER_CTX_set_key_length)

		       dnl check for OpenSSL crypto acceleration capability
		       AC_CHECK_HEADERS(openssl/engine.h)
                       AC_CHECK_FUNCS(ENGINE_load_builtin_engines)
                       AC_CHECK_FUNCS(ENGINE_register_all_complete)
                       AC_CHECK_FUNCS(ENGINE_cleanup)
                     ],
		     [AC_MSG_ERROR([OpenSSL crypto Library is too old.])]
		   )
	    ],
	    [AC_MSG_ERROR([OpenSSL Crypto library not found.])]
	)],
	[AC_MSG_ERROR([OpenSSL Crypto headers not found.])]
   )

dnl
dnl check for OpenSSL-SSL library
dnl

   if test "$SSL" = "yes"; then
      AC_CHECKING([for OpenSSL SSL Library and Header files])
      AC_CHECK_HEADER(openssl/ssl.h,
	   [AC_CHECK_LIB(ssl, SSL_CTX_new,
	       [
                  if test "$MEMCHECK" = "ssl"; then
                     AC_CHECKING([for Memory Debugging Capabilities in OpenSSL Library])
                     AC_CHECK_LIB(ssl, CRYPTO_mem_ctrl,
                         [
                             AC_DEFINE(CRYPTO_MDEBUG, 1, [Use memory debugging function in OpenSSL])
			     AC_MSG_RESULT([NOTE: OpenSSL library must be compiled with CRYPTO_MDEBUG])
                         ],
                         [AC_MSG_ERROR([Memory Debugging function in OpenSSL library not found.])]
                     )
                  fi
	       ],
	       [AC_MSG_ERROR([OpenSSL SSL library not found.])]
	   )],
	   [AC_MSG_ERROR([OpenSSL SSL headers not found.])]
      )
      AC_DEFINE(USE_SSL, 1, [Use OpenSSL SSL library])
      OPENVPN_ADD_LIBS(-lssl)
   fi
fi

dnl enable pkcs11 capability
if test "$PKCS11" = "yes"; then
   AC_CHECKING([for pkcs11-helper Library and Header files])
   AC_CHECK_HEADER(pkcs11-helper-1.0/pkcs11h-core.h,
	[AC_CHECK_LIB(pkcs11-helper, pkcs11h_initialize,
	    [
		   AC_DEFINE(USE_PKCS11, 1, [Enable PKCS11 capability])
		   OPENVPN_ADD_LIBS(-lpkcs11-helper)
	    ],
	    [AC_MSG_RESULT([pkcs11-helper library not found.])]
	)],
	[AC_MSG_RESULT([pkcs11-helper headers not found.])]
   )
fi

dnl enable multi-client mode
if test "$MULTI" = "yes"; then
   AC_DEFINE(ENABLE_CLIENT_SERVER, 1, [Enable client/server capability])
fi

dnl enable client mode only, not server
if test "$MULTI_SERVER" = "no"; then
   AC_DEFINE(ENABLE_CLIENT_ONLY, 1, [Enable client capability only])
fi

dnl enable management server capability
if test "$MANAGEMENT" = "yes"; then
   AC_DEFINE(ENABLE_MANAGEMENT, 1, [Enable management server capability])
fi

dnl enable socks 
if test "$SOCKS" = "yes"; then
   AC_DEFINE(ENABLE_SOCKS, 1, [Enable Socks proxy support])
fi

dnl enable HTTP proxy
if test "$HTTP_PROXY" = "yes"; then
   AC_DEFINE(ENABLE_HTTP_PROXY, 1, [Enable HTTP proxy support])
fi

dnl compile --multihome option
if test "$MULTIHOME" = "yes"; then
   AC_DEFINE(ENABLE_MULTIHOME, 1, [Enable multi-homed UDP server capability])
fi

dnl enable debugging
if test "$DEBUG" = "yes"; then
   AC_DEFINE(ENABLE_DEBUG, 1, [Enable debugging support])
fi

dnl enable small size optimizations
if test "$SMALL" = "yes"; then
   AC_DEFINE(ENABLE_SMALL, 1, [Enable smaller executable size])
fi

dnl enable --fragment
if test "$FRAGMENT" = "yes"; then
   AC_DEFINE(ENABLE_FRAGMENT, 1, [Enable internal fragmentation support])
fi

dnl enable --port-share
if test "$PORT_SHARE" = "yes"; then
   AC_DEFINE(ENABLE_PORT_SHARE, 1, [Enable TCP Server port sharing])
fi

dnl enable strict compiler warnings
if test "$STRICT" = "yes"; then
   CFLAGS="$CFLAGS -Wall -Wpointer-arith -Wsign-compare -Wno-unused-parameter -Wno-unused-function"
fi

dnl enable pedantic compiler warnings
if test "$PEDANTIC" = "yes"; then
   CFLAGS="$CFLAGS -ansi -pedantic"
fi

dnl enable profiling
if test "$PROFILE" = "yes"; then
   CFLAGS="$CFLAGS -pg -DENABLE_PROFILING"
fi

dnl enable strict options check between peers
if test "$STRICT_OPTIONS" = "yes"; then
   AC_DEFINE(STRICT_OPTIONS_CHECK, 1, [Enable strict options check between peers])
fi

dnl enable password save
if test "$PASSWORD_SAVE" = "yes"; then
   AC_DEFINE(ENABLE_PASSWORD_SAVE, 1, [Allow --askpass and --auth-user-pass passwords to be read from a file])
fi

AC_OUTPUT(Makefile openvpn.spec)
