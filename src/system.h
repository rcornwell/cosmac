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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

/**
 * @file system.h
 * @brief System-level interfaces for SDL rendering, keyboard input,
 *        tape I/O, serial console, and display management.
 *
 * This header defines the interface between the CPU emulation core (cpu.h)
 * and the SDL2-based platform layer (main.c). It includes:
 * - Display operations (pixel drawing, screen buffer management)
 * - Keyboard state tracking (key arrays for VIP and Studio II/III)
 * - Tape I/O (binary tape recording/playback)
 * - Serial console input/output
 * - Global trace and system configuration flags
 *
 * The display uses a 256x128 RGBA32 pixel buffer with an 8-color palette.
 *
 * @author Richard Cornwell (rich@sky-visions.com)
 * @copyright 2025 Richard Cornwell
 */

#pragma once

#ifndef _SYSTEM_H_
#define _SYSTEM_H_

#include <stdint.h>
#include <string.h>

/** @brief Instruction trace flag - non-zero when instruction-level
 *  tracing to stderr is enabled via the -i command-line option. */
extern int trace_flag;

/** @brief VIP keyboard key states (RCA Studio I/VP).
 *  Index: 0=digit 0, 1-9=digits, 0xa=period, 0xb=enter,
 *  0xc=plus, 0xd=minus, 0xe=multiply, 0xf=divide. */
extern uint8_t key[16];

/** @brief RCA Studio II/III keyboard key states.
 *  Index: 0=X, 1=A, 2=S, 3=D, 4=Q, 5=W, 6=E, 7=1, 8=2, 9=3. */
extern uint8_t key2[16];

/**
 * @brief Read binary file into ram.
 *
 * Read binary file into ram, only read at most the first
 * 32k of the file.
 *
 * @param name Name of file to read.
 * @return 1 on success, 0 on failure.
 */
int read_bin(char *name);

/**
 * @brief Read dump file into memory.
 *
 * Read a dump file into memory. A dump file consists of a 4
 * hex character address followed by a series of hex digits. Spaces
 * are ignored. Adresses greater then 32k are ignored. ';' terminates
 * The line.
 *
 * @param name Name of file to read.
 * @return 1 on success, 0 on failure.
 */
int read_dump(char *name);

/**
 * @brief Initialize SDL2 - create the main window, renderer, texture,
 *        and audio device with square wave audio callback support.
 *
 * Also initializes the serial console if the -s flag is provided.
 */
void init_window();

/**
 * @brief Initialize the screen for a new frame.
 *
 * Called at the start of each display frame (before line 80).
 * Currently a no-op - placeholder for frame-start operations.
 */
void init_screen();

/**
 * @brief Draw a single pixel at the specified row and column.
 *
 * Maps the pixel color index to an SDL2 RGBA color via the palette
 * table, then writes 2x2 pixels (scaled) to the screen buffer using
 * SDL_MapRGBA. The screen buffer row is calculated as row*256 + col*4.
 *
 * @param pix Color palette index (0-7) to use for the pixel.
 * @param row Screen row (0-127) within the virtual display.
 * @param col Screen column (0-63) within the virtual display.
 */
void draw_pixel(uint8_t pix, int row, int col);

/**
 * @brief Main SDL2 event loop and CPU simulation driver.
 *
 * Handles SDL2 event processing (keyboard, window resize, quit), runs
 * the CPU for one display frame (CYCLES_PER_SCREEN = 262*14 cycles),
 * updates the display texture via SDL2, manages frame timing to achieve
 * ~60Hz refresh with adaptive delay, and handles audio playback.
 *
 * The main loop:
 * 1. Processes pending SDL2 events (key press/release, window events)
 * 2. Runs step() until CYCLES_PER_SCREEN cycles are consumed
 * 3. Updates the display texture and presents it via SDL2
 * 4. Calculates elapsed time and adjusts frame timing
 * 5. Repeats until POWER flag is cleared (F6 key or window close)
 */
void run_sim();

/**
 * @brief Present the current frame buffer to the SDL2 renderer.
 *
 * Copies the 256x128 screen buffer to the SDL2 texture, then copies
 * the texture through the renderer to the window, and presents it.
 * Called once per frame at the end of the CDP 1861 display timing cycle.
 */
void draw_screen();

/**
 * @brief Shut down the emulator - power off.
 *
 * Currently a no-op. Used to signal cleanup of emulator resources.
 */
void power_off();

/**
 * @brief Encode and write one byte to the virtual tape file.
 *
 * Each byte is written as two uppercase hexadecimal characters
 * (high nibble then low nibble) to the tape_file.
 *
 * @param data The 8-bit value to write to tape.
 */
void tape_write_byte(uint8_t data);

/**
 * @brief Read one byte from the virtual tape file.
 *
 * Reads two hexadecimal characters from the tape file, combines
 * them into an 8-bit value (high nibble first), and returns it.
 * A colon ':' character signals end-of-record and returns 0.
 * Invalid characters produce a stderr warning and return 0.
 *
 * @return The 8-bit value read from tape, or 0 on end-of-record/error.
 */
uint8_t tape_read_byte();

/**
 * @brief Return number of cycles executed.
 *
 * @return cycles.
 */
uint64_t  get_cycles();

/**
 * @brief Reset the CPU and all emulated peripherals to their initial
 *        power-on state.
 *
 * Zeroes all registers, clears DMA/tape flags, sets default ROM mapping
 * (0x8000), fills unprogrammed memory with 0xFF, and resets display
 * position, timing counters, and serial console state.
 */
void reset();

/**
 * @brief Tell memory how many cycles we should have run.
 *
 * Adjusts the main cycle counter by subtracting the specified number
 * of cycles. Used after each complete frame to compensate for cycles
 * that were consumed but could not be emulated (e.g., due to SDL
 * frame rate limitations).
 *
 * @param max_cycles Maximum number of cycles we should have run.
 */
void reset_cycles(const int max_cycles);

/**
 * @brief Start the CPU - set the running state to active.
 */
void run();

/**
 * @brief Halt the CPU - set the running state to stopped.
 */
void stop();

/**
 * @brief Load the CHIP-8 interpreter ROM into low memory.
 *
 * If color mode is enabled, loads chip8x_data (768 bytes).
 * Otherwise loads chip8_data (512 bytes) and sets bin_base
 * to the loaded size.
 */
void load_chip8();

/**
 * @brief Write one control character to the serial console output.
 *
 * Converts the 8 least significant bits of data to a character
 * via XOR with 0x7F, then writes it to the terminal file descriptor.
 *
 * @param data 9-bit data value; the lower 8 bits after XOR with 0x7F
 *             are written as ASCII.
 */
void write_console(uint16_t data);

/**
 * @brief Start tape reading.
 *
 * Initializes the tape read state with a leader countdown of
 * -12*256 bytes (determines sync preamble length) and sets the
 * leader flag to request phase synchronization.
 */
void taperead();

/**
 * @brief Start tape writing..
 */
void tapewrite();

/** @brief CPU running state flag - non-zero when the emulator is
 *  actively running (POWER flag in main.c). */
extern uint8_t    running;

/** @brief Serial console enabled flag - non-zero when the emulator
 *  is running with serial I/O via /dev/tty. */
extern int        serial;

/** @brief Total memory size in bytes for the emulated system. */
extern uint16_t   memsize;

/** @brief Bit mask applied to addresses to determine which address
 *  bits are valid for memory access. */
extern uint16_t   memmask;

/** @brief 32KB emulated system memory (0x0000-0x7FFF RAM, with ROM
 *  mirroring above 0x8000 depending on system type). */
extern uint8_t    memory[32*1024];

/** @brief DMA input request flag - set when the display controller
 *  requests memory data (not actively used in current implementation). */
extern uint8_t    dma_in;

/** @brief DMA output request flag - set during active display lines
 *  to transfer memory contents to the pixel buffer. */
extern uint8_t    dma_out;

/** @brief CPU idle flag - non-zero when the CPU is executing the IDLE
 *  instruction and waiting for wakeup events. */
extern uint8_t    idle;

/** @brief Serial status bit flags for the serial console control registers. */
#define SERIAL_RX_READY  0x01    /**< Receive data available */
#define SERIAL_OVER      0x02    /**< Overrun condition */
#define SERIAL_PARITY    0x04    /**< Parity error */
#define SERIAL_FRAME     0x08    /**< Frame error */
#define SERIAL_STATUS1   0x10    /**< Status register 1 valid */
#define SERIAL_STATUS2   0x20    /**< Status register 2 valid */
#define SERIAL_TX_SHIFT  0x40    /**< Transmit in progress */
#define SERIAL_TX_READY  0x80    /**< Transmit register ready */

#endif
