/*
 * 1802 - CPU instruction execution and dis-assembler
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
 * Implements the complete CDP 1802 instruction set, clock cycle timing,
 * memory management with address space decoding for multiple systems
 * (VIP, VP, RCA Studio II, RCA Studio III), DMA display output, tape
 * I/O with mark/space frequency shift keying, and serial console support.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
*/

/**
 * @file cpu.c
 * @brief CDP 1802 CPU core implementation for Cosmac VIP emulation.
 *
 * Implements the complete CDP 1802 instruction set, clock cycle timing,
 * memory management with address space decoding for multiple systems
 * (VIP, VP, RCA Studio II, RCA Studio III), DMA display output, tape
 * I/O with mark/space frequency shift keying, and serial console support.
 *
 * The CDP 1802 is a register-indirect CPU with 16 universal 16-bit index
 * registers (R0-R15), an 8-bit accumulator (D), and a data flag (DF).
 * The program counter and default index register are selected from the
 * index registers via the P and X registers respectively.
 *
 * @author Richard Cornwell (rich@sky-visions.com)
 * @copyright 2025 Richard Cornwell
 */

#include <stdio.h>
#include <string.h>
#include "system.h"
#include "cpu.h"
#include "roms.h"


/** @brief 16 index registers (R0-R15). R(P) is program counter, R(X) is default
 *  index for register-indirect addressing, R(N) is the instruction-specified
 *  register (lower 4 bits of opcode). */
uint16_t   regs[16];


/** @brief Accumulator (D) - primary 8-bit data register for ALU operations. */
uint8_t    D;


/** @brief Data Flag (DF) - carry/borrow flag for arithmetic operations. */
uint8_t    DF;

/** @brief External Flags (EF) - used during testing to handle values of EF flags. */
uint8_t    EF;

/** @brief Program Index register - contains the index of the current program
 *  counter register (R(P)). */
uint8_t    P;


/** @brief Default Index register - contains the index used for register-indirect
 *  memory operations when not explicitly specified. */
uint8_t    X;


/** @brief Instruction Index register - set to the lower 4 bits of the fetched
 *  opcode to select which R register to use. */
uint8_t    N;


/** @brief Temporary register - holds register indices during SAVE (SAV) and
 *  MARK operations; used to save/restore CPU state. */
uint8_t    T;


/** @brief Q flag - control flag used for serial output sequencing, tape I/O
 *  timing, and request operations. */
uint8_t    Q;


/** @brief Interrupt enable flag - when set, the CPU acknowledges interrupts.
 *  Cleared on interrupt entry, set by RET/DIS instructions. */
uint8_t    I;


/** @brief Interrupt request flag - set by display timing at frame boundaries.
 *  Cleared each cycle; checked by step() to trigger interrupt service. */
uint8_t    irq_flag;


/** @brief Current display scan line (0-261). Driven by DMA timing. */
int        line;


/** @brief Current column within the display line (1-14). */
int        col;


/** @brief Current row in the pixel buffer being written by DMA. */
int        row;


/** @brief Current pixel column position within the active DMA output area. */
int        dot_pos;

/** @brief Display on/off flag - controls whether DMA output draws pixels. */
uint8_t    display_on;


/** @brief Display status indicator - set when display is active. Used for
 *  conditional branching (B1/BN1). */
uint8_t    display_status;


/** @brief ROM enable mask - ORed with addresses to map ROM or RAM regions. */
uint16_t   rom_enable;


/** @brief Total memory size in bytes for the emulated system. */
uint16_t   memsize;


/** @brief Bit mask applied to addresses to determine which address bits are
 *  valid for memory access. */
uint16_t   memmask;


/** @brief 32KB emulated system memory (0x0000-0x7FFF RAM, with ROM mirroring
 *  above 0x8000 depending on system type). */
uint8_t    memory[32*1024];


/** @brief DMA input flag - set when the display controller requests memory
 *  data (not actively used in current implementation). */
uint8_t    dma_in;


/** @brief DMA output flag - set during active display lines to transfer
 *  memory contents to the pixel buffer. */
uint8_t    dma_out;


/** @brief CPU idle flag - set by the IDLE instruction (0000xxxx). CPU waits
 *  in idle state until an interrupt, DMA request, or wakeup event occurs. */
uint8_t    idle;


/** @brief Current key bank selected via OUT 2 instruction. Selects which
 *  element from the key[] array is read during B3/B4 instructions. */
int        key_select;


/** @brief CPU running state - non-zero when the CPU is actively executing
 *  instructions, zero when halted. */
uint8_t    running;


/** @brief Total accumulated clock cycles executed by the CPU. */
uint64_t   cycles;


/** @brief Cycle counter for tape read/write timing relative to program
 *  execution. */
int        tapecycles;


/** @brief Cycle counter at the last Q flag transition - used for tape I/O
 *  timing calculations. */
int        lastcycle;


/** @brief Bit position within a tape-recorded byte (-12 to 8). -12 starts
 *  the leader countdown. */
int        bitcount;


/** @brief Timer for tape phase transitions. Counts cycles since last phase
 *  change for proper tape sync timing. */
int        bittimer;

/** @brief Tape leader flag - non-zero during the sync leader period before
 *  data bytes are recorded. */
uint8_t    leader;

/** @brief Tape signal phase (transition point between mark/space frequencies). */
uint8_t    phase;

/** @brief Accumulator for tape data bytes being read or written. */
uint8_t    value;

/** @brief Parity bit counter for tape recorded bytes (9-bit parity). */
int        parity;

/** @brief Current tape bit value being processed (1 for mark, 0 for space). */
uint8_t    tape_data;

/** @brief Tape read mode flag - non-zero during tape playback. */
uint8_t    tape_read;

/** @brief Tape write mode flag - non-zero during tape recording. */
uint8_t    tape_write;

/** @brief Serial output shift register (9 bits) for serial console output.
 *  Q flag is shifted into bit 8 during transmission. */
uint16_t   serial_out;

/** @brief Last data byte received via serial console input. */
uint16_t   serial_in;

/** @brief Counter for serial output timing - decrements each cycle, triggering
 *  data shifts at the serial baud rate. */
int        serial_out_cnt;

/** @brief Counter for serial input timing - decrements each cycle, loading
 *  data at the receive baud rate. */
int        serial_in_cnt;

/** @brief Type of system being emulated */
int       system_type;

/** @brief Color support flag - non-zero when emulating a system with color
 *  display capabilities (VP, RCA Studio). */
int        color;

/** @brief Current background color index (0-3). */
int        bg_color;

/** @brief Color display is currently enabled */
int       color_en;

/** @brief Color map entry for the given index. Maps display pixels to RGB
 *  values for color output. */
int        pallet[256];

/** @brief Background color pixel mapping - converts color index to display
 *  pixel values. Background rotate: 2 -> 0 -> 4 -> 1 -> 2 */
int        bg_map[4] = { 2, 0, 4, 1 };

/** @brief Cartridge loaded flag - non-zero when a cartridge ROM is present
 *  in RCA Studio systems (0x0400-0x07FF). */
int        cartridge = 0;

/**
 * @brief Return number of cycles executed.
 *
 * @return cycles.
 */
uint64_t
get_cycles()
{
    return cycles;
}

/**
 * @brief Reset CPU to default state.
 */
void
reset()
{
    running = 0;
    rom_enable = 0x8000;   /* Force all addresses to access rom */
    I = 1;
    N = 0;
    Q = 0;
    X = 0;
    P = 0;
    D = 0;
    T = 0;
    DF = 0;
    regs[0] = 0;
    dma_in = 0;
    dma_out = 0;
    irq_flag = 0;
    display_on = 0;
    display_status = 0;
    bitcount = -1;
    bittimer = 0;
    value = 0;
    line = 0;
    dot_pos = 0;
    col = 0;
    row = 0;
    idle = 0;
    key_select = 0;
    cycles = 0;
    lastcycle = 0;
    tapecycles = 0;
    bittimer = 0;
    phase = 0;
    tape_read = 0;
    tape_data = 0;
    parity = 0;
    leader = 0;
    bg_color = 0;
    color_en = 0;
}

/**
 * @brief Reset number of cycles run.
 *
 *
 * @param max_cycles Maximum number of cycles we should have run.
 */
void
reset_cycles(int max_cycles) {
    cycles = cycles - max_cycles;
}

/**
 * @brief Turn display on, reset line and column to one.
 */
void
set_display_on() {
    line = 1;
    col = 1;
    display_on = 1;
}

/**
 * @brief Read from memory location without causing cycle.
 *
 * @param r is register to use to select memory location.
 * @param add increase address by amount.
 * @return Value of memory location.
 *
 * Address to ranges:
 * Cosmac VIP:
 * 0000-0FFF     RAM   4K
 * 8x00-81FF     ROM   512.
 *     OUT 1     61    Display off.
 *     OUT 2     62    Select key.
 *     OUT 3     63    nop
 *     0UT 4     64    Rom disable
 *     0UT 5     65    Rom disable
 *     0UT 6     66    Rom disable
 *     0UT 7     67    Rom disable
 *
 *     IN  1     69    nop
 *     IN  2     6a    Display enable
 *     IN  3     6b    nop
 *     IN  4     6c    nop
 *     IN  5     6d    nop
 *     IN  6     6e    nop
 *     IN  7     6f    nop
 *
 * UT4:
 * 0000-0FFF     RAM   4K
 * 8x00-81FF     ROM   512
 * 8C00-8C1F     RAM   32
 *
 * Color expansion:
 * Cx00-CxFF     RAM   256 Color map
 * Dx00-DxFF     RAM   256 Color Map high resolution
 *               OUT   65  Switch color.
 *
 * RCA Studio II
 * 0000-03FF     ROM   1K Chip 8
 * 0400-07FF     ROM Cartridge.
 * 0800-08FF     RAM.  256
 * 0900-09FF     RAM   256 Video display
 * 0b00-0b3f     RAM   64 Color map.
 *     OUT 1     61    Display off.
 *     OUT 2     62    Select key.
 *     OUT 3     63    nop
 *     0UT 4     64    nop
 *     0UT 5     65    nop
 *     0UT 6     66    nop
 *     0UT 7     67    nop
 *
 *     IN  1     69    Enable display
 *     IN  2     6a    Display enable
 *     IN  3     6b    nop
 *     IN  4     6c    nop
 *     IN  5     6d    nop
 *     IN  6     6e    nop
 *     IN  7     6f    nop
 *       OUT 1   61    Rotate color.
 *       OUT 4   64    Tone generator.
 *       OUT 5   65
 *       EF3           Keypad Left
 *       EF4           Keypad Right
 *
 * When running test cartridge, 0x400-0x7ff is duplicated at 0x4000-0x43ff.
 * The valid copy of system rom is at 0x2000 to 0x27ff.
 */

inline uint8_t
mem_read_nocycle(uint8_t r, int add)
{
    uint16_t addr = regs[r] + add;
    switch (system_type) {
    case VIP:
    case VP:
             addr |= rom_enable;

             /* Anything below 0x8000 is ram. */
             if ((addr & 0x8000) != 0) {
                 if ((addr & 0xfe00) != 0x8000) {
                     return 0xff;
                 }
                 if (serial) {
                    return ut4_data[addr & 0x01ff];
                 }
                 return rom_data[addr & 0x01ff];
             }
             break;

   case RCA_STUDIO2:
   case RCA_STUDIO3:
             switch ((addr >> 8) & 0xff) {
             /* System interpreter rom */
             case 0x0: case 0x1: case 0x2: case 0x3:
                      return rca_studio_data[addr & 0x7ff];

             case 0x4: case 0x5: case 0x6: case 0x7:
                       if (!cartridge || rom_enable) {
                          return rca_studio_data[addr & 0x7ff];
                       }
                       return memory[addr & 0x7ff];

             default:
                       return memory[addr & 0x7fff];
             }
             break;
    }
    return memory[addr & memmask];
}

/**
 * @brief Read from memory location and cycle to next cycle.
 *
 * Use R to select register to read memory from.
 *
 * @param r is register to use to select memory location.
 * @return Value of memory location.
 */
inline uint8_t
mem_read(uint8_t r)
{
    uint16_t addr = regs[r];
    cycle();

    switch (system_type) {
    case VIP:
    case VP:
             break;

   case RCA_STUDIO2:
   case RCA_STUDIO3:
             rom_enable = 0;
             if ((addr & 0xfc00) == 0x4000) {
                 /* When running from mirror rom, access to lower
                  * memory is enabled for next cycle.
                  * This simulates the effect of the CD4013 flip
                  * flop.
                  *
                  * When enabled, next access to cartridge area
                  * will select system roms.
                  */
                 rom_enable = 1;
             }
             break;
    }
    return mem_read_nocycle(r, 0);
}


/**
 * @brief Read from memory, advance register after.
 *
 * @param r is register to use to select memory location.
 * @return Value of memory location.
 */
inline uint8_t
mem_read_adv(uint8_t r)
{
    uint8_t t = mem_read(r);
    regs[r]++;
    return t;
}

/**
 * @brief Fetches the next instruction.
 * @return Value of memory location.
 */
inline uint8_t
fetch()
{
    return mem_read_adv(P);
}

/**
 * @brief Write memory location.
 * @param r Register to write get address of memory location to set.
 * @param data Data to store in memory.`
 */
inline void
mem_write(uint8_t r, uint8_t data)
{
    uint16_t addr = regs[r];;

    cycle();

    switch (system_type) {
    case VIP:
    case VP:
             addr |= rom_enable;
             if ((addr & 0x8000) == 0) {
                memory[addr&memmask] = data;
             } else {
                if (system_type == VP && (addr & 0xc000) == 0xc000) {
                    color_en = (addr & 0x1000) ? 0xff : 0xc0;
                    pallet[addr & color_en] = data & 0x7;
                }
             }
             break;

   case RCA_STUDIO2:
   case RCA_STUDIO3:
             switch ((addr >> 8) & 0xff) {
             case 0xb:       /* Address color pallet, also enable color support */
                       color_en = 0xff;
                       pallet[addr & 0x3f] = data & 0x7;
                       break;

             case 0x8:      /* Address on board RAM, due to decoding, */
             case 0x9:      /*  0x8 and 0xc are same memory */
             case 0xc:
             case 0xd:
                       memory[addr & memmask] = data;
                       break;
             default:
                       break;

             }
    }
}

/**
 * @brief Write memory location, then decrement R(P).
 * @param r Register to write.
 * @param data Data to stop at memory location.
 */
inline void
mem_write_back(uint8_t r, uint8_t data)
{
    mem_write(r, data);
    regs[r]--;
}

/**
 * @brief Set CPU to running and not halted.
 */
void
run()
{
    running = 1;
};

/**
 * @brief Set CPU to running and not halted.
 */
void
stop()
{
    running = 0;
};

/**
 * @brief Start tape reading.
 */
void
taperead()
{
    tape_read = 1;
    bitcount = -12*256; /* Set size of leader. */
    leader = 1;
}

/**
 * @brief Start tape writing..
 */
void
tapewrite()
{
    tape_write = 1;
    bitcount = -12*256; /* Set size of leader. */
}

/*typedef enum { OPR, OPN, OPB, OPO, OPI, OPL} opcode_type; */

/**
 * @brief Preform addition operation.
 *
 * Add two numbers together plus flag if needed. Save
 * results in D and DF.
 *
 * @param dreg Value of D register passed to addition.
 * @param value Value to add to D register.
 * @param flag DF or constant flag.
 */
static void inline
add_op(uint8_t dreg, uint8_t value, uint8_t flag)
{
    D = dreg + value + flag;
    DF = !!(((dreg & value) | ((dreg ^ value) & ~D)) & 0x80);
}

/**
 * @brief Process a cpu cycle. Update the display postion.
 *
 * Count lines on screen, 80 lines of blanks. 128 lines of display.
 * Followed by 55 lines of blanks. 4 lines before the display starts,
 * display status is starting. 2 lines before interrupt is set. 4
 * lines before the end of screen display status is set.
 *
 * If serial I/O is enabled, count timeout and build serial character.
 * For output continuely append Q flag to bit 9 of output byte. First
 * bit will be start bit. When there is a 1 in bit 0 we have character.
 * Send it and clear for next character.
 *
 * For input, continue to shift serial input character right. Detected
 * by looking at bit 0 of EF2.
 *
 */
void
cycle()
{
    cycles++;
    tapecycles++;
    bittimer++;

    if (serial) {
        if (serial_in_cnt == 0) {
            serial_in >>= 1;
            serial_in_cnt = 1000;
        } else {
            serial_in_cnt--;
        }
        if (serial_out_cnt == 0) {
            serial_out >>= 1;
            if (Q) {
                serial_out |= 0x0100;
            }
            if (serial_out & 1) {
                write_console((serial_out >> 1) ^ 0xff);
                serial_out = 0;
            }
            serial_out_cnt = 1000;
        } else {
            serial_out_cnt--;
        }
    }

    /* Clear interrupt and display status. */
    display_status = 0;
    irq_flag = 0;
    /* When to show display status, and post interrupt before first line. */
    if (line >= 76 && line <= 79) {
        display_status = display_on;
        /* Interrupt before frame starts and ends. */
        if (line >= 78) {
            irq_flag = display_on;
        }
        row = -1;
    /* Just before end of screen */
    } else if(line >= 206 && line <= 210) {
        display_status = display_on;
    /* Main display area. */
    } else if (line >= 80 && line <= 206 && col >= 2 && col <= 10) {
        if (display_on) {
            dma_out = 1;
        } else {
            /* If not displaying, clear row. */
            for (int i = 0; i < 8; i++) {
                draw_pixel(0, row, dot_pos++);
            }
       }
    } else {
       dma_out = 0;
    }

    /* Turn DMA off if hit 64 dots. */
    if (dot_pos == 64) {
       dma_out = 0;
    }

    col++;
    /* Bump column clock */
    if (col == 15) {
        col = 1;
        dot_pos = 0;
        line++;
        row++;
        /* Check if at end of screen */
        if (line == 262) {
            /* Draw screen and set line to start of screen. */
            draw_screen();
            line = 0;
        }
    }
}

/**
 * @brief handle DMA out cycles.
 *
 * For DMA output copy each each memory location to the display.
 */
void
dma_out_cycle()
{
     uint8_t t = mem_read_adv(0);

     /* On DMA output, copy each bit to display memory */
     for (uint8_t m = 0x80; m != 0; m >>= 1) {
         uint8_t value = (t & m) ? 0x7 : 0;

         if (color_en) {
             if (value != 0) {
                 value = pallet[color_en & (regs[0]-1)];
             } else {
                 value = bg_map[bg_color];
             }
         } else if (color && value == 0) {
             value = bg_map[bg_color];
         }
         draw_pixel(value, row, dot_pos);
         dot_pos++;
    }
}

/**
 * @brief handle DMA in cycles.
 *
 * For the moment there is never generation of DMA in signals.
 */
void
dma_in_cycle()
{
}

/**
 * @brief execute one instruction.
 *
 * Step the CPU by one instruction. If CPU is not running, let everybody
 * know that a clock has occurred. Then return right away. If CPU is in
 * stopped state, preform an internal cycle, and read the Joypad to
 * see if any buttons are pressed, at which point we either enter
 * the halt set and take an interrupt. When halted check if there
 * is an interrupt ready, if so exit halt state.
 *
 * Next we clear the interrupt enable hold-off flag to allow interrupts
 * to occur. If CPU is halted, just do internal cycle. Otherwise fetch
 * next byte and decode it with a switch statement. This will result in
 * fetching registers and possibly calling a routine to handle the opcode.
 * For 0313 (0xCB) the opcode is two bytes, so fetch second and call second()
 * to decode it. Lastly check to see if an interrupt is pending, if
 * so let do_irq() save state and set up for next routine.
 */
void
step()
{
    uint8_t     flag = 0;
    uint8_t     temp;
    uint16_t    word;

    if (!running) {
        cycles++;
        return;
    }

    /* If halted if any interrupts pending, exit halt state */
    if (idle) {
        if ((I & irq_flag) != 0 || dma_out || dma_in) {
            idle = 0;
        }
    }

    /* If DMA flag set, handle it. */
    if (dma_out) {
       dma_out_cycle();
       return;
    }

    /* If DMA out flag set, handle it. */
    if (dma_in) {
       dma_in_cycle();
       return;
    }

    /* During Idle instruction just sit idle. */
    if (idle) {
        cycle();
        return;
    }

    /* Check if interrupt pending. */
    if (I && irq_flag) {
       T = (X << 4) | P;
       P = 1;
       X = 2;
       I = 0;
       cycle();
       return;
    }

    /* Decode instruction */
    uint8_t ir = fetch();

    N = ir & 0xf;
    switch((ir >> 4) & 0xf) {
    case 0x0:  /* IDLE & LDN */
               if (N == 0) {
                   idle = 1;
               } else {
                   D = mem_read(N);
               }
               break;

    case 0x1:  /* INC */
               regs[N]++;
               cycle();
               break;

    case 0x2:  /* DEC */
               regs[N]--;
               cycle();
               break;

    case 0x3:  /* branch */
               switch (N) {
               case 0x0:   /* BR */
               case 0x8:   /* NBR */
                           flag = 1;
                           break;

               case 0x1:   /* BQ */
               case 0x9:   /* BNQ */
                           flag = Q;
                           break;

               case 0x2:   /* BZ */
               case 0xA:   /* BNZ */
                           flag = (D==0);
                           break;

               case 0x3:   /* BDF */
               case 0xB:   /* BNF */
                           flag = DF;
                           break;

               case 0x4:   /* B1 */
               case 0xC:   /* BN1 */
                           if (EF & 0x1) {   /* Used for testing */
                               flag = 1;
                               break;
                           }
                           flag = display_status;
                           break;

               case 0x5:   /* B2 */
               case 0xD:   /* BN2 */
                           if (EF & 0x2) {   /* Used for testing */
                               flag = 1;
                               break;
                           }

                           if (serial) {
                               flag = !(serial_in & 1);
                               break;
                           }
                           if (bittimer > 200) {   /* If we were not checking tape in, clear timer. */
                               bittimer = 0;
                           }
                           flag = !phase;
                           /* Time how long to keep value high/low. */
                           if (tape_data) {
                               if (bittimer > 133) {
                                   phase = !phase;
                                   bittimer = 0;
                               }
                           } else {
                               if (bittimer > 45) {
                                   phase = !phase;
                                   bittimer = 0;
                               }
                           }

                           /* Start of bit */
                           if (tape_read && phase && bittimer == 0) {
                               switch (bitcount) {
                               default:
                                          bitcount++;
                                          break;
                               case -1:   /* Start bit, grab next value. */
                                          tape_data = 1;
                                          value = tape_read_byte();
                                          printf("Read %02x\n", value);
                                          parity = 9;
                                          bitcount++;
                                          break;
                               case 0:
                               case 1:
                               case 2:
                               case 3:
                               case 4:
                               case 5:
                               case 6:
                               case 7:
                                          tape_data = (value & 0x01) != 0;
                                          value >>= 1;
                                          bitcount++;
                                          parity += tape_data;
                                          break;
                               case 8:
                                          tape_data = (parity & 1) == 0;
                                          bitcount = -1;
                                          break;
                               }
                           }
                           break;

               case 0x6:   /* B3 */
               case 0xE:   /* BN3 */
                           if (EF & 0x4) {   /* Used for testing */
                               flag = 1;
                               break;
                           }

                           flag = key[key_select];
                           break;

               case 0x7:   /* B4 */
               case 0xF:   /* BN4 */
                           if (EF & 0x8) {   /* Used for testing */
                               flag = 1;
                               break;
                           }

                           if (system_type == RCA_STUDIO2) {
                               flag = key2[key_select];
                           } else {
                               flag = 0;
                           }
                           break;

               }

               /* If opcode >= 8 reverse flag value. */
               flag ^= ((N >> 3) & 1);

               word = (uint16_t)fetch();
               /* If flag true, branch to location on same page. */
               if (flag) {
                   regs[P] = (regs[P] & 0xff00) | word;
               }
               break;

    case 0x4:  /* LDA */
               D = mem_read_adv(N);
               break;

    case 0x5:  /* STR */
               mem_write(N, D);
               break;

    case 0x6:  /* Process input and output instructions. */
               switch (N) {
               case 0x0:   /* IRX */
                           (void)mem_read_adv(X);
                           break;

               case 0x1:   /* OUT 1 */
                           (void)mem_read_adv(X);
                           switch(system_type) {
                           case VIP:
                           case VP:
                               display_on = 0;
                               break;

                           case RCA_STUDIO2:
                           case RCA_STUDIO3:
                               bg_color = (bg_color + 1) & 0x3;
                               break;
                           }
                           break;

               case 0x2:   /* OUT 2 */
                           /* Keyboard select */
                           key_select = mem_read_adv(X) & 0xf;
                           break;

               case 0x3:   /* OUT 3 */
                           /* Output port; */
                           (void)mem_read_adv(X);
                           break;

               case 0x4:   /* OUT 4 */
                           (void)mem_read_adv(X);
                           rom_enable = 0;
                           break;

               case 0x5:   /* OUT 5 */
                           (void)mem_read_adv(X);
                           bg_color = (bg_color + 1) & 0x3;
                           rom_enable = 0;
                           break;

               case 0x6:   /* OUT 6 */
                           (void)mem_read_adv(X);
                           rom_enable = 0;
                           break;

               case 0x7:   /* OUT 7 */
                           (void)mem_read_adv(X);
                           rom_enable = 0;
                           break;

               case 0x8:   /* NOP */
               case 0xA:   /* INP 2 */
               case 0xB:   /* INP 3 */
               case 0xC:   /* INP 4 */
               case 0xD:   /* INP 5 */
               case 0xE:   /* INP 6 */
               case 0xF:   /* INP 7 */
                           mem_write(X, 0xff);
                           D = 0xff;
                           break;

               case 0x9:   /* INP 1 */
                           set_display_on();
                           mem_write(X, 0xff);
                           D = 0xff;
                           break;
               }
               break;

    case 0x7:  /* Process operator instructions. */
               switch (N) {
               case 0x0:   /* RET */
               case 0x1:   /* DIS */
                           temp = mem_read_adv(X);
                           X = (temp >> 4) & 0xf;
                           P = (temp) & 0xf;
                           I = !(N & 1);
                           break;

               case 0x2:   /* LDXA */
                           D = mem_read_adv(X);
                           break;

               case 0x3:   /* STXD */
                           mem_write_back(X, D);
                           break;

               case 0x4:   /* ADC */
                           add_op(D, mem_read(X), DF);
                           break;

               case 0x5:   /* SDB */
                           add_op(D^0xff, mem_read(X), !DF);
                           break;

               case 0x6:   /* SHRC */
                           flag = DF;
                           DF = D & 0x1;
                           D >>= 1;
                           if (flag) {
                              D |= 0x80;
                           }
                           cycle();
                           break;

               case 0x7:   /* SMB */
                           add_op(D, mem_read(X)^0xff, !DF);
                           break;

               case 0x8:   /* SAV */
                           mem_write(X, T);
                           break;

               case 0x9:   /* MARK */
                           T = (X << 4) | P;
                           mem_write_back(2, T);
                           X = P;
                           break;

               case 0xA:   /* REQ */
                           Q = 0;
                           cycle();
                           serial_out_cnt = 100;
                           if ((tapecycles - lastcycle) < 3000 && tape_write) {
                               int    v = 0;

                               if ((tapecycles - lastcycle) > 100) {
                                     v = 0x80;
                               }
                               switch (bitcount) {
                               case -1:
                                       if (v) {  /* Start bit. */
                                           bitcount++;
                                       }
                                       break;
                               case 0:
                               case 1:
                               case 2:
                               case 3:
                               case 4:
                               case 5:
                               case 6:
                               case 7:
                                       value >>= 1;
                                       value |= v;
                                       bitcount++;
                                       break;

                               case 8:
                                       /* Parity */
                                       tape_write_byte(value);
                                       bitcount = -1;
                                       value = 0;
                                       break;
                               }
                           }
                           break;

               case 0xB:   /* SEQ */
                           Q = 1;
                           cycle();
                           if (serial_out == 0) {
                               serial_out_cnt = 250;  /* Sample at 1/2 bit time */
                           }
                           lastcycle = tapecycles;
                           break;

               case 0xC:   /* ADCI */
                           add_op(D, fetch(), DF);
                           break;

               case 0xD:   /* SDBI */
                           add_op(D ^ 0xff, fetch(), !DF);
                           break;

               case 0xE:   /* SHLC */
                           cycle();
                           flag = DF;
                           DF = !!(D & 0x80);
                           D <<= 1;
                           D |= flag;
                           break;

               case 0xF:   /* SMBI */
                           add_op(D, fetch() ^ 0xff, !DF);
                           break;
               }
               break;

    case 0x8:  /* GLO */
               D = regs[N] & 0xff;
               cycle();
               break;

    case 0x9:  /* GHI */
               D = (regs[N] >> 8) & 0xff;
               cycle();
               break;

    case 0xA:  /* PLO */
               regs[N] = (regs[N] & 0xff00) | (uint16_t)D;
               cycle();
               break;

    case 0xB:  /* PHI */
               word = (uint16_t)D << 8;
               regs[N] = (regs[N] & 0x00ff) | word;
               cycle();
               break;

    case 0xC:  /* Process long branch and long skip instructions.
                *        00xx      01xx      10xx    11xx
                * xx00    1         !1        !1       I
                * xx01    Q         !Q        !Q       Q
                * xx10    !D        D         D        !D
                * xx11    DF        !DF       !DF      DF
                *        branch    skip     branch    skip
                *
                *       00 1      0             | (!2 & !3)
                *       01 0      0             |  0
                *       10 0      0             |  0
                *       11 I      (I & (2 & 3)) |  0
                *
                *   Branch or (skip taken). Read P and increment.
                *   Skip !taken. Read P, no increment.
                *   branch taken, Set P to result of fetch.
                */

               switch (N) {
               case 0x0:   /* LBR */
                          word = ((uint16_t)fetch()) << 8;
                          word |= (uint16_t)fetch();
                          regs[P] = word;
                          break;

               case 0x1:  /* LBQ */
                          word = ((uint16_t)fetch()) << 8;
                          word |= (uint16_t)fetch();
                          if (Q) {
                              regs[P] = word;
                          }
                          break;

               case 0x2:  /* LBZ */
                          word = ((uint16_t)fetch()) << 8;
                          word |= (uint16_t)fetch();
                          if (D == 0) {
                              regs[P] = word;
                          }
                          break;

               case 0x3:  /* LBDF */
                          word = ((uint16_t)fetch()) << 8;
                          word |= (uint16_t)fetch();
                          if (DF) {
                              regs[P] = word;
                          }
                          break;

               case 0x4:  /* NOP */
                          (void)mem_read(P);
                          (void)mem_read(P);
                          break;

               case 0x5:  /* LSNQ */
                          if (!Q) {
                              (void)fetch();
                              (void)fetch();
                          } else {
                              (void)mem_read(P);
                              (void)mem_read(P);
                          }
                          break;

               case 0x6:  /* LSNZ */
                          if (D != 0) {
                              (void)fetch();
                              (void)fetch();
                          } else {
                              (void)mem_read(P);
                              (void)mem_read(P);
                          }
                          break;

               case 0x7:  /* LSNF */
                          if (!DF) {
                              (void)fetch();
                              (void)fetch();
                          } else {
                              (void)mem_read(P);
                              (void)mem_read(P);
                          }
                          break;

               case 0x8:  /* LSKP */
                          (void)fetch();
                          (void)fetch();
                          break;

               case 0x9:  /* LBNQ */
                          word = ((uint16_t)fetch()) << 8;
                          word |= (uint16_t)fetch();
                          if (!Q) {
                              regs[P] = word;
                          }
                          break;

               case 0xA:  /* LBNZ */
                          word = ((uint16_t)fetch()) << 8;
                          word |= (uint16_t)fetch();
                          if (D != 0) {
                              regs[P] = word;
                          }
                          break;

               case 0xB:  /* LBNF */
                          word = ((uint16_t)fetch()) << 8;
                          word |= (uint16_t)fetch();
                          if (!DF) {
                              regs[P] = word;
                          }
                          break;

               case 0xC:  /* LSIE */
                          if (I) {
                              (void)fetch();
                              (void)fetch();
                          } else {
                              (void)mem_read(P);
                              (void)mem_read(P);
                          }
                          break;

               case 0xD:  /* LSQ */
                          if (Q) {
                              (void)fetch();
                              (void)fetch();
                          } else {
                              (void)mem_read(P);
                              (void)mem_read(P);
                          }
                          break;

               case 0xE:  /* LSZ */
                          if (D == 0) {
                              (void)fetch();
                              (void)fetch();
                          } else {
                              (void)mem_read(P);
                              (void)mem_read(P);
                          }
                          break;

               case 0xF:  /* LSDF */
                          if (DF) {
                              (void)fetch();
                              (void)fetch();
                          } else {
                              (void)mem_read(P);
                              (void)mem_read(P);
                          }
                          break;
               }
               break;

    case 0xD:  /* SEP */
               P = N;
               cycle();
               break;

    case 0xE:  /* SEX */
               X = N;
               cycle();
               break;

    case 0xF:  /* Second operator group instructions. */
               switch(N) {
               case 0x0:   /* LDX */
                           D = mem_read(X);
                           break;

               case 0x1:   /* OR */
                           D |= mem_read(X);
                           break;

               case 0x2:   /* AND */
                           D &= mem_read(X);
                           break;

               case 0x3:   /* XOR */
                           D ^= mem_read(X);
                           break;

               case 0x4:   /* ADD */
                           add_op(D, mem_read(X), 0);
                           break;

               case 0x5:   /* SD */
                           add_op(D ^ 0xff, mem_read(X), 1);
                           break;

               case 0x6:   /* SHR */
                           DF = D & 0x1;
                           D >>= 1;
                           cycle();
                           break;

               case 0x7:   /* SM */
                           add_op(D, mem_read(X) ^ 0xff, 1);
                           break;

               case 0x8:   /* LDI */
                           D = fetch();
                           break;

               case 0x9:   /* ORI */
                           D |= fetch();
                           break;

               case 0xA:   /* ANI */
                           D &= fetch();
                           break;

               case 0xB:   /* XRI */
                           D ^= fetch();
                           break;

               case 0xC:   /* ADI */
                           add_op(D, fetch(), 0);
                           break;

               case 0xD:   /* SDI */
                           add_op(D ^ 0xff, fetch(), 1);
                           break;

               case 0xE:   /* SHL */
                           DF = !!(D & 0x80);
                           D <<= 1;
                           cycle();
                           break;

               case 0xF:   /* SMI */
                           add_op(D, fetch() ^ 0xff, 1);
                           break;
               }
               break;
     }
}

