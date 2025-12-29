ifneq (,)
This makefile requires GNU Make.
endif

INSTALL = install
INSTFLAGS = -m 755
VERSION = $(shell git describe --tags 2>/dev/null || echo "2.0")

DESTDIR =
PREFIX = /usr/local

CFLAGS += -std=c99 -Wall -O3 -D_POSIX_C_SOURCE=200809L
SHELL = sh
OS = $(shell uname -s)
CC = gcc

WAYLAND_PROTOCOLS_DIR = $(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER = $(shell pkg-config --variable=wayland_scanner wayland-scanner)

OBJS = bubblemon.o wayland_surface.o xdg-shell-protocol.o

ifeq ($(OS), Linux)
	OBJS += sys_linux.o
	LIBS = -lwayland-client -lrt -lm
endif

ifeq ($(OS), FreeBSD)
	OBJS += sys_freebsd.o
	LIBS = -lwayland-client -lrt -lm
endif

ifeq ($(OS), GNU/kFreeBSD)
	OBJS += sys_freebsd.o
	LIBS = -lwayland-client -lrt -lm
	CFLAGS += -D_BSD_SOURCE
endif

ifeq ($(OS), NetBSD)
	OBJS += sys_netbsd.o
	LIBS = -lwayland-client -lkvm -lm
	INSTFLAGS = -c -g kmem -m 2755 -o root
endif

ifeq ($(OS), OpenBSD)
	OBJS += sys_openbsd.o
	LIBS = -lwayland-client -lm
endif

all: wmbubble

xdg-shell-client-protocol.h:
	$(WAYLAND_SCANNER) client-header $(WAYLAND_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-protocol.c: xdg-shell-client-protocol.h
	$(WAYLAND_SCANNER) private-code $(WAYLAND_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-protocol.o: xdg-shell-protocol.c xdg-shell-client-protocol.h
	$(CC) $(CFLAGS) -c -o $@ $<

wayland_surface.o: wayland_surface.c wayland_surface.h xdg-shell-client-protocol.h
	$(CC) $(CFLAGS) -c -o $@ $<

wmbubble: $(OBJS)
	$(CC) $(LDFLAGS) -o wmbubble $(OBJS) $(LIBS)

bubblemon.o: bubblemon.c wayland_surface.h include/bubblemon.h \
 include/sys_include.h include/numbers-2.h include/ducks.h \
 include/digits.h misc/numbers.xpm misc/ofmspct.xpm misc/datefont.xpm

sys_%.o: sys_%.c include/bubblemon.h include/sys_include.h

clean:
	rm -f wmbubble *.o *.bb* *.gcov gmon.* *.da *~ \
		xdg-shell-protocol.c xdg-shell-client-protocol.h

install: wmbubble wmbubble.1
	$(INSTALL) -m 755 -d $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) $(INSTFLAGS) wmbubble $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -m 755 -d $(DESTDIR)$(PREFIX)/share/man/man1
	$(INSTALL) -m 644 wmbubble.1 $(DESTDIR)$(PREFIX)/share/man/man1

dist-tar:
	git archive -v -9 --prefix=wmbubble-$(VERSION)/ master \
		-o ../wmbubble-$(VERSION).tar.gz

dist: dist-tar

dist-debian: ../wmbubble-$(VERSION).tar.gz
	cp $< ../wmbubble_$(VERSION).orig.tar.gz

.PHONY: all clean install dist dist-tar dist-debian
