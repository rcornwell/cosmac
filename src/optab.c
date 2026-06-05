/*
 * 1802 - Instruction table.
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

#include <stdint.h>
#include "optab.h"

const struct opcode opcode_map[] = {
    { "IDL",  OPR, 0x00, 0xFF, 1 },
    { "LDN",  OPN, 0x00, 0xF0, 1 },
    { "INC",  OPN, 0x10, 0xF0, 1 },
    { "DEC",  OPN, 0x20, 0xF0, 1 },
    { "BR ",  OPB, 0x30, 0xFF, 2 },
    { "BQ ",  OPB, 0x31, 0xFF, 2 },
    { "BZ ",  OPB, 0x32, 0xFF, 2 },
    { "BDF",  OPB, 0x33, 0xFF, 2 },
    { "B1 ",  OPB, 0x34, 0xFF, 2 },
    { "B2 ",  OPB, 0x35, 0xFF, 2 },
    { "B3 ",  OPB, 0x36, 0xFF, 2 },
    { "B4 ",  OPB, 0x37, 0xFF, 2 },
    { "NBR",  OPB, 0x38, 0xFF, 2 },
    { "SKP",  OPR, 0x38, 0xFF, 2 },
    { "BNQ",  OPB, 0x39, 0xFF, 2 },
    { "BNZ",  OPB, 0x3A, 0xFF, 2 },
    { "BNF",  OPB, 0x3B, 0xFF, 2 },
    { "BN1",  OPB, 0x3C, 0xFF, 2 },
    { "BN2",  OPB, 0x3D, 0xFF, 2 },
    { "BN3",  OPB, 0x3E, 0xFF, 2 },
    { "BN4",  OPB, 0x3F, 0xFF, 2 },
    { "LDA",  OPN, 0x40, 0xF0, 1 },
    { "STR",  OPN, 0x50, 0xF0, 1 },
    { "IRX",  OPR, 0x60, 0xFF, 1 },
    { "OUT",  OPO, 0x60, 0xF8, 1 },
    { "x68",  OPR, 0x68, 0xFF, 1 },
    { "INP",  OPO, 0x68, 0xF8, 1 },
    { "RET",  OPR, 0x70, 0xFF, 1 },
    { "DIS",  OPR, 0x71, 0xFF, 1 },
    { "LDXA", OPR, 0x72, 0xFF, 1 },
    { "STXD", OPR, 0x73, 0xFF, 1 },
    { "ADC",  OPR, 0x74, 0xFF, 1 },
    { "SDB",  OPR, 0x75, 0xFF, 1 },
    { "SHRC", OPR, 0x76, 0xFF, 1 },
    { "SMB",  OPR, 0x77, 0xFF, 1 },
    { "SAV",  OPR, 0x78, 0xFF, 1 },
    { "MARK", OPR, 0x79, 0xFF, 1 },
    { "REQ",  OPR, 0x7A, 0xFF, 1 },
    { "SEQ",  OPR, 0x7B, 0xFF, 1 },
    { "ADCI", OPI, 0x7C, 0xFF, 2 },
    { "SDBI", OPI, 0x7D, 0xFF, 2 },
    { "SHLC", OPR, 0x7E, 0xFF, 1 },
    { "SMBI", OPI, 0x7F, 0xFF, 2 },
    { "GLO",  OPN, 0x80, 0xF0, 1 },
    { "GHI",  OPN, 0x90, 0xF0, 1 },
    { "PLO",  OPN, 0xA0, 0xF0, 1 },
    { "PHI",  OPN, 0xB0, 0xF0, 1 },
    { "LBR",  OPL, 0xC0, 0xFF, 3 },
    { "LBQ",  OPL, 0xC1, 0xFF, 3 },
    { "LBZ",  OPL, 0xC2, 0xFF, 3 },
    { "LBDF", OPL, 0xC3, 0xFF, 3 },
    { "NOP",  OPR, 0xC4, 0xFF, 1 },
    { "LSNQ", OPR, 0xC5, 0xFF, 1 },
    { "LSNZ", OPR, 0xC6, 0xFF, 1 },
    { "LSNF", OPR, 0xC7, 0xFF, 1 },
    { "LSKP", OPR, 0xC8, 0xFF, 1 },
    { "LBNQ", OPL, 0xC9, 0xFF, 3 },
    { "LBNZ", OPL, 0xCA, 0xFF, 3 },
    { "LBNF", OPL, 0xCB, 0xFF, 3 },
    { "LSIE", OPR, 0xCC, 0xFF, 1 },
    { "LSQ",  OPR, 0xCD, 0xFF, 1 },
    { "LSZ",  OPR, 0xCE, 0xFF, 1 },
    { "LSDF", OPR, 0xCF, 0xFF, 1 },
    { "SEP",  OPN, 0xD0, 0xF0, 1 },
    { "SEX",  OPN, 0xE0, 0xF0, 1 },
    { "LDX",  OPR, 0xF0, 0xFF, 1 },
    { "OR ",  OPR, 0xF1, 0xFF, 1 },
    { "AND",  OPR, 0xF2, 0xFF, 1 },
    { "XOR",  OPR, 0xF3, 0xFF, 1 },
    { "ADD",  OPR, 0xF4, 0xFF, 1 },
    { "SD ",  OPR, 0xF5, 0xFF, 1 },
    { "SHR",  OPR, 0xF6, 0xFF, 1 },
    { "SM ",  OPR, 0xF7, 0xFF, 1 },
    { "LDI",  OPI, 0xF8, 0xFF, 2 },
    { "ORI",  OPI, 0xF9, 0xFF, 2 },
    { "ANI",  OPI, 0xFA, 0xFF, 2 },
    { "XRI",  OPI, 0xFB, 0xFF, 2 },
    { "ADI",  OPI, 0xFC, 0xFF, 2 },
    { "SDI",  OPI, 0xFD, 0xFF, 2 },
    { "SHL",  OPR, 0xFE, 0xFF, 1 },
    { "SMI",  OPI, 0xFF, 0xFF, 2 },
    { 0,   OPR, 0, 0, 0}
};


