#include "breakpoint_map.h"
#include <string.h>
#include <stdio.h>

Map my_map = {.size = 0};

/**
 * Inserts a new breakpoint into the map. If the key already exists, it updates the existing entry.
 * @param key The address of the breakpoint.
 * @param bp Pointer to the Breakpoint structure to be inserted.
 */
void map_insert(unsigned long key, Breakpoint *bp)
{
    for (int i = 0; i < my_map.size; i++)
    {
        if (my_map.entries[i].key == key)
        {
            my_map.entries[i].value = *bp;
        }
    }
    my_map.entries[my_map.size].key = key;
    my_map.entries[my_map.size].value = *bp;
    my_map.size++;
}

/**
 * Looks up a breakpoint in the map by its address.
 * @param addr The address of the breakpoint to look up.
 * @return Pointer to the Breakpoint structure's value if found, NULL otherwise.
 */
Breakpoint *lookup_addr(unsigned long addr)
{
    for (int i = 0; i < my_map.size; i++)
    {
        if (my_map.entries[i].key == addr)
        {
            return &my_map.entries[i].value;
        }
    }
    return NULL;
}

/**
 * Looks up a breakpoint in the map by its function name.
 * @param name The name of the breakpoint to look up.
 * @return Pointer to the Breakpoint structure's value if found, NULL otherwise.
 */
Breakpoint *lookup_func_name(char name[256])
{
    for (int i = 0; i < my_map.size; i++)
    {
        if (strcmp(my_map.entries[i].value.func_name, name) == 0)
        {
            return &my_map.entries[i].value;
        }
    }
    return NULL;
}
