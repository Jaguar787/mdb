CC ?= gcc
CFLAGS ?= -Wall -Wextra -g -O0
CPPFLAGS ?=
LDFLAGS ?=

PKG_CONFIG ?= pkg-config

# Link dbg with the required debug and tracing libraries.
DBG_CPPFLAGS := $(shell $(PKG_CONFIG) --cflags libdw libelf capstone 2>/dev/null)
DBG_LDLIBS := $(shell $(PKG_CONFIG) --libs libdw libelf capstone 2>/dev/null || echo "-ldw -lelf -lcapstone")

CPPFLAGS += $(DBG_CPPFLAGS)

.PHONY: all clean

all: dbg test

dbg: dbg.c breakpoint_map.c dwarf_utils.c find_symbol_address.c get_child_base_address.c breakpoint_map.h dwarf_utils.h find_symbol_address.h get_child_base_address.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ dbg.c breakpoint_map.c dwarf_utils.c find_symbol_address.c get_child_base_address.c $(LDFLAGS) $(DBG_LDLIBS)

test: test.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ test.c $(LDFLAGS)

clean:
	rm -f dbg test
