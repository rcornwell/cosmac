/*
 * Cosmac VIP - Main interface to System.
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


#pragma once

#ifndef _SYSTEM_H_
#define _SYSTEM_H_

#include <stdint.h>
#include <string.h>

extern int trace_flag;
extern uint8_t key[16];
extern uint8_t key2[16];

void init_window();

void init_screen();

void draw_pixel(uint8_t pix, int row, int col);

void run_sim();

void draw_screen();

void power_off();

void tape_write_byte(uint8_t data);

uint8_t tape_read_byte();


/**
 * @brief Return number of cycles executed.
 *
 * @return cycles.
 */
uint64_t  get_cycles();

void reset();

/**
 * @brief Tell memory how many cycles we should have run.
 *
 * @param max_cycles Maximum number of cycles we should have run.
 */
void reset_cycles(const int max_cycles);

void run();

/**
 * @brief Set CPU to running and not halted.
 */
void stop();

/**
 * @brief Load image into memory.
 *
 * @param rom pointer to data.
 * @param size number of bytes to transfer.
 */
void load_chip8();

/**
 * @brief Write character to console.
 */
void write_console(uint16_t data);

/**
 * @brief Start tape reading.
 */
void taperead();

/**
 * @brief Start tape writing..
 */
void tapewrite();

extern uint8_t    running;         /**< CPU running */
extern int        serial;          /**< Emulate serial console */
extern uint16_t   memsize;         /**< Size of memory */
extern uint16_t   memmask;         /**< Valid address bits mask */
extern uint8_t    memory[32*1024]; /**< Memory */
extern uint8_t    dma_in;          /**< DMA input request flag */
extern uint8_t    dma_out;         /**< DMA output request flag */
extern uint8_t    idle;            /**< CPU Idling. */


#define SERIAL_RX_READY  0x01
#define SERIAL_OVER      0x02
#define SERIAL_PARITY    0x04
#define SERIAL_FRAME     0x08
#define SERIAL_STATUS1   0x10
#define SERIAL_STATUS2   0x20
#define SERIAL_TX_SHIFT  0x40
#define SERIAL_TX_READY  0x80
#endif
