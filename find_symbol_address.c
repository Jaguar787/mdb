#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <libelf.h>
#include <gelf.h>

// Returns the address of a symbol, or 0 if not found/error
unsigned long find_symbol_address(const char *filename, const char *symbol_name) {
    int fd;
    Elf *elf;
    Elf_Scn *scn = NULL;
    GElf_Shdr shdr;
    Elf_Data *data = NULL;
    unsigned long symbol_address = 0;

    // Initialize libelf
    if (elf_version(EV_CURRENT) == EV_NONE) {
        fprintf(stderr, "Libelf initialization failed.\n");
        return 0;
    }

    // Open the target binary file
    if ((fd = open(filename, O_RDONLY, 0)) < 0) {
        perror("Failed to open binary file");
        return 0;
    }

    if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
        fprintf(stderr, "elf_begin() failed: %s\n", elf_errmsg(-1));
        close(fd);
        return 0;
    }

    // Loop through all sections in the ELF file looking for the Symbol Table
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        gelf_getshdr(scn, &shdr);
        
        // SHT_SYMTAB holds the regular symbol table
        if (shdr.sh_type == SHT_SYMTAB) {
            data = elf_getdata(scn, data);
            int count = shdr.sh_size / shdr.sh_entsize; // Number of symbols

            // Iterate through every symbol record
            for (int i = 0; i < count; i++) {
                GElf_Sym sym;
                gelf_getsym(data, i, &sym);

                // Get the string name of the symbol from the associated string table
                char *name = elf_strptr(elf, shdr.sh_link, sym.st_name);
                if (!name) continue;

                // Check if this symbol matches the name we are looking for
                if (strcmp(name, symbol_name) == 0) {
                    symbol_address = sym.st_value;
                    break;
                }
            }
        }
        if (symbol_address != 0) break; 
    }

    // Clean up resources
    elf_end(elf);
    close(fd);
    
    return symbol_address;
}