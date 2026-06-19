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

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <program_to_trace>\n", argv[0]);
        return 1;
    }

    csh capstone_handle;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &capstone_handle) != CS_ERR_OK) {
        fprintf(stderr, "Failed to initialize Capstone disassembler\n");
        return 1;
    }

    int dwarf_fd;
    Dwarf *dbg = open_dwarf(argv[1], &dwarf_fd);

    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child: allow tracing then exec target */
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execl(argv[1], argv[1], NULL);
        perror("Failed to execute child process");
        exit(1);
    }

    /* Parent */
    int status;
    struct user_regs_struct regs;

    waitpid(child_pid, &status, 0);

    long offset = find_symbol_address(argv[1], "main");
    unsigned long base_address = get_child_base_address(child_pid);
    unsigned long main_address = base_address + offset;

    printf("Base address of child process: 0x%lx\n", base_address);
    printf("Offset of 'main' function: 0x%lx\n", offset);
    printf("Calculated address of 'main' in child process: 0x%lx\n", main_address);

    long word = ptrace(PTRACE_PEEKTEXT, child_pid, (void *)main_address, NULL);
    long trap = (word & ~0xFF) | 0xCC; /* INT3 */
    ptrace(PTRACE_POKETEXT, child_pid, (void *)main_address, (void *)trap);

    /* Continue until breakpoint */
    ptrace(PTRACE_CONT, child_pid, NULL, NULL);
    waitpid(child_pid, &status, 0);

    /* Restore original instruction and set RIP to main */
    ptrace(PTRACE_POKETEXT, child_pid, (void *)main_address, (void *)word);
    ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
    regs.rip = main_address;
    ptrace(PTRACE_SETREGS, child_pid, NULL, &regs);

    cs_insn *insn = NULL;

    printf("word at main_address: 0x%lx\n", word);
    printf("trap word:            0x%lx\n", trap);

    int call_depth = 0;
    int step_count = 0;

    while (WIFSTOPPED(status)) {
        if (ptrace(PTRACE_GETREGS, child_pid, NULL, &regs) < 0) {
            perror("ptrace GETREGS");
            break;
        }

        step_count++;
        printf("Step %d, RIP: 0x%" PRIx64 "\n", step_count, (uint64_t)regs.rip);

        char code[16];
        long word1 = ptrace(PTRACE_PEEKTEXT, child_pid, (void *)regs.rip, NULL);
        long word2 = ptrace(PTRACE_PEEKTEXT, child_pid, (void *)(regs.rip + 8), NULL);
        memcpy(code, &word1, sizeof(word1));
        memcpy(code + 8, &word2, sizeof(word2));

        size_t count = cs_disasm(capstone_handle, (uint8_t *)code, sizeof(code), regs.rip, 0, &insn);
        if (count > 0) {
            printf("0x%" PRIx64 ":\t%s\t%s", insn[0].address, insn[0].mnemonic, insn[0].op_str);
            lookup_dwarf_line(dbg, regs.rip - base_address);
            printf("\n");
            if (strcmp(insn[0].mnemonic, "call") == 0) {
                // 1. Calculate the address immediately after the call instruction
                unsigned long next_insn_addr = regs.rip + insn[0].size;

                // 2. Read the original byte at that address and plant an INT3 breakpoint
                long original_word = ptrace(PTRACE_PEEKTEXT, child_pid, (void *)next_insn_addr, NULL);
                long trap_word = (original_word & ~0xFF) | 0xCC;
                ptrace(PTRACE_POKETEXT, child_pid, (void *)next_insn_addr, (void *)trap_word);

                // 3. Let the child run at full speed until it hits our breakpoint
                ptrace(PTRACE_CONT, child_pid, NULL, NULL);
                waitpid(child_pid, &status, 0);

                // 4. Clean up: Restore the original code byte
                ptrace(PTRACE_POKETEXT, child_pid, (void *)next_insn_addr, (void *)original_word);

                // 5. Rewind RIP back by 1 byte because the INT3 instruction advanced it
                struct user_regs_struct post_regs;
                ptrace(PTRACE_GETREGS, child_pid, NULL, &post_regs);
                post_regs.rip = next_insn_addr;
                ptrace(PTRACE_SETREGS, child_pid, NULL, &post_regs);

                // Clean up Capstone allocation and continue the loop
                cs_free(insn, count);
                insn = NULL;
                continue;
                call_depth++;
            } else if (strcmp(insn[0].mnemonic, "ret") == 0) {
                if (call_depth == 0) {
                    printf("Returning from main.\n");
                    cs_free(insn, count);
                    break;
                }
                call_depth--;
            }

            cs_free(insn, count);
            insn = NULL;
        }

        if (ptrace(PTRACE_SINGLESTEP, child_pid, NULL, NULL) < 0) {
            perror("PTRACE_SINGLESTEP");
            break;
        }

        waitpid(child_pid, &status, 0);
    }

    printf("Child process has exited at address: 0x%" PRIx64 "\n", (uint64_t)regs.rip);

    /* Detach and cleanup */
    ptrace(PTRACE_DETACH, child_pid, NULL, NULL);
    if (dbg) {
        dwarf_end(dbg);
        close(dwarf_fd);
    }

    cs_close(&capstone_handle);
    return 0;
}