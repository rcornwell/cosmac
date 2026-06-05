/*
 * Cosmac CPU test.
 *
 * Copyright 2026, Richard Cornwell
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ctest.h"
#include "system.h"
#include "cpu.h"

extern int verbose;
int serial = 0;

void draw_pixel(uint8_t pix, int row, int col) {
}

void draw_screen() {
}

void tape_write_byte(uint8_t data) {
}

uint8_t tape_read_byte() {
    return 0;
}

void write_console(uint16_t data) {
}

uint8_t    key[16];
uint8_t    key2[16];

void
run_test(const char *name, const char *prog, size_t prog_len, 
           size_t steps, void (*setup)(void *), void * data)
{
   int i;

   if (verbose) {
      puts(name);
      putchar('\n');
   }

   memsize = 256;
   memmask = 0xfff;
   for (i = 0; i < memsize; memory[i++] = 0x00);
   for (i = 0; i < 16; regs[i++] = 0xcccc);

   /* Copy program to memory. */
   for (i = 0; i < (int)prog_len; i++) {
       memory[i] = prog[i];
   }

   /* Reset CPU */
   reset();
   D = 0xcc;
   X = 0x5;
   P = 0x0;
   rom_enable = 0;
   setup(data);
   run();

   /* Step CPU */
   for (i = 0; i < steps; i++) {
       step();
       if (idle) {
           break;
       }
   }
}
   
void
setup_noop (void *data)
{
}

void
run_basic_test(const char *name, const char *prog, size_t prog_len, 
           size_t steps) {
   run_test(name, prog, prog_len, steps, setup_noop, 0);
}

#define NBRANCH_PROG(x) (x), sizeof(x), sizeof(x)
#define PROG(x) (x), sizeof(x)

char name_buffer[64];

/* Check IDLE instruction */
CTEST(cpu, idle)
{
    char prog[] = { 0x00, 0x01, 0x01, 0x01 };

    run_basic_test("00 IDL", NBRANCH_PROG(prog));
    ASSERT_TRUE(idle);
    ASSERT_EQUAL_X(regs[0], 0x01);
    ASSERT_EQUAL_X(0xcc, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(X, 0x5);
}

void
setup_ldn(void *data)
{
   regs[*(int *)data] = 0xb3;
   memory[0xb3] = 0xa0;
}

/* Check LDN instruction */
CTEST(cpu, ldn)
{
    int i;

    for (i = 0x01; i < 0x10; i++) {
        char c = (char)i;
        int  j = i & 0xf;
        sprintf(name_buffer, " %02x LDN R(N)", i);
        run_test(name_buffer, &c, 1, 1, setup_ldn, &j);
        ASSERT_EQUAL_X(0xb3, regs[i]);
        ASSERT_EQUAL_X(0xa0, D);
        ASSERT_EQUAL_X(0, P);
        ASSERT_EQUAL_X(0x5, X);
    }
}

void
setup_inc1(void *data)
{
   regs[*(int *)data] = 0;
}
 
void
setup_inc2(void *data)
{
   regs[0] = 5;
}
 
/* Check INC instruction */
CTEST(cpu, inc)
{
    int  i, j;

    char inc_prog1 [7];

    char inc_prog2 [] = { 0xf8, 0x07, 0xa3, 0xf8, 0x00, 0xb3, 0xd3, 0xa0, 0x10, 0x10, 0x10, 0x10, 0x10};

    run_basic_test(" 10 INC R(0)", NBRANCH_PROG(inc_prog2));

    ASSERT_EQUAL(5, regs[0]);
    ASSERT_EQUAL_X(P, 3);
    ASSERT_EQUAL_X(0x5, X);
    ASSERT_EQUAL_X(0, D);

    for (i = 0x11; i < 0x20; i++) {
        char c = (char)i;
        sprintf(name_buffer, " %02x INC R(N)", i);
        for (j = 0; j < (int)sizeof(inc_prog1); j++) {
           inc_prog1[j] = c;
        }
        j = i & 0xf;
        run_test(name_buffer, NBRANCH_PROG(inc_prog1), setup_inc1, (void *)&j);
        ASSERT_EQUAL_X(0xcc, D);
        ASSERT_EQUAL_X(0x7, regs[j]);
        ASSERT_EQUAL_X(0, P);
        ASSERT_EQUAL_X(0x5, X);
    }
}

void
setup_dec1(void *data)
{
   regs[*(int *)data] = 13;
}
 
/* Check DEC instruction */
CTEST(cpu, dec)
{
    int  i, j;

    char dec_prog1 [3];

    char dec_prog2 [] = { 0xf8, 0x07, 0xa3, 0xf8, 0x00, 0xb3, 0xd3, 0xa0, 0x20, 0x20, 0x20};

    run_basic_test(" 10 INC R(0)", NBRANCH_PROG(dec_prog2));

    ASSERT_EQUAL(0xfffd, regs[0]);
    ASSERT_EQUAL_X(P, 3);
    ASSERT_EQUAL_X(0x5, X);
    ASSERT_EQUAL_X(0, D);

    for (i = 0x21; i < 0x30; i++) {
        char c = (char)i;
        sprintf(name_buffer, " %02x DEC R(N)", i);
        for (j = 0; j < (int)sizeof(dec_prog1); j++) {
           dec_prog1[j] = c;
        }
        j = i & 0xf;
        run_test(name_buffer, NBRANCH_PROG(dec_prog1), setup_dec1, (void *)&j);
        ASSERT_EQUAL_X(0xcc, D);
        ASSERT_EQUAL_X(10, regs[j]);
        ASSERT_EQUAL_X(0, P);
        ASSERT_EQUAL_X(0x5, X);
    }
}


const char br_test_progs1 [13][20] = {
/* 30: unconditional
 * End condition : R (0) == 0x04 after two instructions */
 { 0x30, 0x0F, 0x30, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x0C, 0x30, 0x05, 0x00, 0x00, 0x30, 0x04 },

/* 31: if Q == 1 || 39: if Q == 0
 * End condition : R (0) == 0x08 after six instructions */
 { 0x31 ,0x00 ,0x39 ,0x06 ,0x30 ,0x00 ,0x7B ,0x31 ,0x0B ,0x30 ,0x00 ,0x7A ,0x39 ,0x08 },

/* 32: if D == 0 || 3A : if D != 0
 * End condition : R (0) == 0x0F after six instructions */
 { 0xF8 ,0x00 ,0x32 ,0x05 ,0x00 ,0xF8 ,0x03 ,0x3A ,0x0A ,0x00 ,0xF8 ,0x00 ,0x32 ,0x0F },

/* 33: if DF == 1 || 3B : if DF == 0
 * End condition : R (0) == 0x0C after five instructions */
 { 0x33, 0xFF, 0x3B, 0x05, 0x00, 0xF8, 0x01, 0x76, 0x33, 0x0C, 0x30, 0xFE },

/* 38: SKP
 * End condition : R (0) == 0x02 after one instruction */
 { 0x38, 0x0a, 0x00 },

/* 34 -37: branch if external flag N == 1
 * 3C -3F : inverse
 * End conditions : R (0) == 0x08 after two instructions */
 { 0x3C, 0xFF, 0x34, 0x08 },
 { 0x3D, 0xFF, 0x35, 0x08 },
 { 0x3E, 0xFF, 0x36, 0x08 },
 { 0x3F, 0xFF, 0x37, 0x08 },
 { 0x34, 0xFF, 0x3C, 0x08 },
 { 0x35, 0xFF, 0x3D, 0x08 },
 { 0x36, 0xFF, 0x3E, 0x08 },
 { 0x37, 0xFF, 0x3F, 0x08 }
};

const char * br_prog_names [13] = {
  " 30 BRANCH UNCOND ",
  " 31 BQ + 39 BNQ ",
  " 32 BZ + 3 A BNZ ",
  " 33 BDF + 3 B BNF ",
  " 38 SKP ",
  " 34 B1 ",
  " 35 B2 ",
  " 36 B3 ",
  " 37 B4 ",
  " 3C BN1 ",
  " 3D BN2 ",
  " 3E BN3 ",
  " 3F BN4 "
};

int br_prog_instrs [13] = {2 , 6 , 6 , 5 , 1 , 2 , 2 , 2 , 2 , 2 , 2 , 2 , 2};

int br_prog_targets [13] = {0x05 , 0x08 , 0x0F , 0x0d , 0x03 , 0x08 , 0x08 , 0x08 , 0x08 , 0x08 , 0x00};

void
setup_cbr1(void *data)
{
   int   x = *(int *)data;

   if (x >= 5 && x <= 8) {
       EF = 1 << (x - 5);
   }
   if (x >= 9) {
       EF = ~(1 << (x - 9));
   }
}

/* Check Short branch instruction */
CTEST(cpu, short_branch)
{
    int  i;

    for (i = 0; i < 5; i++) {
        run_test ( br_prog_names[i], &br_test_progs1[i][0], 19, 6, setup_cbr1, (void *)&i);

        ASSERT_EQUAL_X(br_prog_targets[i], regs[0]);
        ASSERT_EQUAL_X(0, P);
        ASSERT_EQUAL_X(0x5, X);
    }
}

/* Test LDA instruction */
void
setup_lda1(void *data)
{
   if ((*(int *)data) != 0) {
       regs[*(int *)data] = 0x09;
   }
}

CTEST(cpu, lda)
{
    int  i, j;
    char lda_test_prog1[] = { 0x40, 0xe0, 0x00, 0x00, 0x00, 0x40, 0x09, 0x00, 0x00, 0xe0 };

    for (i = 0x40; i < 0x50; i++) {
        lda_test_prog1[0] = lda_test_prog1[5] = (char)i;
        sprintf(name_buffer, " %02x LDA R(N)", i);
        j = i & 0xf;
        run_test(name_buffer, PROG(lda_test_prog1), 1, setup_lda1, (void *)&j);
        ASSERT_EQUAL_X(0xe0, D);
        ASSERT_EQUAL_X((j)?0xA: 0x2, regs[j]);
        ASSERT_EQUAL_X(0, P);
        ASSERT_EQUAL_X(0x5, X);
    }
}

/* Test STR instruction */
void
setup_str1(void *data)
{
   D = 0x2c;
   if (*(int *)data != 0) {
      regs[*(int *)data] = 0x05;
   }
}

CTEST(cpu, str)
{
   int   i, j;
   char str_test_prog1[] = { 0x50, 0x00, 0x00, 0x00, 0x00, 0x00 };

   for (i = 0x50; i < 0x60; i++) {
        str_test_prog1[0] = (char)i;
        sprintf(name_buffer, " %02x STR R(N)", i);
        j = i & 0xf;
        run_test(name_buffer, PROG(str_test_prog1), 1, setup_str1, &j);
        ASSERT_EQUAL_X(0x2c, D);
        ASSERT_EQUAL_X(0x2c, memory[(j) ? 5: 1]);
        ASSERT_EQUAL_X(0, P);
        ASSERT_EQUAL_X(0x5, X);
   }
}
       
/* Test IRX instruction */
void
setup_irx(void *data)
{
   int   x = *(int *)data;
   if (x != 0) {
       regs[x] = 0xBF;
   }
   X = x;
}

/* Test IRX instruction */
CTEST(cpu, irx)
{
    int i;

    for (i = 0x00; i < 0x10; i++) {
        char c = 0x60;
        sprintf(name_buffer, " %02x IRX", i);
        run_test(name_buffer, &c, 1, 1, setup_irx, (void *)&i);
        ASSERT_EQUAL_X((i)? 0xC0: 0x02, regs[i]);
        ASSERT_EQUAL_X(0xcc, D);
        ASSERT_EQUAL_X(0, P);
        ASSERT_EQUAL_X(X, i);
    }
}

/* Test GLO instruction */
void
setup_glo1(void *data)
{
    D = 0xcc;
   if (*(int *)data != 0) {
      regs[*(int *)data] = 0x2e74;
   }
}

CTEST(cpu, glo)
{
   int   i, j;

    for (i = 0x80; i < 0x90; i++) {
        char c = (char)i;
        j = i & 0xf;
        sprintf(name_buffer, " %02x GLO %d", i, j);
        run_test(name_buffer, &c, 1, 1, setup_glo1, (void *)&j);
        ASSERT_EQUAL_X((j)? 0x74: 0x01, D);
        ASSERT_EQUAL_X(0, P);
        ASSERT_EQUAL_X(0x5, X);
    }
}

/* Test GHI instruction */

CTEST(cpu, ghi)
{
   int   i, j;

    for (i = 0x90; i < 0xA0; i++) {
        char c = (char)i;
        j = i & 0xf;
        sprintf(name_buffer, " %02x GHI %d", i, j);
        run_test(name_buffer, &c, 1, 1, setup_glo1, (void *)&j);
        ASSERT_EQUAL_X((j)? 0x2e: 0x00, D);
        ASSERT_EQUAL_X(0, P);
        ASSERT_EQUAL_X(0x5, X);
    }
}

/* Test PLO instruction */
void
setup_plo1(void *data)
{
   D = 0xCB;
   if (*(int *)data != 0) {
      regs[*(int *)data] = 0x0000;
   }
}

CTEST(cpu, plo)
{
   int   i, j;

    for (i = 0xA0; i < 0xB0; i++) {
        char c = (char)i;
        j = i & 0xf;
        sprintf(name_buffer, " %02x PLO %d", i, j);
        run_test(name_buffer, &c, 1, 1, setup_plo1, (void *)&j);
        ASSERT_EQUAL_X(0xCB, D);
        ASSERT_EQUAL_X(0x00CB, regs[j]);
        ASSERT_EQUAL_X(0, P);
        ASSERT_EQUAL_X(0x5, X);
    }
}

/* Test PHI instruction */

CTEST(cpu, phi)
{
   int   i, j;

    for (i = 0xB0; i < 0xC0; i++) {
        char c = (char)i;
        j = i & 0xf;
        sprintf(name_buffer, " %02x PHI %d", i, j);
        run_test(name_buffer, &c, 1, 1, setup_plo1, (void *)&j);
        ASSERT_EQUAL_X(0xCB, D);
        ASSERT_EQUAL_X((j)? 0xCB00: 0xCB01, regs[j]);
        ASSERT_EQUAL_X(0, P);
        ASSERT_EQUAL_X(0x5, X);
    }
}

/* Test Long branch instrion */

CTEST(cpu, lbr)
{
    char lbr_test_prog1[] =  {0xc0, 0x34, 0x56 };

    run_basic_test("C0 LBR", PROG(lbr_test_prog1), 1);
    ASSERT_EQUAL_X(0x3456, regs[0]);
    ASSERT_EQUAL_X(0xcc, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);
}

void
setup_lbq1(void *data)
{
    Q = 1;
    D = 0;
}

void
setup_lbnq1(void *data)
{
    Q = 0;
    D = 0;
}


CTEST(cpu, lbq)
{
    char lbq_test_prog1[] =  {0xc1, 0x34, 0x56 };
    char lbnq_test_prog2[] = {0xc9, 0x34, 0x56 };

    run_test("C1 LBQ", PROG(lbq_test_prog1), 1, setup_lbq1, NULL);
    ASSERT_EQUAL_X(0x3456, regs[0]);
    ASSERT_EQUAL_X(0, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("C9 LBNQ", PROG(lbnq_test_prog2), 1, setup_lbq1, NULL);
    ASSERT_EQUAL_X(0x0003, regs[0]);
    ASSERT_EQUAL_X(0, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("C1 LBQ", PROG(lbq_test_prog1), 1, setup_lbnq1, NULL);
    ASSERT_EQUAL_X(0x0003, regs[0]);
    ASSERT_EQUAL_X(0, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("C9 LBNQ", PROG(lbnq_test_prog2), 1, setup_lbnq1, NULL);
    ASSERT_EQUAL_X(0x3456, regs[0]);
    ASSERT_EQUAL_X(0, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);
}

void
setup_lbz1(void *data)
{
    Q = 0;
    D = 0x00;
}

void
setup_lbnz1(void *data)
{
    Q = 0;
    D = 0xfe;
}

CTEST(cpu, lbz)
{
    char lbz_test_prog1[] =  {0xc2, 0x34, 0x56 };
    char lbnz_test_prog2[] = {0xca, 0x34, 0x56 };

    run_test("C2 LBZ", PROG(lbz_test_prog1), 1, setup_lbz1, NULL);
    ASSERT_EQUAL_X(0x3456, regs[0]);
    ASSERT_EQUAL_X(0, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("CA LBNZ", PROG(lbnz_test_prog2), 1, setup_lbz1, NULL);
    ASSERT_EQUAL_X(0x0003, regs[0]);
    ASSERT_EQUAL_X(0, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("C2 LBZ", PROG(lbz_test_prog1), 1, setup_lbnz1, NULL);
    ASSERT_EQUAL_X(0x0003, regs[0]);
    ASSERT_EQUAL_X(0xfe, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("CA LBNZ", PROG(lbnz_test_prog2), 1, setup_lbnz1, NULL);
    ASSERT_EQUAL_X(0x3456, regs[0]);
    ASSERT_EQUAL_X(0xfe, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);
}

void
setup_lbdf1(void *data)
{
    DF = 1;
}

void
setup_lbndf1(void *data)
{
    DF = 0;
}

CTEST(cpu, lbdf)
{
    char lbdf1_test_prog1[] =  {0xc3, 0x34, 0x56 };
    char lbnf1_test_prog2[] = {0xcb, 0x34, 0x56 };

    run_test("C3 LBDF", PROG(lbdf1_test_prog1), 1, setup_lbdf1, NULL);
    ASSERT_EQUAL_X(0x3456, regs[0]);
    ASSERT_EQUAL_X(0xcc, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("CB LBNF", PROG(lbnf1_test_prog2), 1, setup_lbdf1, NULL);
    ASSERT_EQUAL_X(0x0003, regs[0]);
    ASSERT_EQUAL_X(0xcc, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("C3 LBDF", PROG(lbdf1_test_prog1), 1, setup_lbndf1, NULL);
    ASSERT_EQUAL_X(0x0003, regs[0]);
    ASSERT_EQUAL_X(0xcc, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("CB LBNF", PROG(lbnf1_test_prog2), 1, setup_lbndf1, NULL);
    ASSERT_EQUAL_X(0x3456, regs[0]);
    ASSERT_EQUAL_X(0xcc, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);
}

/* Test SEP instruction */
CTEST(cpu, sep)
{
    int    i, j;

    for (i = 0xd0; i < 0xe0; i++) {
        char c = (char)i;
        j = i & 0xf;
        sprintf(name_buffer, " %02x SEP %d", i, j);
        run_basic_test(name_buffer, &c, 1, 1);
        ASSERT_EQUAL_X(0xcc, D);
        ASSERT_EQUAL_X(j, P);
        ASSERT_EQUAL_X(0x5, X);
   }
}

/* Test SEX instruction */
CTEST(cpu, sex)
{
    int    i, j;

    for (i = 0xe0; i < 0xf0; i++) {
        char c = (char)i;
        j = i & 0xf;
        sprintf(name_buffer, " %02x SEX %d", i, j);
        run_basic_test(name_buffer, &c, 1, 1);
        ASSERT_EQUAL_X(0xcc, D);
        ASSERT_EQUAL_X(0, P);
        ASSERT_EQUAL_X(j, X);
   }
}

/* Test NOP instruction */
CTEST(cpu, nop)
{
    char c = 0xc4;

    sprintf(name_buffer, " %02x NOP", c);
    run_basic_test(name_buffer, &c, 1, 1);
    ASSERT_EQUAL(regs[0], 0x01);
    ASSERT_EQUAL_X(0xcc, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);
}

/* Test long skip Q instructions */
CTEST(cpu, lsq)
{
    char lsq1_test_prog1[] =  {0xcd, 0xf8, 0x56 };
    char lsnq1_test_prog2[] = {0xc5, 0xf8, 0x56 };

    run_test("CD LSQ", PROG(lsq1_test_prog1), 1, setup_lbq1, NULL);
    ASSERT_EQUAL_X(0x3, regs[0]);
    ASSERT_EQUAL_X(0, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("C5 LSNQ", PROG(lsnq1_test_prog2), 1, setup_lbq1, NULL);
    ASSERT_EQUAL_X(0x1, regs[0]);
    ASSERT_EQUAL_X(0, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("CD LSQ", PROG(lsq1_test_prog1), 1, setup_lbnq1, NULL);
    ASSERT_EQUAL_X(0x1, regs[0]);
    ASSERT_EQUAL_X(0, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("C5 LSNQ", PROG(lsnq1_test_prog2), 1, setup_lbnq1, NULL);
    ASSERT_EQUAL_X(0x3, regs[0]);
    ASSERT_EQUAL_X(0, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);
}

/* Test long skip D=0 instructions */
CTEST(cpu, lsz)
{
    char lsz1_test_prog1[] =  {0xce, 0xf8, 0x56 };
    char lsnz1_test_prog2[] = {0xc6, 0xf8, 0x56 };

    run_test("CE LSZ", PROG(lsz1_test_prog1), 1, setup_lbz1, NULL);
    ASSERT_EQUAL_X(0x3, regs[0]);
    ASSERT_EQUAL_X(0, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("C6 LSNZ", PROG(lsnz1_test_prog2), 1, setup_lbz1, NULL);
    ASSERT_EQUAL_X(0x1, regs[0]);
    ASSERT_EQUAL_X(0, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("CE LSZ", PROG(lsz1_test_prog1), 1, setup_lbnz1, NULL);
    ASSERT_EQUAL_X(0x1, regs[0]);
    ASSERT_EQUAL_X(0xfe, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("C6 LSNZ", PROG(lsnz1_test_prog2), 1, setup_lbnz1, NULL);
    ASSERT_EQUAL_X(0x3, regs[0]);
    ASSERT_EQUAL_X(0xfe, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);
}

/* Test long skip DF=0 instructions */
CTEST(cpu, lsdf)
{
    char lsdf1_test_prog1[] =  {0xcf, 0xf8, 0x56 };
    char lsnf1_test_prog2[] = {0xc7, 0xf8, 0x56 };

    run_test("CF LSDF", PROG(lsdf1_test_prog1), 1, setup_lbdf1, NULL);
    ASSERT_EQUAL_X(0x3, regs[0]);
    ASSERT_EQUAL_X(0xcc, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("C7 LSNF", PROG(lsnf1_test_prog2), 1, setup_lbdf1, NULL);
    ASSERT_EQUAL_X(0x1, regs[0]);
    ASSERT_EQUAL_X(0xcc, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("CF LSDF", PROG(lsdf1_test_prog1), 1, setup_lbndf1, NULL);
    ASSERT_EQUAL_X(0x1, regs[0]);
    ASSERT_EQUAL_X(0xcc, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);

    run_test("C7 LSNF", PROG(lsnf1_test_prog2), 1, setup_lbndf1, NULL);
    ASSERT_EQUAL_X(0x3, regs[0]);
    ASSERT_EQUAL_X(0xcc, D);
    ASSERT_EQUAL_X(0, P);
    ASSERT_EQUAL_X(0x5, X);
}

/* Test LDXA instruction */
void
setup_ldxa1(void *data)
{
   if ((*(int *)data) != 0) {
       regs[*(int *)data] = 0x09;
   }
   X = *(int *)data;
}

CTEST(cpu, ldxa)
{
    int  i, j;
    char ldxa_test_prog1[] = { 0x72, 0x00, 0x00, 0x00, 0x00, 0x40, 0x09, 0x00, 0x00, 0xe0 };

    for (i = 0x1; i < 0x10; i++) {
        sprintf(name_buffer, " %02x LDAX", 0x72);
        j = i & 0xf;
        run_test(name_buffer, PROG(ldxa_test_prog1), 1, setup_ldxa1, (void *)&j);
        ASSERT_EQUAL_X(0xe0, D);
        ASSERT_EQUAL_X(0xA, regs[j] );
        ASSERT_EQUAL_X(0xe0, memory[9]);
        ASSERT_EQUAL_X(0, P);
        ASSERT_EQUAL_X(j, X);
    }
}

/* Test STXD instruction */
void
setup_stxd1(void *data)
{
   D = 0x2c;
   if ((*(int *)data) != 0) {
       regs[*(int *)data] = 0x09;
   }
   X = *(int *)data;
}

CTEST(cpu, stxd)
{
   int   i, j;
   char stxd_test_prog1[] = { 0x73, 0x00, 0x00, 0x00, 0x00, 0x00 };

   for (i = 0x1; i < 0xF; i++) {
        sprintf(name_buffer, " 73 STXD");
        j = i & 0xf;
        run_test(name_buffer, PROG(stxd_test_prog1), 1, setup_stxd1, &j);
        ASSERT_EQUAL_X(0x2c, D);
        ASSERT_EQUAL_X(0x8, regs[j] );
        ASSERT_EQUAL_X(0x2c, memory[9]);
        ASSERT_EQUAL_X(0, P);
        ASSERT_EQUAL_X(j, X);
   }
}

/* Test alu instruction */
char *alu_opcodes[] = {
    "LDX", "OR", "AND", "XOR", "ADD", "SD", "SHR", "SM",
    "LDI", "ORI", "ANDI", "XORI", "ADDI", "SDI", "SHL", "SMI"
};

void
setup_alu1(void *data)
{
   int     r = *(int *)data;

   if ((r & 0xf) != 0) {
       regs[r & 0xf] = 0x02;
   }
   D = 0xc1;
   X = r;
   DF = 0;
}

struct {
   int P, D, DF;
} alu_result1[] = {
   { 1, 0x54, 0 },        /* LDX */
   { 1, (0x54|0xc1) & 0xff, 0 },   /* OR */
   { 1, (0x54&0xc1) & 0xff, 0 },   /* AND */
   { 1, (0x54^0xc1) & 0xff, 0 },   /* XOR */
   { 1, (0x54+0xc1) & 0xff, 1 },   /* ADD */
   { 1, (0x54-0xc1) & 0xff, 0 },   /* SD */
   { 1, (0xc1 >> 1) & 0xff, 1 },   /* SHR */
   { 1, (0xc1-0x54) & 0xff, 1 },   /* SM */
   { 2, 0x45, 0 },        /* LDI */
   { 2, (0x45|0xc1) & 0xff, 0 },   /* ORI */
   { 2, (0x45&0xc1) & 0xff, 0 },   /* ANI */
   { 2, (0x45^0xc1) & 0xff, 0 },   /* XRI */
   { 2, (0x45+0xc1) & 0xff, 1 },   /* ADI */
   { 2, (0x45-0xc1) & 0xff, 0 },   /* SDI */
   { 1, (0xc1 << 1) & 0xff, 1 },   /* SHL */
   { 2, (0xc1-0x45) & 0xff, 1 },   /* SMI */
};

CTEST(cpu, alu1)
{
    int i, j;
    char alu_test_prog1[] =  {0xF0, 0x45, 0x54 };

    for (i = 0xf0; i < 0x100; i++) {
         alu_test_prog1[0] = i;
         for (j = 1; j < 0x10; j++) {
             sprintf(name_buffer, " %02x %s", i, alu_opcodes[i & 0xf]);
             run_test(name_buffer, PROG(alu_test_prog1), 1, setup_alu1, &j);
             ASSERT_EQUAL_X(0x02, regs[j]);
             ASSERT_EQUAL_X(alu_result1[i & 0xf].P, regs[0]);
             ASSERT_EQUAL_X(j, X);
             ASSERT_EQUAL_X(0, P);
             ASSERT_EQUAL_X(alu_result1[i & 0xf].D, D);
             ASSERT_EQUAL_X(alu_result1[i & 0xf].DF, DF);
         }
    }
}

void
setup_alu2(void *data)
{
   int     r = *(int *)data;

   if ((r & 0xf) != 0) {
       regs[r & 0xf] = 0x02;
   }
   D = 0x01;
   X = r;
   DF = 1;
}

struct {
   int P, D, DF;
} alu_result2[] = {
   { 1, 0x04, 1 },                 /* LDX */
   { 1, (0x04|0x01) & 0xff, 1 },   /* OR */
   { 1, (0x04&0x01) & 0xff, 1 },   /* AND */
   { 1, (0x04^0x01) & 0xff, 1 },   /* XOR */
   { 1, (0x04+0x01) & 0xff, 0 },   /* ADD */
   { 1, (0x04-0x01) & 0xff, 1 },   /* SD */
   { 1, (0x01 >> 1) & 0xff, 1 },   /* SHR */
   { 1, (0x01-0x04) & 0xff, 0 },   /* SM */
   { 2, 0x05, 1 },        /* LDI */
   { 2, (0x05|0x01) & 0xff, 1 },   /* ORI */
   { 2, (0x05&0x01) & 0xff, 1 },   /* ANI */
   { 2, (0x05^0x01) & 0xff, 1 },   /* XRI */
   { 2, (0x05+0x01) & 0xff, 0 },   /* ADI */
   { 2, (0x05-0x01) & 0xff, 1 },   /* SDI */
   { 1, (0x01 << 1) & 0xff, 0 },   /* SHL */
   { 2, (0x01-0x05) & 0xff, 0 },   /* SMI */
};

CTEST(cpu, alu2)
{
    int i, j;
    char alu_test_prog2[] =  {0xF0, 0x05, 0x04 };

    for (i = 0xf0; i < 0x100; i++) {
         alu_test_prog2[0] = i;
         for (j = 0x1; j < 0x10; j++) {
             sprintf(name_buffer, " %02x %s", i, alu_opcodes[i & 0xf]);
             run_test(name_buffer, PROG(alu_test_prog2), 1, setup_alu2, &j);
             ASSERT_EQUAL_X(0x02, regs[j & 0xf]);
             ASSERT_EQUAL_X(alu_result2[i & 0xf].P, regs[0]);
             ASSERT_EQUAL_X((j & 0xf), X);
             ASSERT_EQUAL_X(0, P);
             ASSERT_EQUAL_X(alu_result2[i & 0xf].D, D);
             ASSERT_EQUAL_X(alu_result2[i & 0xf].DF, DF);
         }
    }
}

char *alu_opcodes2[] = {
    NULL, NULL, NULL, NULL, "ADC", "SDB", "SHRC", "SMB",
    NULL, NULL, NULL, NULL, "ADCI", "SDBI", "SHLC", "SMBI"
};

void
setup_alu3(void *data)
{
   int     r = *(int *)data;

   if ((r & 0xf) != 0) {
       regs[r & 0xf] = 0x02;
   }
   D = 0xc1;
   X = r;
   DF = 0;
}

struct {
   int P, D, DF;
} alu_result3[] = {
   { 1, 0xc1, 0 },        
   { 1, 0xc1, 0 },        
   { 1, 0xc1, 0 },        
   { 1, 0xc1, 0 },        
   { 1, (0x54+0xc1) & 0xff, 1 },   /* ADC */
   { 1, (0x54-0xc1) & 0xff, 0 },   /* SDB */
   { 1, (0xc1 >> 1) & 0xff, 1 },   /* SHRC */
   { 1, (0xc1-0x54) & 0xff, 1 },   /* SMB */
   { 1, 0xc1, 0 },        
   { 1, 0xc1, 0 },        
   { 1, 0xc1, 0 },        
   { 1, 0xc1, 0 },        
   { 2, (0x45+0xc1) & 0xff, 1 },   /* ADCI */
   { 2, (0x45-0xc1) & 0xff, 0 },   /* SDBI */
   { 1, (0xc1 << 1) & 0xff, 1 },   /* SHLC */
   { 2, (0xc1-0x45) & 0xff, 1 },   /* SMBI */
};

CTEST(cpu, alu3)
{
    int i, j;
    char alu_test_prog1[] =  {0x70, 0x45, 0x54 };

    for (i = 0x74; i < 0x80; i++) {
         if ((i & 0x4) == 0) {
             continue;
         }
         alu_test_prog1[0] = i;
         for (j = 1; j < 0x10; j++) {
             sprintf(name_buffer, " %02x %s", i, alu_opcodes2[i & 0xf]);
             run_test(name_buffer, PROG(alu_test_prog1), 1, setup_alu3, &j);
             ASSERT_EQUAL_X(0x02, regs[j]);
             ASSERT_EQUAL_X(alu_result3[i & 0xf].P, regs[0]);
             ASSERT_EQUAL_X(j, X);
             ASSERT_EQUAL_X(0, P);
             ASSERT_EQUAL_X(alu_result3[i & 0xf].D, D);
             ASSERT_EQUAL_X(alu_result3[i & 0xf].DF, DF);
         }
    }
}

void
setup_alu4(void *data)
{
   int     r = *(int *)data;

   if ((r & 0xf) != 0) {
       regs[r & 0xf] = 0x02;
   }
   D = 0x01;
   X = r;
   DF = 1;
}

struct {
   int P, D, DF;
} alu_result4[] = {
   { 1, 0x01, 0 },        
   { 1, 0x01, 0 },        
   { 1, 0x01, 0 },        
   { 1, 0x01, 0 },        
   { 1, (0x04+0x01+1) & 0xff, 0 },   /* ADC */
   { 1, (0x04-0x02) & 0xff, 1 },   /* SDB */
   { 1, 0x80, 1 },   /* SHRC */
   { 1, (0x01-0x04-1) & 0xff, 0 },   /* SMB */
   { 1, 0x01, 0 },        
   { 1, 0x01, 0 },        
   { 1, 0x01, 0 },        
   { 1, 0x01, 0 },        
   { 2, (0x05+0x01+1) & 0xff, 0 },   /* ADCI */
   { 2, (0x05-0x02) & 0xff, 1 },   /* SDBI */
   { 1, 0x03, 0 },   /* SHLC */
   { 2, (0x01-0x05-1) & 0xff, 0 },   /* SMBI */
};

CTEST(cpu, alu4)
{
    int i, j;
    char alu_test_prog2[] =  {0xF0, 0x05, 0x04 };

    for (i = 0x74; i < 0x80; i++) {
         if ((i & 0x4) == 0) {
             continue;
         }
         alu_test_prog2[0] = i;
         for (j = 0x1; j < 0x2; j++) {
             sprintf(name_buffer, " %02x %s", i, alu_opcodes2[i & 0xf]);
             run_test(name_buffer, PROG(alu_test_prog2), 1, setup_alu4, &j);
             ASSERT_EQUAL_X(0x02, regs[j & 0xf]);
             ASSERT_EQUAL_X(alu_result4[i & 0xf].P, regs[0]);
             ASSERT_EQUAL_X((j & 0xf), X);
             ASSERT_EQUAL_X(0, P);
             ASSERT_EQUAL_X(alu_result4[i & 0xf].D, D);
             ASSERT_EQUAL_X(alu_result4[i & 0xf].DF, DF);
         }
    }
}

/* Test MARK instruction */
void
setup_mark(void *data)
{
    X = 4;
    regs[2] = 0x20;
    regs[4] = 0x10;
}

CTEST(cpu, mark)
{
    char c = 0x79;

    sprintf(name_buffer, " %02x MARK", c);
    run_test(name_buffer, &c, 1, 1, setup_mark, NULL);
    ASSERT_EQUAL_X(0x01, regs[0]);
    ASSERT_EQUAL_X(0x1F, regs[2]);
    ASSERT_EQUAL_X(0x10, regs[4]);
    ASSERT_EQUAL_X(0, X);
    ASSERT_EQUAL_X(0x40, memory[0x20]);
    ASSERT_EQUAL_X(0xcc, D);
    ASSERT_EQUAL_X(0, P);
}

/* Test SAV instruction */
void
setup_sav(void *data)
{
    T = 0x45;
    X = 4;
    regs[2] = 0x20;
    regs[4] = 0x10;
}

CTEST(cpu, sav)
{
    char c = 0x78;

    sprintf(name_buffer, " %02x SAV", c);
    run_test(name_buffer, &c, 1, 1, setup_sav, NULL);
    ASSERT_EQUAL_X(0x01, regs[0]);
    ASSERT_EQUAL_X(0x20, regs[2]);
    ASSERT_EQUAL_X(0x10, regs[4]);
    ASSERT_EQUAL_X(4, X);
    ASSERT_EQUAL_X(0x45, memory[0x10]);
    ASSERT_EQUAL_X(0xcc, D);
    ASSERT_EQUAL_X(0, P);
}

/* Test DIS instruction */
void
setup_dis(void *data)
{
    T = 0x45;
    X = 4;
    regs[2] = 0x20;
    regs[4] = 0x10;
    memory[0x10] = 0x27;
    I = *(int *)data;
}

CTEST(cpu, dis)
{
    char c = 0x71;
    int  i = 1;

    sprintf(name_buffer, " %02x DIS", c);
    run_test(name_buffer, &c, 1, 1, setup_dis, &i);
    ASSERT_EQUAL_X(0x01, regs[0]);
    ASSERT_EQUAL_X(0x20, regs[2]);
    ASSERT_EQUAL_X(0x11, regs[4]);
    ASSERT_EQUAL_X(2, X);
    ASSERT_EQUAL_X(7, P);
    ASSERT_EQUAL_X(0, I);
    ASSERT_EQUAL_X(0x45, T);
    ASSERT_EQUAL_X(0x27, memory[0x10]);
}

/* Test RET instruction */
CTEST(cpu, ret)
{
    char c = 0x70;
    int  i = 0;

    sprintf(name_buffer, " %02x RET", c);
    run_test(name_buffer, &c, 1, 1, setup_dis, &i);
    ASSERT_EQUAL_X(0x01, regs[0]);
    ASSERT_EQUAL_X(0x20, regs[2]);
    ASSERT_EQUAL_X(0x11, regs[4]);
    ASSERT_EQUAL_X(2, X);
    ASSERT_EQUAL_X(7, P);
    ASSERT_EQUAL_X(1, I);
    ASSERT_EQUAL_X(0x45, T);
    ASSERT_EQUAL_X(0x27, memory[0x10]);
}

/* Test long interrupt enabled instructions */
void
setup_sie(void *data)
{
    T = 0x45;
    X = 4;
    regs[2] = 0x20;
    regs[4] = 0x10;
    memory[0x10] = 0x20;
}

CTEST(cpu, lsie)
{
    char lsie1_test_prog1[] =  {0x70, 0xcc, 0x34, 0x56 };
    char lsnie1_test_prog2[] = {0x71, 0xcc, 0x34, 0x56 };

    run_test("CE LSZ", PROG(lsie1_test_prog1), 2, setup_sie, NULL);
    ASSERT_EQUAL_X(0x4, regs[0]);

    run_test("C6 LSNZ", PROG(lsnie1_test_prog2), 2, setup_sie, NULL);
    ASSERT_EQUAL_X(0x2, regs[0]);

    run_test("CE LSZ", PROG(lsie1_test_prog1), 2, setup_sie, NULL);
    ASSERT_EQUAL_X(0x4, regs[0]);

    run_test("C6 LSNZ", PROG(lsnie1_test_prog2), 2, setup_sie, NULL);
    ASSERT_EQUAL_X(0x2, regs[0]);
}

/* Test interrupt */
void
setup_irq(void *data)
{
    T = 0x45;
    X = 4;
    regs[1] = 0x05;
    regs[2] = 0x20;
    regs[4] = 0x10;
    memory[0x10] = 0x52;
    memory[0x20] = 0xc4;
    line = 78;      /* Trigger IRQ */
    display_on = 1;
    I = 0;
}

CTEST(cpu, irq)
{
    char irq_test = 0x70;

    sprintf(name_buffer, " irq");
    run_test(name_buffer, &irq_test, 1, 4, setup_irq, NULL);
    ASSERT_EQUAL_X(0x01, regs[0]);
    ASSERT_EQUAL_X(0x06, regs[1]);
    ASSERT_EQUAL_X(0x11, regs[4]);
    ASSERT_EQUAL_X(2, X);
    ASSERT_EQUAL_X(1, P);
    ASSERT_EQUAL_X(0, I);
    ASSERT_EQUAL_X(0x52, T);
    ASSERT_EQUAL_X(0x52, memory[0x10]);
}



