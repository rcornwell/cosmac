/*
 * Cosmac VIP - Disassemble a binary.
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

#ifndef _WIN32
#include "config.h"
#else
#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#endif
#include "optab.h"
#include "cpu.h"
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>


/**
 * @brief Disassemble an instruction into string.
 *
 * Convert instruction to readable string.
 */
void
disassemble(char *res, uint8_t ir, uint16_t pc, uint16_t addr, int *len)
{
    const struct opcode *op;
    char temp[50];

    *len = 1;

    /* Search opcode map for match */
    for(op = opcode_map; op->name != NULL; op++) {
        if ((ir & op->mask) == op->base) {
            break;
        }
    }

    /* If not found dump possible opcode */
    if (op->name == NULL) {
        sprintf(res, ",%02x ", ir);
        return;
    }

    /* Convert opcode based on type. */
    *len = op->len;
    strcpy(res, op->name);

    switch(op->type) {
    case OPR:  /* Basic operators. */
         temp[0] = '\0';
         break;

    case OPN:  /* Index register operators. */
         sprintf(temp, " R%X", ir & 0xf);
         break;

    case OPB:  /* Short branch instructions. */
         sprintf(temp, " L%03x", (pc & 0x0f00) | (addr & 0xff));
         break;

    case OPO:  /* Input/output instructions. */
         sprintf(temp, " %d", ir & 0x7);
         break;

    case OPI: /* Immediate value instructions. */
         sprintf(temp, " #%02x", addr & 0xff);
         break;

    case OPL: /* Long branches. */
         sprintf(temp, " L%x%02x", addr & 0xf,  (addr >> 8) & 0xff);
         break;
    }
    strcat(res, temp);
}

/**
 * @brief Main interface between emulator and SDL.
 *
 * This file holds the interface between the emulator and SDL library.
 * This is not a class since these functions interface with C libraries.
 */

uint8_t           memory[32*1024];
int               read_bin(char *name);
int               read_dump(char *name);
/**
 * @brief  main, entry to system.
 *
 * Scan arguments looking for scale and cartridge file names to load.
 */
int main(int argc, char **argv)
{
     int           i;            /* Temp */
     int           sz;
     char          inst[40];
     uint16_t      addr;
     uint8_t       ir;
     int           len;

     /* Read dump file */
     sz = read_dump(argv[1]);
     printf(".. Read %d bytes\n", sz);

     for (i = 0; i < sz; i++) {
         ir = memory[i];
         addr = memory[i+1];
         addr |= (memory[i+2]) << 8;
         disassemble(inst, ir, i, addr, &len);
         printf("L%03X: %s                  .. ,#%02X \n", i, inst, ir);
     }

     return 0;
}

/**
 * @brief Read binary file into ram.
 *
 * Read binary file into ram, only read at most the first
 * 32k of the file.
 *
 * @param name Name of file to read.
 * @return 1 on success, 0 on failure.
 */
int
read_bin(char *name)
{
     FILE    *in = fopen(name, "rb");
     size_t   len;
     size_t   got;

     if (in == NULL) {
         fprintf(stderr, "Unable to read file %s: %s\n", name, strerror(errno));
         return 0;
     }

     fseek(in, 0, SEEK_END);
     len = ftell(in);
     rewind(in);

     if (len > (32 * 1024)) {
         len = 32 * 1024;
     }

     got = fread(memory, 1, len, in);

     if (got != len) {
         fprintf(stderr, "Unable to read file %s: %s\n", name, strerror(errno));
         fclose(in);
         return 0;
     }

     fclose(in);
     fprintf(stderr, "Read %ld bytes from %s\n", got, name);
     return got;
}

const char *hex = "0123456789ABCDEF";

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
int
read_dump(char *name)
{
     FILE     *in = fopen(name, "r");
     char     buffer[256];
     uint16_t high = 0;


     if (in == NULL) {
         fprintf(stderr, "Unable to read file %s: %s\n", name, strerror(errno));
         return 0;
     }

     /* Read in line of input */
     while(fgets(buffer, sizeof(buffer), in) != NULL) {
        int       count = 0;
        char      *digit;
        uint16_t  address = 0;
        char      *ptr;
        uint16_t  value = 0;

        for (ptr = buffer; *ptr != '\0'; ptr++) {
            /* If space skip to next char. */
            if (isspace(*ptr)) {
                continue;
            }

            /* Terminate scan on ;. */
            if (*ptr == ';') {
                break;
            }

            /* Convert hex digits to binary */
            digit = strchr(hex, toupper(*ptr));
            if (digit == NULL) {
                fprintf(stderr, "Invalid character %s: %s", name, buffer);
                fclose(in);
                return 0;
            }
            value = (value << 4) | (digit - hex);
            count++;
            /* First 4 digits are address */
            if (count == 4) {
                address = value;
                value = 0;
                continue;
            }
            /* Check if address over 32K */
            if (address > (32 * 1024)) {
                break;
            }

            /* Every 2 hex digits, deposit another byte */
            if ((count > 4) && (count & 1) == 0) {
                memory[address++] = value;
                value = 0;
                if (address > high) {
                    high = address;
                }
            }
        }
     }
     fclose(in);
     return (int)high;
}

