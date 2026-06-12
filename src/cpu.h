/*
 * Cosmac VIP CPU Emulation - CDP 1802 Instruction Execution
 *
 * Author:      Richard Cornwell (rich@sky-visions.com)
 * Copyright 2025, Richard Cornwell
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _CPU_H_
#define _CPU_H_
/* Define CPU registers */

/** @brief 16 index registers (R0-R15). R(P) is program counter, R(X) is default
 *  index for register-indirect addressing, R(N) is the instruction-specified
 *  register (lower 4 bits of opcode). */
extern uint16_t regs[16];

/** @brief Accumulator (D) - primary 8-bit data register for ALU operations. */
extern uint8_t D;

/** @brief Data Flag (DF) - carry/borrow flag for arithmetic operations. */
extern uint8_t DF;

/** @brief Program Index register - contains the index of the current program
 *  counter register (R(P)). */
extern uint8_t P;

/** @brief Default Index register - contains the index used for register-indirect
 *  memory operations when not explicitly specified. */
extern uint8_t X;

/** @brief Instruction Index register - set to the lower 4 bits of the fetched
 *  opcode to select which R register to use. */
extern uint8_t N;

/** @brief Temporary register - holds register indices during SAVE (SAV) and
 *  MARK operations; used to save/restore CPU state. */
extern uint8_t T;

/** @brief Q flag - control flag used for serial output sequencing, tape I/O
 *  timing, and request operations. */
extern uint8_t Q;

/** @brief Interrupt enable flag - when set, the CPU acknowledges interrupts.
 *  Cleared on interrupt entry, set by RET/DIS instructions. */
extern uint8_t I;

/** @brief External flags - reserved for external hardware status. */
extern uint8_t EF;

/** @brief Current display scan line (0-261). Driven by DMA timing. */
extern int line;

/** @brief Current column within the display line (1-14). */
extern int col;

/** @brief Current row in the pixel buffer being written by DMA. */
extern int row;

/** @brief Current pixel column position within the active DMA output area. */
extern int dot_pos;

/** @brief Interrupt request flag - set by display timing at frame boundaries.
 *  Cleared each cycle; checked by step() to trigger interrupt service. */
extern uint8_t irq_flag;

/** @brief Display on/off flag - controls whether DMA output draws pixels. */
extern uint8_t display_on;

/** @brief Display status indicator - set when display is active. Used for
 *  conditional branching (B1/BN1). */
extern uint8_t display_status;

/** @brief ROM enable mask - ORed with addresses to map ROM or RAM regions. */
extern uint16_t rom_enable;

/** @brief Total memory size in bytes for the emulated system. */
extern uint16_t memsize;

/** @brief Bit mask applied to addresses to determine which address bits are
 *  valid for memory access. */
extern uint16_t memmask;

/** @brief 32KB emulated system memory (0x0000-0x7FFF RAM, with ROM mirroring
 *  above 0x8000 depending on system type). */
extern uint8_t memory[32*1024];

/** @brief DMA input flag - set when the display controller requests memory
 *  data (not actively used in current implementation). */
extern uint8_t dma_in;

/** @brief DMA output flag - set during active display lines to transfer
 *  memory contents to the pixel buffer. */
extern uint8_t dma_out;

/** @brief CPU idle flag - set by the IDLE instruction (0000xxxx). CPU waits
 *  in idle state until an interrupt, DMA request, or wakeup event occurs. */
extern uint8_t idle;

/** @brief Current key bank selected via OUT 2 instruction. Selects which
 *  element from the key[] array is read during B3/B4 instructions. */
extern int key_select;

/** @brief CPU running state - non-zero when the CPU is actively executing
 *  instructions, zero when halted. */
extern uint8_t running;

/** @brief Total accumulated clock cycles executed by the CPU. */
extern uint64_t cycles;

/** @brief Cycle counter for tape read/write timing relative to program
 *  execution. */
extern int tapecycles;

/** @brief Cycle counter at the last Q flag transition - used for tape I/O
 *  timing calculations. */
extern int lastcycle;

/** @brief Bit position within a tape-recorded byte (-12 to 8). -12 starts
 *  the leader countdown. */
extern int bitcount;

/** @brief Timer for tape phase transitions. Counts cycles since last phase
 *  change for proper tape sync timing. */
extern int bittimer;

/** @brief Tape leader flag - non-zero during the sync leader period before
 *  data bytes are recorded. */
extern uint8_t leader;

/** @brief Tape signal phase (transition point between mark/space frequencies). */
extern uint8_t phase;

/** @brief Accumulator for tape data bytes being read or written. */
extern uint8_t value;

/** @brief Parity bit counter for tape recorded bytes (9-bit parity). */
extern int parity;

/** @brief Current tape bit value being processed (1 for mark, 0 for space). */
extern uint8_t tape_data;

/** @brief Tape read mode flag - non-zero during tape playback. */
extern uint8_t tape_read;

/** @brief Tape write mode flag - non-zero during tape recording. */
extern uint8_t tape_write;

/** @brief Serial output shift register (9 bits) for serial console output.
 *  Q flag is shifted into bit 8 during transmission. */
extern uint16_t serial_out;

/** @brief Last data byte received via serial console input. */
extern uint16_t serial_in;

/** @brief Counter for serial output timing - decrements each cycle, triggering
 *  data shifts at the serial baud rate. */
extern int serial_out_cnt;

/** @brief Counter for serial input timing - decrements each cycle, loading
 *  data at the receive baud rate. */
extern int serial_in_cnt;

/** @brief Type of system being emulated */
extern int system_type;

/** @brief Color support flag - non-zero when emulating a system with color
 *  display capabilities (VP, RCA Studio). */
extern int color;

/** @brief Current background color index (0-3). */
extern int bg_color;

/** @brief Color display is currently enabled */
extern int color_en;

/** @brief Color map entry for the given index. Maps display pixels to RGB
 *  values for color output. */
extern int pallet[256];

/** @brief Background color pixel mapping - converts color index to display
 *  pixel values. */
extern int bg_map[4];

/** @brief Cartridge loaded flag - non-zero when a cartridge ROM is present
 *  in RCA Studio systems (0x0400-0x07FF). */
extern int cartridge;

/** @brief System type identifier - selects address decoding rules and
 *  peripheral behavior for the emulated machine. */

#define VIP          0    /**< COSMAC VIP - standard black & white system */
#define VP           1    /**< VP - COSMAC VIP with color expansion */
#define RCA_STUDIO2  2    /**< RCA Studio II - home video game console */
#define RCA_STUDIO3  3    /**< RCA Studio III - later home console variant */

/**
 * @brief Get the total number of CPU cycles executed since initialization.
 *
 * @return Total cycle count accumulated by the CPU.
 */
uint64_t get_cycles();

/**
 * @brief Reset the CPU and all emulated peripherals to their initial power-on
 *  state. This includes zeroing all registers, clearing DMA/tape flags,
 *  setting default ROM mapping (0x8000), and filling unprogrammed memory
 *  with 0xFF.
 *
 * Called on system reset (F1 key) and at startup. Initializes timing
 * counters, display state, and serial console state.
 */
void reset();

/**
 * @brief Adjust the main cycle counter by subtracting a specified number of
 *  cycles. Used to compensate for timing discrepancies in the emulator.
 *
 * @param max_cycles Number of cycles to subtract from the running total.
 */
void reset_cycles(int max_cycles);

/**
 * @brief Enable the video display and initialize the display state to the
 *  start of the first scan line. Sets display_on, line, and col registers.
 *
 * Called when the INP 1 instruction is executed to turn the display on.
 */
void set_display_on();

/**
 * @brief Read a byte from memory at the address specified by register R plus
 *  an offset, without advancing the CPU cycle counter.
 *
 * Handles address space decoding for all supported systems:
 * - VIP/VP: Maps addresses above 0x8000 to ROM data (or UT4 monitor data)
 * - RCA Studio: Maps address banks for ROM, cartridge, and RAM regions
 *
 * For VIP/VP with serial console, reads from UT4 monitor ROM data. For
 * standard VIP/VP, reads from the base ROM data image. For RCA Studio
 * systems, checks the high nibble of the address to select the correct
 * memory region (ROM, cartridge, RAM, or returns 0xFF for unconfigured).
 *
 * @param r Index of the register containing the base address (0-15).
 * @param add Offset added to the register value to form the final address.
 * @return The 8-bit value stored at the specified memory address.
 */
uint8_t mem_read_nocycle(uint8_t r, int add);

/**
 * @brief Read a byte from memory at the address specified by register R,
 *  advancing the CPU cycle counter before the read.
 *
 * This is the standard memory read used during instruction execution. The
 * cycle() call accounts for display timing, tape I/O updates, serial
 * console timing, and DMA state transitions before the actual memory access.
 *
 * @param r Index of the register containing the address (0-15).
 * @return The 8-bit value stored at memory[regs[r]].
 */
uint8_t mem_read(uint8_t r);

/**
 * @brief Read a byte from memory at the address specified by register R,
 *  then automatically increment the register as a post-increment side effect.
 *
 * Used for sequential memory access such as instruction fetch and
 * load-accumulator operations where the register must advance after reading.
 *
 * @param r Index of the register containing the address (0-15).
 * @return The 8-bit value stored at memory[regs[r]], then regs[r] is
 *         incremented by 1.
 */
uint8_t mem_read_adv(uint8_t r);

/**
 * @brief Fetch the next instruction byte from R(P).
 *
 * Combines cycle advancement and memory read with auto-increment. This is
 * the primary method of instruction fetch during CPU step execution.
 *
 * @return The 8-bit opcode byte at address regs[P], then regs[P] is
 *         incremented.
 */
uint8_t fetch();

/**
 * @brief Write a byte to memory at the address specified by register R.
 *
 * Handles address space decoding for all supported systems:
 * - VIP/VP: Writes to RAM below 0x8000; handles color palette writes when
 *   in the VP color expansion region (0xC000+), storing the color palette
 *   index into the pallet[] array.
 * - RCA Studio: Writes to RAM at 0x800-0x8FF and color map at 0xB00-0xB3F
 *   for color expansion support.
 *
 * @param r Index of the register containing the address (0-15).
 * @param data The 8-bit value to store at the specified memory address.
 */
void mem_write(uint8_t r, uint8_t data);

/**
 * @brief Write a byte to memory at the address specified by register R,
 *  then decrement the register as a side effect.
 *
 * Used for stack-like push operations where the register is decremented
 * after writing. Implements the STXD pattern in 1802 register-indirect
 * addressing.
 *
 * @param r Index of the register containing the address (0-15).
 * @param data The 8-bit value to store at memory[regs[r]], then regs[r]
 *        is decremented by 1.
 */
void mem_write_back(uint8_t r, uint8_t data);

/**
 * @brief Start the CPU - set the running state to active.
 *
 * When running, the CPU will execute instructions via step(). When not
 * running, step() still advances the cycle counter for display/timer
 * purposes but does not execute instructions.
 */
void run();

/**
 * @brief Halt the CPU - set the running state to stopped.
 *
 * When stopped, step() advances timing counters but skips instruction
 * execution. The CPU can be woken from idle state by interrupts or DMA.
 */
void stop();

/**
 * @brief Advance the CPU clock by one cycle and update all display/timing state.
 *
 * Performs the following each cycle:
 * 1. Increments the main cycle counter and tape cycle counter
 * 2. Increments the bit timer for tape I/O timing
 * 3. If serial console is active, handles serial transmit/receive timing
 *    for the serial_in and serial_out shift registers
 * 4. Calculates display state based on scan line position:
 *    - Lines 76-79: Display status region (4 lines before active display)
 *    - Lines 78-79: Pre-interrupt region (interrupts fired 2 lines
 *      before display starts)
 *    - Lines 206-210: Post-display region (display status at end)
 *    - Lines 80-206, columns 2-10: Active DMA output area
 * 5. Advances column and line counters, wrapping at frame boundaries
 * 6. Calls draw_screen() at end of each frame (line 262)
 *
 * The display timing follows the CDP 1861 VDC specification:
 * - 80 blank lines before display
 * - 127 active display lines
 * - 55 blank lines after display
 * Total: 262 lines per frame.
 */
void cycle();

/**
 * @brief Process one DMA output cycle - copy a byte from R(0) to the display.
 *
 * Reads a byte from memory via R(0), then converts each bit (MSB first)
 * into a pixel column. Bright bits (bit set) draw visible pixels, while
 * dark bits may render as background color depending on the color palette
 * configuration. Applies the color palette lookup if color mode is enabled.
 *
 * After the byte is processed, R(0) is auto-incremented. The dot_pos
 * counter tracks the current pixel column, advancing until it reaches 64,
 * at which point DMA output is disabled for that line.
 */
void dma_out_cycle();

/**
 * @brief Process one DMA input cycle.
 *
 * Currently a no-op. DMA input (read from display controller) is not
 * supported in the CDP 1861 VDC used by the COSMAC VIP. Future support
 * may require reading the display buffer back into memory.
 */
void dma_in_cycle();

/**
 * @brief Execute one machine instruction and return.
 *
 * The instruction execution flow:
 * 1. If not running, advance cycle counter and return (idle timing only)
 * 2. If in idle state, check for wakeup conditions (interrupt, DMA)
 * 3. Handle pending DMA output or input cycles
 * 4. If still idle after wakeup check, just cycle and return
 * 5. Check for interrupt - if I=1 and irq_flag set, save R(P)+R(X) to R(T),
 *    set P=1, X=2, clear I, cycle and return (interrupt takes one cycle)
 * 6. Fetch instruction via fetch() and decode by grouping bits:
 *    - Upper nibble (bits 7-4): opcode group
 *    - Lower nibble (bits 3-0): register index N or opcode modifier
 * 7. Execute the decoded instruction:
 *    - Group 0 (0000xxxx): IDLE (00) or LDN (0N) - idle or load D
 *    - Group 1 (0001xxxx): INC - increment R(N)
 *    - Group 2 (0010xxxx): DEC - decrement R(N)
 *    - Group 3 (0011xxxx): Branch - conditional or unconditional branch
 *      with various conditions: BR/NBR, BQ/BNQ, BZ/BNZ, BDF/BNF,
 *      B1/BN1, B2/BN2, B3/BN3, B4/BN4
 *    - Group 4 (0100xxxx): LDA - load D from R(N) with auto-increment
 *    - Group 5 (0101xxxx): STR - store D to R(N)
 *    - Group 6 (0110XWWW): IN/OUT instructions - register operations,
 *      peripheral I/O (display, key select, ROM control, keyboard input)
 *    - Group 7 (0111xxxx): Operator group 1 - RET, DIS, LDXA, STXD, ADC, SDB,
 *      SHRC, SMB, SAV, MARK, REQ, SEQ, ADCI, SDBI, SHLC, SMBI
 *    - Group 8 (1000xxxx): GLO - load low byte of R(N) into D
 *    - Group 9 (1001xxxx): GHI - load high byte of R(N) into D
 *    - Group A (1010xxxx): PLO - store low byte of D into R(N)
 *    - Group B (1011xxxx): PHI - store high byte of D into R(N)
 *    - Group C (1100xxxx): Long branch/skip (0xCB prefix) - LBR, LBQ,
 *      LBZ, LBDF, LSNQ, LSNZ, LSNF, LSKP, LBNQ, LBNZ, LBNF, LSIE,
 *      LSQ, LSZ, LSDF
 *    - Group D (1101xxxx): SEP - set program index register to N
 *    - Group E (1110xxxx): SEX - set index register to N
 *    - Group F (1111xxxx): Operator group 2 - LDX, OR, AND, XOR, ADD,
 *      SD, SHR, SM, LDI, ORI, ANI, XRI, ADI, SDI, SHL, SMI
 * 8. After instruction execution, check again for pending interrupt.
 *
 * The 1802 uses register-indirect addressing where the lower 4 bits of the
 * opcode select the R register, and the upper 4 bits determine the opcode
 * group. Many instructions use R(N) where N is determined by instruction
 * context or previous operations.
 *
 */
void step();

#endif
