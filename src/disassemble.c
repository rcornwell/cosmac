/*
 * 1802 - Disassemble a 1802 instruction.
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

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "system.h"
#include "cpu.h"
#include "optab.h"

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

    // Search opcode map for match
    for(op = opcode_map; op->name != NULL; op++) {
        if ((ir & op->mask) == op->base) {
            break;
        }
    }

    // If not found dump possible opcode
    if (op->name == NULL) {
        sprintf(res, "%02x %04x", ir, addr);
        return;
    }

    // Convert opcode based on type.
    *len = op->len;
    switch(op->len) {
    case 1:
           sprintf(res, "%02x        %s", ir, op->name);
           break;
    case 2:
           sprintf(res, "%02x %02x     %s", ir, addr & 0xff, op->name);
           break;
    case 3:
           sprintf(res, "%02x %02x %02x %s", ir, addr & 0xff, (addr >> 8) & 0xff, op->name);
           break;
    }


    switch(op->type) {
    case OPR:  // Basic operators.
         temp[0] = '\0';
         break;

    case OPN:  // Index register operators.
         sprintf(temp, " R%X", ir & 0xf);
         break;

    case OPB:  // Short branch instructions.
         sprintf(temp, " %04x", (pc & 0xff00) | (addr & 0xff));
         break;

    case OPO:  // Input/output instructions.
         sprintf(temp, " %d", ir & 0x7);
         break;

    case OPI: // Immediate value instructions.
         sprintf(temp, " #%02x", addr & 0xff);
         break;

    case OPL: // Long branches.
         sprintf(temp, " %02x%02x", addr & 0xff,  (addr >> 8) & 0xff);
         break;
    }
    strcat(res, temp);
}

/**
 * @brief Dump index registers.
 */
void
dumpregs(char *res)
{
    int i;
    char temp[20];

    for(i = 0; i < 16; i++) {
        sprintf(temp, "R%d=%04x ", i, regs[i]);
        strcat(res, temp);
    }
}

/**
 * @brief Display CPU state and current instruction.
 */
void
trace_irq()
{
    char      line[60];
    char      rdmp[130];

    rdmp[0] = '\0';
    dumpregs(rdmp);
    sprintf(line, "I=%d D=%02x DF=%d P=%2d X=%2d Q=%d i=%d %04x ", I, D, DF, P, X, Q, irq_flag, regs[P]);
    fprintf(stderr, "%s%s irq\n", rdmp, line);
}

/**
 * @brief Display CPU state and current instruction.
 */
void
trace()
{
    char      line[60];
    char      rdmp[130];
    char      inst[40];
    uint16_t  addr;
    uint8_t   ir;
    int       len;

    ir = mem_read_nocycle(P, 0);
    addr = mem_read_nocycle(P, 1);
    addr |= (mem_read_nocycle(P, 2) << 8);
    rdmp[0] = '\0';
    dumpregs(rdmp);
    sprintf(line, "I=%d D=%02x DF=%d P=%2d X=%2d Q=%d i=%d %04x ", I, D, DF, P, X, Q, irq_flag, regs[P]);
    disassemble(inst, ir, regs[P], addr, &len);
    fprintf(stderr, "%s%s%s\n", rdmp, line, inst);
}
