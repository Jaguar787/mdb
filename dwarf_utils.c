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

// Returns a Dwarf* handle. Caller must dwarf_end() and close(fd) when done.
Dwarf *open_dwarf(const char *binary_path, int *fd_out) {
    int fd = open(binary_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return NULL;
    }
    Dwarf *dbg = dwarf_begin(fd, DWARF_C_READ);
    if (!dbg) {
        fprintf(stderr, "Warning: no DWARF info found — was binary compiled with -g?\n");
        close(fd);
        return NULL;
    }
    *fd_out = fd;
    return dbg;
}

// Given an address, iterate compilation units to find the source file and line.
// Prints "filename:lineno" if found, silent if not.
void lookup_dwarf_line(Dwarf *dbg, uint64_t addr) {
    if (!dbg) {
        printf("  ; <no dwarf>\n");
        return;
    }

    Dwarf_Off offset = 0, next_offset;
    size_t header_size;
    int cu_count = 0;

    while (dwarf_nextcu(dbg, offset, &next_offset, &header_size, NULL, NULL, NULL) == 0) {
        cu_count++;
        Dwarf_Die cu_die;
        if (!dwarf_offdie(dbg, offset + header_size, &cu_die)) {
            offset = next_offset;
            continue;
        }

        Dwarf_Line *line = dwarf_getsrc_die(&cu_die, (Dwarf_Addr)addr);
        if (line) {
            const char *src = dwarf_linesrc(line, NULL, NULL);
            int lineno = 0;
            dwarf_lineno(line, &lineno);

            if (src) {
                // Find the last slash in the path
                const char *filename = strrchr(src, '/');
                #ifdef _WIN32
                if (!filename) filename = strrchr(src, '\\'); // Handle Windows paths
                #endif
    
                // If a slash was found, move past it; otherwise, use the whole string
                src = filename ? filename + 1 : src;
            }
            printf("  ; %s:%d\n", src ? src : "??", lineno);
            return;
        }

        offset = next_offset;
    }
    return;
}

// Walk subprogram DIEs in all CUs to find the address range of a named function.
// Writes low_pc and high_pc if found, returns 1 on success, 0 on failure.
int find_function_range(Dwarf *dbg, const char *name,
                        uint64_t base_address,
                        uint64_t *low_pc_out, uint64_t *high_pc_out) {
    Dwarf_Off offset = 0, next_offset;
    size_t header_size;

    while (dwarf_nextcu(dbg, offset, &next_offset, &header_size, NULL, NULL, NULL) == 0) {
        Dwarf_Die cu_die;
        if (!dwarf_offdie(dbg, offset + header_size, &cu_die)) {
            offset = next_offset;
            continue;
        }

        Dwarf_Die child;
        if (dwarf_child(&cu_die, &child) != 0) {
            offset = next_offset;
            continue;
        }

        do {
            if (dwarf_tag(&child) == DW_TAG_subprogram) {
                const char *fn_name = dwarf_diename(&child);
                if (fn_name && strcmp(fn_name, name) == 0) {
                    Dwarf_Addr low_pc, high_pc;
                    if (dwarf_lowpc(&child, &low_pc) == 0 &&
                        dwarf_highpc(&child, &high_pc) == 0) {
                        *low_pc_out  = (uint64_t)low_pc  + base_address;
                        *high_pc_out = (uint64_t)high_pc + base_address;
                        return 1;
                    }
                }
            }
        } while (dwarf_siblingof(&child, &child) == 0);

        offset = next_offset;
    }

    return 0;
}