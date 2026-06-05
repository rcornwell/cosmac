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

extern uint16_t   regs[16];       /**< Cpu index registers */
extern uint8_t    D;              /**< Cpu Data register */
extern uint8_t    DF;             /**< Carry flag */
extern uint8_t    P;              /**< Program counter index register */
extern uint8_t    X;              /**< Index register */
extern uint8_t    N;              /**< Instruction Index register */
extern uint8_t    T;              /**< Temporary register */
extern uint8_t    Q;              /**< Q flag */
extern uint8_t    I;              /**< Interupt enable */
extern uint8_t    EF;             /**< External flags */
extern int        line;           /**< Display line */
extern int        col;            /**< Display column */
extern int        row;            /**< Display row */
extern int        dot_pos;        /**< Position to write data to. */
extern uint8_t    irq_flag;       /**< Interrupt flag */
extern uint8_t    display_on;     /**< Display enabled */
extern uint8_t    display_status;
extern uint16_t   rom_enable;     /**< Rom enable */
extern uint16_t   memsize;        /**< Size of memory */
extern uint16_t   memmask;        /**< Bits use to access memory */
extern uint8_t    memory[32*1024]; /**< Memory */
extern uint8_t    dma_in;         /**< Do DMA input cycle */
extern uint8_t    dma_out;        /**< Do DMA output cycle */
extern uint8_t    idle;           /**< Cpu executing idle instruction */
extern int        key_select;     /**< Current key selection */
extern uint8_t    running;        /**< CPU is currently running */
extern uint64_t   cycles;         /**< Current cycle count */
extern int        tapecycles;     /**< Cycle of tape read/write */
extern int        lastcycle;      /**< Time of last Q bit change */
extern int        bitcount;       /**< Bit number being output */
extern int        bittimer;
extern uint8_t    leader;
extern uint8_t    phase;
extern uint8_t    value;
extern int        parity;
extern uint8_t    tape_data;
extern uint8_t    tape_read;
extern uint8_t    tape_write;
extern uint16_t   serial_out;     /**< Serial output data */
extern uint16_t   serial_in;      /**< Last character recieved */
extern int        serial_in_clk;  /**< Timing for serial data */
extern int        system_type;
extern int        color;          /**< Use color display */
extern int        bg_color;       /**< Current background color */
extern int        cartridge;      /**< Cartridge loaded. */

#define VIP          0
#define VP           1
#define RCA_STUDIO2  2
#define RCA_STUDIO3  3

/**
 * @brief Return number of cycles executed.
 *
 * @return cycles.
 */
uint64_t get_cycles();
void reset();

/**
 * @brief Tell memory how many cycles we should have run.
 *
 * @param max_cycles Maximum number of cycles we should have run.
 */
void reset_cycles(int max_cycles);

void set_display_on();

/**
 * @brief Read from memory location without causing cycle.
 *
 * @param r is register to use to select memory location.
 * @param add increase address by amount.
 * @return Value of memory location.
 */
uint8_t mem_read_nocycle(uint8_t r, int add);

/**
 * @brief Read from memory location and cycle to next cycle.
 *
 * Use R to select register to read memory from.
 *
 * @param r is register to use to select memory location.
 * @return Value of memory location.
 */
uint8_t mem_read(uint8_t r);


/**
 * @brief Read from memory, advance register after.
 *
 * @param r is register to use to select memory location.
 * @return Value of memory location.
 */
uint8_t mem_read_adv(uint8_t r);

/**
 * @brief Fetches the next instruction.
 * @return Value of memory location.
 */
uint8_t fetch();

/**
 * @brief Write memory location.
 * @param r Register to write get address of memory location to set.
 * @param data Data to store in memory.`
 */
void mem_write(uint8_t r, uint8_t data);

/**
 * @brief Write memory location, then decrement R(P).
 * @param r Register to write.
 * @param data Data to stop at memory location.
 */
void mem_write_back(uint8_t r, uint8_t data);

/**
 * @brief Set CPU to running and not halted.
 */
void run();

/**
 * @brief Set CPU to running and not halted.
 */
void stop();

/**
 * @brief Process a cpu cycle. Update the display postion.
 *
 * Count lines on screen, 80 lines of blanks. 128 lines of display.
 * Followed by 55 lines of blanks. 4 lines before the display starts,
 * display status is starting. 2 lines before interrupt is set. 4
 * lines before the end of screen display status is set.
 *
 */
void cycle();

/**
 * @brief handle DMA out cycles.
 *
 * For DMA output copy each each memory location to the display.
 */
void dma_out_cycle();

/**
 * @brief handle DMA in cycles.
 *
 * For the moment there is never generation of DMA in signals.
 */
void dma_in_cycle();

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
void step();

#endif
