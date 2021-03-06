#
# Makefile
#
# Version:      $Id: Makefile,v 1.20.4.6 2006/04/12 20:45:20 aland Exp $
#

include ../../Make.inc

SRCS		= crypt.c dict.c filters.c hash.c hmac.c hmacsha1.c \
		  isaac.c log.c misc.c missing.c md4.c print.c \
		  radius.c rbtree.c sha1.c snprintf.c token.c udpfromto.c \
		  valuepair.c

LT_OBJS		= $(SRCS:.c=.lo)

INCLUDES	= ../include/radius.h ../include/libradius.h \
		  ../include/missing.h ../include/autoconf.h

CFLAGS		+= -D_LIBRADIUS -I../include

# if you have problems with things that need SHA1-HMAC, this will
# dump the key and the input to the hash so you can compare to what
# the other end is doing.
#CFLAGS += -DHMAC_SHA1_DATA_PROBLEMS

ifeq ($(USE_SHARED_LIBS),yes)
LINK_MODE	= -export-dynamic
else
LINK_MODE	= -static
endif

TARGET		= $(LIBPREFIX)radius

# Define new rule for libtool objects
%.lo : %.c
	$(LIBTOOL) --mode=compile $(CC) $(CFLAGS) -c $<

all: $(TARGET).la

$(TARGET).la: $(LT_OBJS)
	$(LIBTOOL) --mode=link $(CC) -release $(RADIUSD_VERSION) \
	$(LDFLAGS) $(LINK_MODE) -o $@ -rpath $(libdir) $^ $(LCRYPT)

$(LT_OBJS): $(INCLUDES)

clean:
	rm -f *.o *.lo $(TARGET).la
	rm -rf .libs

install: all
	$(INSTALL) -d -m 755	$(R)$(libdir)
	$(LIBTOOL) --mode=install $(INSTALL) -c $(TARGET).la \
		$(R)$(libdir)/$(TARGET).la
	rm -f $(R)$(libdir)/$(TARGET)-$(RADIUSD_VERSION).la;
	ln -s $(TARGET).la $(R)$(libdir)/$(TARGET)-$(RADIUSD_VERSION).la
