/*
 * Cosmac VIP - Rom images.
 *
 * Author:      Richard Cornwell (rich@sky-visions.com)
 * Copyright 2026, Richard Cornwell
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

#include <stdint.h>


/**
 * @brief COSMAC VIP monitor Rom.
 *
 */
extern const uint8_t rom_data[512];

/**
 * @brief UT4 serial monitor rom.
 */
extern const uint8_t ut4_data[512];

/**
 * @brief Chip8 interpreter.
 */
extern const uint8_t chip8_data[512];

/**
 * @brief Chip8x interpreter.
 */
extern const uint8_t chip8x_data[768];


/**
 * @brief RCA_Studio rom.
 */
extern const uint8_t rca_studio_data[2048];
