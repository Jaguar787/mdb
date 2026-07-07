# Micro Debugger

A lightweight C-based debugger for Linux that enables interactive debugging of programs through process tracing and DWARF symbol information.

## Features

- **Breakpoint Management**: Set breakpoints at function names
- **Execution Control**: Continue execution, single-step through instructions
- **Disassembly**: Real-time x86-64 instruction disassembly with Capstone
- **Source-level Debugging**: Display source file and line numbers from DWARF debug info
- **Call Stack Navigation**: Step over function calls while tracking call depth

## Dependencies

The debugger requires the following libraries:

- `libdw` - DWARF debugging information library
- `libelf` - ELF file format library
- `capstone` - Disassembly engine library

### Installing Dependencies

**Debian/Ubuntu:**
```bash
sudo apt-get install libdw-dev libelf-dev libcapstone-dev
```

## Building

```bash
make
```

This will produce two binaries:
- `dbg` - The debugger binary (requires libdw, libelf, capstone)
- `test` - A simple test program

To clean build artifacts:
```bash
make clean
```

## Usage

Launch the debugger with a target program:

```bash
./dbg ./test
```

### Interactive Commands

| Command | Description |
|---------|-------------|
| `b` | Set a breakpoint at a function name |
| `r` | Remove a breakpoint given a function name |
| `c` | Continue execution until the next breakpoint or program exit |
| `s` | Single-step through one instruction |
| `q` | Quit the debugger |

Note: removing a breakpoint is still in development

### Example Session

```
./dbg ./test
Welcome to micro debugger.
The options are:
1. b (breakpoint at a function name)
2. c (continue execution)
3. s (step through instructions)
4. q (quit)
Enter command: b
Enter function name: main
Base address: 0x7f1234567000
Setting breakpoint at main: offset 0x1234, runtime 0x7f1234568234
Breakpoint set at address: 0x7f1234568234
It worked.
Enter command: c
Hit breakpoint at 0x7f1234568234!
Stopped at breakpoint. Ready to step or continue.
Enter command: s
Step 1, RIP: 0x7f1234568235
0x7f1234568235:	push	rbp
  ; test.c:3
```

## Project Structure

```
.
├── dbg.c                    # Main debugger entry point
├── breakpoint_map.c/h       # Breakpoint storage and lookup
├── dwarf_utils.c/h          # DWARF symbol and source line queries
├── find_symbol_address.c/h  # ELF symbol table queries
├── get_child_base_address.c/h # Process base address retrieval
├── test.c                   # Simple test program
├── Makefile                 # Build configuration
└── README.md                # This file
```

## Implementation Details

### Breakpoint System

Breakpoints are implemented using the INT3 instruction (0xCC). When a breakpoint is hit:

1. The original instruction is restored
2. The instruction pointer is rewound
3. The instruction is executed via single-step
4. The INT3 trap is re-installed for persistence

### Symbol Resolution

The debugger resolves symbol addresses in two stages:

1. **Static offset**: Retrieved from the ELF symbol table via `libelf`
2. **Runtime address**: Combined with the process base address (ASLR) obtained via `/proc/<pid>/maps`

### Debug Information

Source-level debugging information is retrieved from DWARF sections using `libdw`, enabling the display of source filenames and line numbers during execution.

## Limitations

- Only supports x86-64 architecture
- Requires binaries compiled with debug symbols (`-g` flag)
- Single-threaded debugging
- No variable inspection