#ifndef BREAKPOINT_MAP_H
#define BREAKPOINT_MAP_H

#define MAX_ITEMS 100

typedef struct {
    char func_name[256];
    unsigned long addr;         // I need this when removing
    long original_instruction;
    int is_active;
} Breakpoint;

typedef struct {
    unsigned long key;  // Addr
    Breakpoint value;   // Bp
} MapEntry;

typedef struct {
    MapEntry entries[MAX_ITEMS];
    int size;
} Map;

// 'extern' tells C that my_map exists, but is allocated in map.c
extern Map my_map; 

// Function declarations (signatures)
void map_insert(unsigned long addr, Breakpoint *bp);
Breakpoint* lookup_addr(unsigned long addr);
Breakpoint* lookup_func_name(char func_name[256]);

#endif