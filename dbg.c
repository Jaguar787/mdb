#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <libelf.h>
#include <gelf.h>
#include <capstone/capstone.h>

#include "dwarf_utils.h"
#include "find_symbol_address.h"
#include "get_child_base_address.h"
#include "breakpoint_map.h"

int main(int argc, char *argv[])
{
    int status;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <program_to_trace>\n", argv[0]);
        return 1;
    }

    csh capstone_handle;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &capstone_handle) != CS_ERR_OK)
    {
        fprintf(stderr, "Failed to initialize Capstone disassembler\n");
        return 1;
    }

    int dwarf_fd;
    Dwarf *dbg = open_dwarf(argv[1], &dwarf_fd);

    pid_t child_pid = fork();

    if (child_pid == 0)
    {
        /* Child: allow tracing then exec target */
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execl(argv[1], argv[1], NULL);
        perror("Failed to execute child process");
        exit(1);
    }

    waitpid(child_pid, &status, 0);

    /* Fetch base address once, right after the child stops at exec */
    unsigned long base_address = get_child_base_address(child_pid);

    int call_depth = 1;
    int step_count = 0;
    struct user_regs_struct regs;
    unsigned long main_address = 0;
    long saved_word = 0;
    cs_insn *insn = NULL;

    printf("Welcome to micro debugger.\n");
    printf("The options are:\n");
    printf("1. b (breakpoint at a function name)\n");
    printf("2. r (remove a breakpoint)\n");
    printf("3. c (continue execution)\n");
    printf("4. s (step through instructions)\n");
    printf("5. q (quit)\n");

    while (1)
    {
        printf("Enter command: ");

        char input[256];
        fgets(input, sizeof(input), stdin);

        if (input[0] == 'q')
        {
            printf("Quitting debugger.\n");
            break;
        }
        else if (input[0] == 'b')
        {
            printf("Enter function name: ");
            fgets(input, sizeof(input), stdin);
            char func_name[256];
            sscanf(input, "%255s", func_name);

            unsigned long offset = find_symbol_address(argv[1], func_name); // Fetch offset because of ASLR
            if (offset == 0)
            {
                printf("Function '%s' not found.\n", func_name);
                continue;
            }
            main_address = base_address + offset;
            printf("Base address: 0x%lx\n", base_address);
            printf("Setting breakpoint at %s: offset 0x%lx, runtime 0x%lx\n",
                   func_name, offset, main_address);

            /* Save original word then plant INT3 */
            saved_word = ptrace(PTRACE_PEEKTEXT, child_pid, (void *)main_address, NULL);
            long trap = (saved_word & ~0xFF) | 0xCC;
            ptrace(PTRACE_POKETEXT, child_pid, (void *)main_address, (void *)trap);

            printf("Breakpoint set at address: %p\n", (void *)main_address);

            Breakpoint bp;

            strncpy(bp.func_name, func_name, sizeof(bp.func_name) - 1);
            bp.func_name[sizeof(bp.func_name) - 1] = '\0';
            bp.addr = main_address;
            bp.original_instruction = saved_word;
            bp.is_active = 1;

            map_insert(main_address, &bp);
        }
        else if (input[0] == 'r')
        {
            printf("Removing a breakpoint: ");
            fgets(input, sizeof(input), stdin);
            char bpToRemove[256];
            sscanf(input, "%255s", bpToRemove);

            Breakpoint *bp = lookup_func_name(bpToRemove);

            if (bp == NULL)
            {
                printf("The function breakpoint, %s, does not exist.\n", bpToRemove);
                continue;
            }
            bp->is_active = 0; // False
            printf("Breakpoint, %s, is removed at %lx.\n", bpToRemove, bp->addr);
        }
        else if (input[0] == 'c')
        {
            ptrace(PTRACE_CONT, child_pid, NULL, NULL);
            waitpid(child_pid, &status, 0);

            if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP)
            {
                ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
                unsigned long bp_address = regs.rip - 1;
                Breakpoint *bp = lookup_addr(bp_address);

                /* Verify if we actually hit the expected breakpoint */
                if (bp != NULL)
                {
                    /* 1. Rewind RIP back to the original instruction start address */
                    regs.rip = bp->addr;
                    ptrace(PTRACE_SETREGS, child_pid, NULL, &regs);

                    /* 2. Restore the original opcode byte so it can execute safely */
                    ptrace(PTRACE_POKETEXT, child_pid, (void *)bp->addr, (void *)bp->original_instruction);

                    /* Return without resetting breakpoint*/
                    if (!bp->is_active)
                    {
                        ptrace(PTRACE_CONT, child_pid, NULL, NULL);
                        waitpid(child_pid, &status, 0);
                        continue;
                    }

                    printf("Hit breakpoint at 0x%lx!\n", bp->addr);

                    /* 3. To make the breakpoint PERSISTENT: */
                    /* We single-step over the original instruction right now, then re-trap it. */
                    if (ptrace(PTRACE_SINGLESTEP, child_pid, NULL, NULL) >= 0)
                    {
                        waitpid(child_pid, &status, 0);

                        /* Re-enact the INT3 trap so it catches next time the function runs */
                        long trap = (bp->original_instruction & ~0xFF) | 0xCC;
                        ptrace(PTRACE_POKETEXT, child_pid, (void *)bp->addr, (void *)trap);

                        /* Fetch current registers after the secret single-step */
                        ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
                    }

                    printf("Stopped at breakpoint. Ready to step or continue.\n");
                }
                else
                {
                    printf("Target stopped at RIP: 0x%" PRIx64 " (Signal: %d)\n", (uint64_t)regs.rip, WSTOPSIG(status));
                }
            }
            else if (WIFEXITED(status))
            {
                printf("Target program exited cleanly with status %d.\n", WEXITSTATUS(status));
                break;
            }
            else if (WIFSIGNALED(status))
            {
                printf("Target program terminated by signal %d.\n", WTERMSIG(status));
                break;
            }
        }

        else if (input[0] == 's')
        {
            if (ptrace(PTRACE_SINGLESTEP, child_pid, NULL, NULL) < 0)
            {
                perror("PTRACE_SINGLESTEP");
                break;
            }
            waitpid(child_pid, &status, 0);

            if (WIFEXITED(status))
            {
                printf("Target program exited cleanly with status %d.\n", WEXITSTATUS(status));
                break;
            }
            else if (WIFSIGNALED(status))
            {
                printf("Target program terminated by signal %d.\n", WTERMSIG(status));
                break;
            }

            ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
            step_count++;
            printf("Step %d, RIP: 0x%" PRIx64 "\n", step_count, (uint64_t)regs.rip);

            /* Read 16 bytes at RIP for disassembly */
            char code[16];
            long word1 = ptrace(PTRACE_PEEKTEXT, child_pid, (void *)regs.rip, NULL);
            long word2 = ptrace(PTRACE_PEEKTEXT, child_pid, (void *)(regs.rip + 8), NULL);
            memcpy(code, &word1, sizeof(word1));
            memcpy(code + 8, &word2, sizeof(word2));

            int should_quit = 0;
            Breakpoint *bp = lookup_addr(regs.rip);

            if (bp != NULL && bp->is_active)
            {
                memcpy(code, &bp->original_instruction, sizeof(bp->original_instruction));
            }

            size_t count = cs_disasm(capstone_handle, (uint8_t *)code, sizeof(code), regs.rip, 0, &insn);
            if (count > 0)
            {
                printf("0x%" PRIx64 ":\t%s\t%s", insn[0].address, insn[0].mnemonic, insn[0].op_str);
                lookup_dwarf_line(dbg, regs.rip - base_address);
                printf("\n");

                if (strcmp(insn[0].mnemonic, "call") == 0)
                {
                    /* Step over the call: plant INT3 at return address, run to it */
                    unsigned long next_insn_addr = regs.rip + insn[0].size;

                    long original_word = ptrace(PTRACE_PEEKTEXT, child_pid, (void *)next_insn_addr, NULL);
                    long trap_word = (original_word & ~0xFF) | 0xCC;
                    ptrace(PTRACE_POKETEXT, child_pid, (void *)next_insn_addr, (void *)trap_word);

                    ptrace(PTRACE_CONT, child_pid, NULL, NULL);
                    waitpid(child_pid, &status, 0);

                    ptrace(PTRACE_POKETEXT, child_pid, (void *)next_insn_addr, (void *)original_word);

                    struct user_regs_struct post_regs;
                    ptrace(PTRACE_GETREGS, child_pid, NULL, &post_regs);
                    post_regs.rip = next_insn_addr;
                    ptrace(PTRACE_SETREGS, child_pid, NULL, &post_regs);

                    insn = NULL;
                    call_depth++;
                    continue;
                }
                else if (strcmp(insn[0].mnemonic, "ret") == 0)
                {
                    if (call_depth == 0)
                    {
                        printf("Returning from main.\n");
                        should_quit = 1;
                    }
                    else
                    {
                        call_depth--;
                    }
                }

                free(insn);
                insn = NULL;
            }

            if (should_quit)
                break;
        }
    }

    /* Detach and cleanup */
    ptrace(PTRACE_DETACH, child_pid, NULL, NULL);
    if (dbg)
    {
        dwarf_end(dbg);
        close(dwarf_fd);
    }
    cs_close(&capstone_handle);

    return 0;
}
