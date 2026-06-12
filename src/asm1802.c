/*
 * 1802 - Assembler
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "optab.h"

#define BEGIN   10   /* Read new line in */
#define START   11   /* Start of statement */
#define NEXT    12   /* Start line processing over */
#define OP      13   /* Process opcode */
#define EQU     14   /* Process equate statement */
#define CONST   15   /* Process DC or , */
#define TERM    16   /* Look for name or constant. */
#define OFFSET  17   /* Check of + or - */
#define EXPR    18   /* Pick up trailing constant. */
#define TEXT    19   /* Process a text string. */
#define REG     20   /* Process register operator */
#define FINISH  21   /* Finish up processing an opcode */
#define END     22   /* End of assembly */
#define DONE    23   /* Process end of statement */

#define EQ      30  /* Equate operator */
#define PAG     31  /* Emit end of page */
#define ORG     32  /* Set origin */
#define DC      33  /* Define constant */

struct _symbol_table {
    char name[6];
    uint16_t   value;
} symbol_table[1024];

char     *special[] = { "EQU   ", "PAGE  ", "ORG   ", "END   ", "DC    ", NULL};
int       svalue[] = {   EQ,       PAG,      ORG,      END,      DC,      0 };
char      buffer[256];           /**> Input buffer */
char      symbol[6];             /**> Collected symbol in input */
uint16_t  value;                 /**> Current value of expression */
int       pass_num;              /**> Current pass number */
int       outputed;              /**> Line has been typed out */
char     *line_ptr;              /**> Pointer to current character in buffer */
int       bytes = 0;             /**> Number of bytes typed on current output */
int       line_num = 0;          /**> Current line number */
int       acon;                  /**> Generating a A(...) or A.?(....) */
int       aoff;                  /**> Which bytes of address to output */
struct _symbol_table *label;     /**> Current label on line */
struct _symbol_table *expr;      /**> Current symbolic name in expressiont */
struct _symbol_table location;   /**> Current location counter. */
FILE    *input = NULL;
FILE    *output = NULL;
FILE    *bin_out = NULL;
int      bin_cnt;
uint16_t last_page;              /**> Highest location for code. */
uint16_t start_page;             /**> Start page to dump */
uint8_t  memory[64 * 1024];      /**> Hold generated output */


int hex_digit(char c);
void error(int err);
void dumpline();
int skip_blanks();
struct _symbol_table * lookup_symbol(int define);
void clear_symbol();
void define_symbol(uint16_t value) ;
int scan_symbol(char ch, int emit_byte);
void hex(char ch, int emit_byte);
void decimal(char ch, int emit_byte);
void binary(char ch, int emit_byte);
void emit(uint8_t byte);
int pass();

/* Error codes:
 *
 *   01    invalid opcode
 *   02    previous defined symbol
 *   04    invalid binary character
 *   05    binary more then 8 bits
 *   06    expect or decimal constant
 *   07    undefined symbol
 *   08    expected expression
 *   09    invalid character in hex constant
 *   10    missing trailing quote
 *   11    period error
 *   12    leading char before error.
 *   14    branch out of page.
 *   15    invalid register number
 *   16    device number out of range.
 *   OVFL  symbol table overflowed.
 *
 */

/**
 * @brief 1802 Assembler.
 *
 * Scan arguments: [-l <filename>] <filename>
 *     -l <file>  sets listing file name.
 *     <file>     sets input file name.
 *
 * If no output file, output to standard out,
 *
 * Set pass number to 1, run pass().
 *
 * Rewind input file, and set pass number to 2, run pass over file.
 */
int main(int argc, char *argv[])
{
    int i;

    /* Scan arguments */
    for ( i = 1; i < argc; i++) {
         char *p = argv[i];
         if (*p == '-') {
             p++;
             /* Set listing file instead of stdout */
             if(*p == 'l') {
                 if (output != NULL) {
                     fprintf(stderr, "Duplicate output file\n");
                     exit(1);
                 }
                 output = fopen(argv[++i],"w");
                 if (output == NULL) {
                     fprintf(stderr, "Unable to open output: %s\n", argv[i]);
                     exit(1);
                 }
              }
              /* Write binary output to file, optional starting page for binary */
              if (*p == 'b') {
                  if (bin_out != NULL) {
                      fprintf(stderr, "Duplicate output file\n");
                      exit(1);
                  }
                  bin_out = fopen(argv[++i],"w");
                  if (bin_out == NULL) {
                      fprintf(stderr, "Unable to open output: %s\n", argv[i]);
                      exit(1);
                  }
                  start_page = 0;
                  last_page = 0;
                  for(p++; *p != '\0'; p++) {
                      if (!isxdigit(*p)) {
                          break;
                      }
                      start_page = (start_page << 4) + hex_digit(*p);
                  }
                  start_page &= 0xff;
                  start_page <<= 8;
               }
         } else {
             if (input != NULL) {
                 fprintf(stderr, "Duplicate input file\n");
                 exit(1);
             }
             input = fopen(argv[i], "r");
             if (input == NULL) {
                 fprintf(stderr, "Unable to open input: %s\n", argv[i]);
                 exit(1);
             }
         }
    }

    /* If no output given, output to stdout */
    if (output == NULL) {
        output = stdout;
    }

    /* Run first pass to collect symbols. */
    pass_num = 1;
    location.value = 0;
    line_num = 0;
    pass();

    /* Rewind and run pass again to output binary */
    rewind(input);
    pass_num++;
    line_num = 0;
    location.value = 0;
    outputed = 0;
    pass();
    dumpline();

    /* Dump binary if requested */
    last_page = (last_page | 0x00ff) + 1;
    if (bin_out != NULL && start_page < last_page) {
        fwrite(&memory[start_page], last_page - start_page, sizeof(uint8_t), bin_out);
        fclose(bin_out);
    }

    /* Close files */
    fclose(input);
    fclose(output);
}

/**
 * @brief convert Hex character to binary.
 *
 * @param c   ASCII character.
 * @return    binary value.
 */
int
hex_digit(char c)
{
    static char hex_str[] = "0123456789ABCDEF";
    int   i;

    for(i = 0; i < sizeof(hex_str); i++) {
        if (toupper(c) == hex_str[i]) {
            return i;
        }
    }
    return 0;
}

/**
 * @brief Print out error message.
 *
 * Print out the offending line, followed by spacing to
 * location of current scan pointer, print '?'. On next
 * line print error number.
 */
void
error(int err)
{
    char *p;

    if (pass_num == 1) {
        if (err == 17) {
            fprintf(output, "OVFL\n");
        }
        fprintf(output, "%04d %s     ", line_num, buffer);
    } else {
        dumpline();
        fputs("                                     ", output);
    }
    for (p = buffer; p != line_ptr && *p != '\0'; p++) {
         putc(' ', output);
    }
    fprintf(output, "?\n%d\n", err);
}

/**
 * @brief Dump line to output.
 *
 * Output current line.
 */
void
dumpline()
{
   if (pass_num == 1) {
      return;
   }
   putc(';', output);
   if (outputed == 0) {
      while(bytes < 7) {
         putc(' ', output);
         putc(' ', output);
         bytes++;
      }
      fprintf(output, "  %04d        %s", line_num, buffer);
      outputed = 1;
   } else {
      putc('\n', output);
   }
   bytes = 0;
}

/**
 * @brief Skip blanks in input stream.
 *
 * Return 0 if at non-blank character.
 *        1 if at end  of input.
 */
int
skip_blanks()
{
    /* Skip leading blanks */
    while(*line_ptr == ' ' || *line_ptr == '\t') {
       line_ptr++;
    }
    /* Check if end of line */
    if (*line_ptr == '\n' || *line_ptr == '\r' || *line_ptr == '\0') {
        return 1;
    }
    return 0;
}

/**
 * @brief Lookup symbol in symbol.

 * Lookup a symbol in the symbol table.
 * Return pointer to empty symbol, or matching symbol.
 * Return NULL on no more space in symbol table.
 */
struct _symbol_table *
lookup_symbol(int define)
{
    int   hash = 0;  /* Hash of symbol */
    int   i;
    int   loop = 0;  /* Scanned full table */
    int   match;

    hash = 0;
    /* Compute initial hash */
    for (i = 0; i <sizeof(symbol); i++) {
         hash = (hash << 2) + symbol[i];
    }

    /* Compute initial entry */
    hash = hash % (sizeof(symbol_table)/sizeof(struct _symbol_table));

    while(1) {
       match = 1;
       /* If space is empty, found it */
       if (symbol_table[hash].name[0] == 0) {
           if (define) {
               for (i = 0; i < sizeof(symbol); i++) {
                   symbol_table[hash].name[i] = symbol[i];
               }
           }
           return &symbol_table[hash];
       }

       /* Check if match */
       for (i = 0; i <sizeof(symbol); i++) {
           if (symbol[i] != (0x7f & symbol_table[hash].name[i])) {
               match = 0;
               break;
           }
       }

       /* Found it */
       if (match) {
           return &symbol_table[hash];
       }

       /* See if next symbol */
       hash++;
       if (hash >= sizeof(symbol_table)/sizeof(struct _symbol_table)) {
           if (loop) {          /* If we went around twice table full */
              if (define) {
                  error(17);    /* Symbol table overflow */
              }
              return NULL;
           }
           hash = 0;
           loop = 1;
      }
   }

   /* We hit end of table */
   return NULL;
}

/**
 * @brief Clear symbol variable.
 */
void
clear_symbol()
{
    int i;

    for(i = 0; i < sizeof(symbol); i++) {
        symbol[i] = ' ';
    }
}

/**
 * @brief Copy symbol variable to current label.
 */
void
define_symbol(uint16_t value)
{
    int i;

    if (pass_num == 2) {
        return;
    }
    label->value = value;
    label->name[0] |= 0x80;
    fprintf (output, "     %04x ", label->value);
    for (i = 0; i < sizeof(symbol) && label->name[i] != ' '; i++) {
        putc(label->name[i] & 0x7f, output);
    }
    putc('\n', output);
}

/**
 * @brief Scan a text string.
 *
 * Detect special constant or symbol name.
 */
int
scan_symbol(char ch, int emit_byte)
{
    int     i;

    acon = 0;
    if (*line_ptr == '\'') {
        switch(ch) {
        case 'X':
               hex('\'', emit_byte);
               break;
        case 'D':
               decimal('\'', emit_byte);
               break;
        case 'B':
               binary('\'', emit_byte);
               break;
        case 'T':
               return TEXT;
        default:
               break;
        }
        return FINISH;
    }
    if (ch == 'A' && !isalnum(*line_ptr)) {
        aoff = 3;
        if (line_ptr[0] == '.' && line_ptr[1] == '0') {
            aoff = 1;
            line_ptr += 2;
        } else if (line_ptr[0] == '.' && line_ptr[1] == '1') {
            aoff = 2;
            line_ptr += 2;
        }
        ch = *line_ptr;
        if (ch == '(') {
            acon = 1;
            line_ptr++;
            if (skip_blanks()) {
                error(6); /* invalid statement terminater */
                return START;
            }
            ch = *line_ptr++;
            if (ch == '*') {
                expr = &location;
                value = expr->value;
                return OFFSET;
            }
        }
    }

    if (isalpha(ch)) {
        /* Pick up symbol and see if defined */
        clear_symbol();
        for(i = 0; i < sizeof(symbol); i++) {
            symbol[i] = toupper(ch);
            if (!isalnum(*line_ptr)) {
                break;
            }
            ch = toupper(*line_ptr++);
        }

        expr = lookup_symbol(0);
        if (expr == NULL) {
            return BEGIN;
        }
        if (pass_num == 2 && (expr->name[0] & 0x80) == 0) {
             error(7);
        }
        value = expr->value;
    } else {
        error(1); /* Syntax error */
        return BEGIN;
    }
    return OFFSET;
}

/**
 * @brief Scan hex constant.
 *
 * Parse hex number, if e emit it. Enter with matching character.
 * @param ch        First character scanned.
 * @param emit_byte Emit values for every 2 character found.
 */
void
hex(char ch, int emit_byte)
{
    char     term = 0;
    char     c;
    int      digits;

    if (ch == '\'') { /* If qouted, skip */
       term = *line_ptr++;
    }
    digits = 0;
    while (isxdigit(*line_ptr)) {
       c = *line_ptr++;
       digits++;
       value = (value << 4) | hex_digit(c);
       if (emit_byte && digits == 2) {
           emit(value);
           digits = 0;
           value = 0;
       }
   }
   if (term != 0) {
       if (term != *line_ptr++) {
           error(4); /* Missing terminator */
       }
   }
}

/**
 * @brief Scan a decimal number.
 *
 * Parse decimal number, if e emit it.
 * Enter with matching character.
 */
void
decimal(char ch, int emit_byte)
{
    char     term = 0;
    char     c;
    int      digits;

    if (ch == '\'') { /* If qouted, skip */
       term = *line_ptr++;
    }
    digits = 0;
    while (isdigit(*line_ptr)) {
       c = *line_ptr++;
       digits++;
       value = (value * 10) + (c - '0');
   }
   if (term != 0) {
       if (term != *line_ptr++) {
           error(4); /* Missing terminator */
       }
   }
   if (emit_byte) {
      if (value >= 0x0100) {
          emit((value >> 8) & 0xff);
      }
      emit(value & 0xff);
   }
}

/**
 * @brief Scan a binary number.
 *
 * Parse binary number, if e emit it.
 * Enter with matching character.
 */
void
binary(char ch, int emit_byte)
{
    char     term = 0;
    char     c;
    int      digits;

    if (ch == '\'') { /* If qouted, skip */
       term = *line_ptr++;
    }
    digits = 0;
    while (*line_ptr == '0' || *line_ptr == '1') {
       c = *line_ptr++;
       digits++;
       value = (value << 1) | (c - '0');
   }

   if (term != 0) {
       if (term != *line_ptr++) {
           error(4); /* Missing terminator */
       }
   }
   if (digits > 8) {
       error(5);    /* More then 8 digits */
   }
   if (emit_byte) {
      emit(value & 0xff);
   }
}

/**
 * @brief Emit next byte.
 *
 * Output a byte. If over 6 bytes, print new line.
 */
void
emit(uint8_t byte)
{
     if (pass_num == 2) {
         if (bytes == 7) {
             dumpline();
             bytes = 0;
             fprintf(output, "%04X    ", location.value);
         }
         bytes++;
         fprintf(output, "%02X", byte);
         if (bin_out != NULL) {
             memory[location.value] = byte;
             if (location.value > last_page) {
                 last_page = location.value;
             }
         }
     }
     location.value++;
}

/**
 * @brief run over input.
 *
 */
int pass()
{
    int        state = BEGIN;
    int        emit_byte;
    int        i;
    int        j;
    char       ch;
    int        type;
    int        match;
    const struct opcode *op;
    char       opr;

    while (state != END) {
        switch (state) {
        case BEGIN:    /* Current line buffer is empty, fill it */
                 line_ptr = fgets(buffer, sizeof(buffer), input);
                 if (line_ptr == NULL) {
                     return 0;   /* Done with file */
                 }
                 line_num++;
                 state = START;
                 if (pass_num == 2) {
                     fprintf(output, "%04X    ", location.value);
                 }
                 bytes = 0;
                 outputed = 0;
                 /* Fall through */

        case START:   /* Look for valid start of line */
                 /* start can be:
                  * .. comment.
                  * symbol:
                  * opcode
                  * ,      Constant.
                  * ;      end of statement.
                  *
                  * symbol or opcode can be terminated by:
                  *   : indicating label.
                  *   ; end of statement.
                  *   . start of comment.
                  *   = equate name to value.
                  */
                 label = NULL;

                 /* Check if end of line */
                 if (skip_blanks()) {
                     dumpline();
                     state = BEGIN;
                     break;
                 }
                 /* Fall through */

        case NEXT:   /* Look for valid start of line */
                 ch = *line_ptr++;
                 /* Check if comment */
                 if (ch == '.') {
                     if (*line_ptr == '.') {
                         dumpline();
                     } else {
                         error(11); /* Missing period to start comment */
                     }
                     state = BEGIN;
                     break;
                 }

                 /* Check if constant */
                 if (ch == ',') {
                     state = CONST;
                     break;
                 }

                 /* If ; start over */
                 if (ch == ';') {
                     break;
                 }

                 /* Convert to upper case */
                 ch = toupper(ch);
                 if (!isalpha(ch)) {
                     line_ptr--;
                     error(22); /* Statement does not begin with symbol or opcode */
                     state = BEGIN;
                     break;
                 }
                 /* Collect name */
                 clear_symbol();
                 symbol[0] = ch;
                 state = START;
                 i = 1;
                 while(i <= sizeof(symbol) && state == START) {
                     ch = toupper(*line_ptr);
                     switch(ch) {
                     case '.':
                     case '\n':
                     case '\0':
                     case ' ':
                     case '\t':
                     case '#':
                     case '*':
                     case ',':
                     case ';':
                         state = OP;   /* See if we got opcode */
                         break;

                     case '=':
                         label = lookup_symbol(1);
                         if (label == NULL) {
                              state = END;
                              break;
                         }
                         line_ptr++;
                         state = EQU;  /* Got an equate */
                         break;

                     case ':':
                         /* Got label, look it up */
                         label = lookup_symbol(1);
                         if (label == NULL) {
                              state = END;
                              break;
                         }
                         /* If first pass and define, error */
                         if (pass_num == 1) {
                              if ((label->name[0] & 0x80) != 0) {
                                  error(2);   /* Attempt to redefine symbol */
                              }
                         }
                         line_ptr++;
                         if (skip_blanks()) {
                            define_symbol(location.value);
                            state = BEGIN;
                         } else {
                            state = NEXT;
                         }
                         clear_symbol();
                         break;

                     default:
                         if (!isalnum(ch)) {
                             error(22); /* Invalid start of statement */
                             state = BEGIN;
                         }
                         line_ptr++;
                         if (i == sizeof(symbol)) {
                             error(32); /* Invalid start of statement */
                             state = BEGIN;
                         }
                         symbol[i++] = ch;
                         break;
                     }
                 }
                 break;

          case OP: /* Look up opcode in table of opcodes or check if special opcode */
                 match = 0;
                 for (op = &opcode_map[0]; op->name != NULL && match != 1; op++) {
                      match = 1;
                      for (i = 0; i < sizeof(symbol) && op->name[i] != '\0'; i++) {
                           if (op->name[i] != symbol[i]) {
                               match = 0;
                               break;
                           }
                      }
                      if (match && i < sizeof(symbol) && symbol[i] != ' ') {
                          match = 0;
                      }
                 }
                 op--;
                 type = op->type;
                 /* Not found in opcode table, check if special name */
                 if (!match) {
                     for (j = 0; special[j] != NULL && match != 1;j++) {
                          const char *s = special[j];
                          type = svalue[j];
                          match = 1;
                          for (i = 0; i < sizeof(symbol); i++) {
                               if (s[i] != symbol[i]) {
                                   match = 0;
                                   break;
                               }
                          }
                     };
                     if (!match) {
                         error(1); /* Opcode not found */
                         state = DONE;
                     }
                 }

                 emit_byte = 0;
                 if (type == EQ) {
                     state = EQU;
                     break;
                 }
                 if (type == DC) {
                     state = CONST;
                     break;
                 }
                 if (label != 0) {
                     if (pass_num == 2 && label->value != location.value) {
                         error(2);  /* Symbol redefined */
                     }
                     define_symbol(location.value);
                     label = NULL;
                 }
                 switch(type) {
                 case OPR:  emit(op->base);
                            state = FINISH; break;  /* Loop for end of statement */
                 case DC:   state = CONST; break;
                 case ORG:
                 case OPI:
                 case OPL:
                 case OPB:
                 case OPO:
                            state = TERM; break;    /* Look for expression */
                 case OPN:  state = REG; break;     /* Look for expression */

                 case PAG:  state = FINISH; break;  /* Look for end of statement */

                 case END:
                 default:
                            state = END; break;     /* Look for end of statement */

                 }
                 break;

          case CONST:  /* Process a constant expression */
                 type = DC;
                 if (label != NULL) {
                     if (pass_num == 2 && label->value != location.value) {
                         error(2);  /* Symbol redefined */
                     }
                     define_symbol(location.value);
                     label = NULL;
                 }
                 if (skip_blanks()) {
                     error(8); /* Missing expression */
                     state = FINISH;
                     break;
                 }

                 opr = '\0';
                 /* Clear symbol buffer */
                 clear_symbol();

                 value = 0;
                 ch = toupper(*line_ptr++);
                 emit_byte = 0;
                 expr = NULL;
                 switch (ch) {
                 case ',':
                 case ';':
                 case '.':
                     error(8);   /* Expression missing */
                     state = FINISH;
                     break;

                 case '#':
                     hex(0, 1);  /* Scan hex digit */
                     state = FINISH;
                     break;

                 case '0': case '1': case '2': case '3': case '4':
                 case '5': case '6': case '7': case '8': case '9':
                     line_ptr--;
                     decimal(0, 1);  /* Scan decimal digit */
                     state = FINISH;
                     break;

                 default:
                     state = scan_symbol(ch, 1);
                     if (state == OFFSET && !acon) {
                         emit_byte = 1;
                     }
                 }
                 break;

          case EQU:   /* Process an equate statement */
                 if (label == NULL) {
                      state = END;
                      break;
                 }
                 type = EQ;
                 emit_byte = 0;

                 /* If first pass and define, error */
                 if (pass_num == 1 && (label->name[0] & 0x80) != 0) {
                      error(2);   /* Attempt to redefine symbol */
                      state = DONE;
                 }
                 state = TERM;
                 break;

           case TERM:   /* Process first value of an expression */
                 expr = NULL;
                 value = 0;
                 opr = '\0';
                 acon = 0;
                 aoff = 0;
                 if (skip_blanks()) {
                     error(8); /* Missing expression */
                     state = FINISH;
                     break;
                 }

                 ch = toupper(*line_ptr++);
                 switch (ch) {
                 case ',':
                 case ';':
                 case '.':
                     error(8);   /* Expression missing */
                     state = FINISH;
                     break;

                 case '*':
                     /* Current location */
                     expr = &location;
                     value = expr->value;
                     state = OFFSET;
                     break;

                 case '#':
                     hex(0, emit_byte);  /* Scan hex digit */
                     state = FINISH;
                     break;

                 case '0': case '1': case '2': case '3': case '4':
                 case '5': case '6': case '7': case '8': case '9':
                     line_ptr--;
                     decimal(0, emit_byte);  /* Scan hex digit */
                     state = FINISH;
                     break;

                 default:
                     state = scan_symbol(ch, emit_byte);
                 }
                 break;

           case OFFSET:  /* For symbol or *, allow +/- constant */
                state = FINISH;
                if (skip_blanks()) {
                    break;
                }
                if (*line_ptr == '+' || *line_ptr == '-') {
                    state = EXPR;
                    break;
                }
                if (acon) {
                    if (*line_ptr != ')') {
                        error(4); /* Invalid address constant */
                    } else {
                        line_ptr++;
                    }
                }
                break;

           case EXPR:  /* Constant after name+/- */
                 opr = *line_ptr++;
                 value = 0;
                 state = FINISH;
                 if (skip_blanks()) {
                     error(8); /* Missing expression */
                     break;
                 }

                 ch = toupper(*line_ptr++);
                 switch (ch) {
                 case ',':
                     error(8); /* Missing expression */
                     state = CONST;
                     break;

                 case ';':
                     error(8); /* Missing expression */
                     break;

                 /* Check if comment */
                 case '.':
                     error(8); /* Missing expression */
                     break;

                 case '#':
                     hex(0, 0);  /* Scan hex digit */
                     break;

                 case '0': case '1': case '2': case '3': case '4':
                 case '5': case '6': case '7': case '8': case '9':
                     line_ptr--;
                     decimal(0, 0);  /* Scan hex digit */
                     break;

                 default:
                     if (*line_ptr == '\'') {
                         switch(ch) {
                         case 'X':
                                hex('\'', 0);
                                break;
                         case 'D':
                                decimal('\'', 0);
                                break;
                         case 'B':
                                binary('\'', 0);
                                break;
                         default:
                                break;
                        }
                     }
                 }
                 if (expr != NULL) {
                     if (opr == '+') {
                         value = expr->value + value;
                     } else {
                         value = expr->value - value;
                     }
                 }
                 if (acon) {
                     if (*line_ptr++ != ')') {
                         error(1); /* Invalid address constant */
                     }
                 }
                 break;

           case REG:   /* Process register name, single hex digit,
                        *  Rx, or name.
                        */
                 state = FINISH;
                 if (skip_blanks()) {
                     error(8); /* Missing expression */
                     break;
                 }

                 opr = '\0';
                 /* Clear symbol buffer */
                 clear_symbol();

                 value = 0;
                 ch = *line_ptr++;
                 if (isdigit(ch)) {
                     value = ch - '0';
                     break;
                 }
                 if (isalpha(ch)) {
                     /* Pick up symbol and see if defined */
                     clear_symbol();
                     for(i = 0; i < sizeof(symbol); i++) {
                         symbol[i] = ch;
                         ch = toupper(*line_ptr++);
                         if (!isalnum(ch)) {
                             line_ptr--;
                             break;
                         }
                     }
                     expr = lookup_symbol(0);
                     if (expr == NULL) {
                         break;
                     }
                     if ((expr->name[0] & 0x80) == 0) {
                          i = 0;
                          if (symbol[0] == 'R') {
                              i++;
                          }
                          if (isxdigit(symbol[i]) && symbol[i+1] == ' ') {
                              if (isalpha(symbol[i])) {
                                  value = symbol[i] - 'A' + 10;
                              } else {
                                  value = symbol[i] - '0';
                              }
                          } else {
                              error(7); /* Undefined */
                          }
                     } else {
                          value = expr->value;
                     }
                 }
                 break;

           case TEXT:  /* Process text string */
                 line_ptr++;
                 while (*line_ptr != '\0' && *line_ptr != '\n') {
                     if (*line_ptr == '\'' && *++line_ptr != '\'') {
                        emit_byte = 0;
                        break;
                     }
                     if (type == DC) {
                         emit(*line_ptr++);
                     } else {
                         value = (int)*line_ptr++;
                    }
                 }
                 emit_byte = 0;
                 state = FINISH;
                 break;

           case FINISH:  /* Finish off generation of line */
                 state = START;
                 switch (type) {
                 case DC:
                           if (acon && (aoff & 2) != 0) {
                               emit((value >> 8) & 0xff);
                           }
                           if (emit_byte || (acon && (aoff & 1) != 0)) {
                               emit(value & 0xff);
                           }
                           break;

                 case ORG:
                           if (label != NULL) {
                               label->value = value;
                               label->name[0] |= 0x80;
                           }
                           location.value = value;
                           break;

                 case EQ:
                           if (pass_num == 2 && expr != NULL && (expr->name[0] & 0x80) == 0) {
                                error(7);
                           }
                           if (pass_num == 1 && label != NULL) {
                               define_symbol(value);
                           }
                           break;

                 case OPO:
                           if (value > 7) {  /* Value out of range */
                               error(16);
                               break;
                           }
                           emit(op->base | (value & 0x7));
                           break;

                 case OPB:
                           emit(op->base);
                           if (pass_num == 2 && ((location.value) & 0xff00) != (value & 0xff00)) {
                               error(14);
                           }
                           emit(value & 0xff);
                           break;

                 case OPI:
                           emit(op->base);
                           if (acon && (aoff & 2) != 0) {
                               emit((value >> 8) & 0xff);
                           } else {
                               emit(value & 0xff);
                           }
                           break;

                 case OPN:
                           emit(op->base + (value & 0xf));
                           break;

                 case OPL:
                           emit((value >> 8) & 0xff);
                           emit(value & 0xff);
                           break;

                 case PAG: /* Adjust location to next page. */
                           location.value &= 0xff00;
                           location.value += 0x0100;
                           break;
                 }
                 acon = 0;
                 aoff = 0;
                 break;

          case DONE:  /* Scan for comment, next statement, constant, end of line */
                 /* Skip leading blanks */
                 if (skip_blanks()) {
                     state = END;
                     break;
                 }

                 ch = *line_ptr++;
                 /* Check if comment */
                 if (ch == '.') {
                     if (*line_ptr == '.') {
                         state = BEGIN;
                         break;
                     } else {
                         error(11); /* Missing period to start comment */
                         break;
                     }
                 }

                 /* Check for end of statement */
                 if (ch == ';') {
                      state = START;  /* Go look up another statment */
                      break;
                 }

                 /* If constant handle */
                 if (ch == ',') {
                     state = CONST;
                     break;
                 }
                 error(1);
                 state = BEGIN;     /* Skip rest of line */
                 break;
          }
      }
      return 0;
}

