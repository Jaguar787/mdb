#ifndef GET_CHILD_BASE_ADDRESS_H
#define GET_CHILD_BASE_ADDRESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

unsigned long get_child_base_address(pid_t child_pid);

#endif