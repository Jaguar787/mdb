#ifndef FIND_SYMBOL_ADDRESS_H
#define FIND_SYMBOL_ADDRESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <libelf.h>
#include <gelf.h>

// Returns the address of a symbol, or 0 if not found/error
unsigned long find_symbol_address(const char *filename, const char *symbol_name);

#endif