# st version
VERSION = 0.3

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# includes and libs
INCS = -I. -I/opt/retrofw/mipsel-buildroot-linux-uclibc/sysroot/usr/include
LIBS = -lc -lutil

# flags
CPPFLAGS = -DVERSION=\"${VERSION}\"
CFLAGS += -g -Wall ${INCS} ${CPPFLAGS} -DD8 -fPIC
LDFLAGS += -g ${LIBS} -lSDL

# compiler and linker
CC ?= cc
