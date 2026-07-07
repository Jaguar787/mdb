#ifndef DWARF_UTILS_H
#define DWARF_UTILS_H

#include <elfutils/libdw.h>
#include <elfutils/libdwfl.h>
#include <dwarf.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <fstab.h>
#include <elfutils/libdw.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <elfutils/libdw.h>
#include <stdint.h>
#include <string.h>

Dwarf *open_dwarf(const char *binary_path, int *fd_out);
void   lookup_dwarf_line(Dwarf *dbg, uint64_t addr);
int    find_function_range(Dwarf *dbg, const char *name,
                           uint64_t base_address,
                           uint64_t *low_pc_out, uint64_t *high_pc_out);

#endif