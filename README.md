# Cosmac VIP Emulator

A CDP 1802 CPU emulator for vintage computer systems, including the **COSMAC VIP**, **VP500**, **RCA Studio II**, and **RCA Studio III**. Built with C and SDL2, this emulator provides hardware-accurate emulation with tape I/O, serial console support, and CHIP-8 functionality.

## Architecture

The CDP 1802 is a simple RISC-like architecture featuring:

- **Accumulator (D)** and **carry flag (DF)**
- **16 index registers**, with one acting as the program counter (P register)
- **Default index register (X)** for fast memory access
- **Context saving** via the (T) register during interrupts (saves P and X, sets P=1, X=2)

### Video Display

The VIP used a **CDP 1861** video controller, displaying a screen of **64 pixels wide** by up to **128 pixels high**. Each display line consists of **14 cycles**, with 8 cycles dedicated to DMA memory copies from the address in register R0.

## Building

### Prerequisites

- CMake
- SDL2 (core, mixer, image)
- GCC (Linux) or MSVC (Windows)

### Build Steps

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

### Keyboard Input

**COSMAC VIP**: Single hex keyboard via number pad.

| Key | Function    |
|-----|-------------|
| 0–9 | Digits      |
| A   | `.` (dot)   |
| B   | Enter       |
| C   | `+`         |
| D   | `-`         |
| E   | `*`         |
| F   | `/`         |

**RCA Studio II/III**: Second keypad maps to `0(X)`, `1(A)`, `2(S)`, `3(D)`, `4(Q)`, `5(W)`, `6(E)`, `7(1)`, `8(2)`, `9(3)`.

### Function Keys

| Key | Action |
|-----|--------|
| F1 | Start or stop (reset) |
| F2 | Start tape read |
| F3 | Start tape write |
| F4 | Step one instruction |
| F5 | Start without reset |
| F6 | Quit emulator |

### Command-Line Options

| Option | Description |
|--------|-------------|
| `-b <file>` | Load binary file at address `0x0` (or `0x200` with CHIP-8) |
| `-c` | Load CHIP-8 ROM into lower memory |
| `-d <file>` | Load a memory dump file |
| `-e <type>` | Set emulation target |
| `-h` | Print help |
| `-i` | Trace instruction execution to stderr |
| `-m <K>` | Set memory size in K |
| `-r <file>` | Load binary file at `0x400` (RCA Studio) |
| `-s` | Enable serial console (UT4 monitor) |
| `-t <file>` | Tape read/write file |
| `-1` to `-9` | Scale display by factor |

### Emulation Targets (`-e`)

| Target | System |
|--------|--------|
| `vip` | COSMAC VIP (default) |
| `vp` | COSMAC VIP with color display |
| `studio2` | RCA Studio II |
| `studio3` | RCA Studio III |

### Running a ROM

```bash
cosmac_vip -e vip uvip.rom
```

## Project Structure

```
cosmac/
src/          Emulator source code
  main.c      SDL2 rendering and input
  cpu.c       CDP 1802 CPU implementation
  cpu.h       CPU data structures
  system.c    System peripheral emulation
  disassemble.c Disassembler
  dis1802.c   Instruction decoding
  roms.c      Built-in ROM support
  roms.h      ROM declarations
rom_src/      Assembly sources for built-in ROMs
  uvip.asm    VIP monitor ROM
  ch ip8.asm  CHIP-8 ROM
  ut4v.asm    UT4 monitor ROM
cmake/        CMake find modules for SDL2
CMakeLists.txt Build configuration
```

## Author

Richard Cornwell (rich@sky-visions.com)

## License

MIT License - see [LICENSE](LICENSE) for details.
