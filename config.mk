# wmii-dialog version
VERSION = 0.1

# Customize below to fit your system

# debug
#DEBUG = -ggdb -std=c99 -pedantic

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# includes and libs
INCS = -I. -I/usr/include -I${X11INC}
LIBS = -L/usr/lib -L${X11LIB} -lX11 -pthread

# flags
CFLAGS = ${DEBUG} -Wall -Os ${INCS} -DVERSION=\"${VERSION}\"
LDFLAGS = ${DEBUG} ${LIBS}

# Solaris
#CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS}
#CFLAGS += -xtarget=ultra

# compiler and linker
CC = cc
