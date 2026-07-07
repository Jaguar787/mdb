#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

unsigned long get_child_base_address(pid_t child_pid) {
    char maps_path[256];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", child_pid);

    FILE *maps_file = fopen(maps_path, "r");
    if (!maps_file) {
        perror("Failed to open maps file");
        return 0;
    }

    char line[256];
    unsigned long base_address = 0;

    // Read the first line of the maps file to get the base address
    if (fgets(line, sizeof(line), maps_file)) {
        sscanf(line, "%lx-%*s", &base_address);
    }

    fclose(maps_file);
    return base_address;
}