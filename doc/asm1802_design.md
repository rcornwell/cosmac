# Asm1802 Assembler - Design Document

## 1. Overview

`asm1802.c` is a two-pass cross-assembler for the TI COSMAC 1802 microprocessor. It transforms COSMAC Level I assembly language (defined in `asm1802.md`) into hex code listings and/or binary machine code.

### 1.1 What the 1802 Assembler Targets

The CDP1802 is an 8-bit RISC CPU with a 64KB address space (16-bit address bus). Its instructions are all single-byte opcodes (except long branches, which are 3 bytes) with variable-length operand fields. The ISA defines six instruction operand formats:

| Format | Operand | Example          | Output           |
|--------|---------|------------------|------------------|
| OPR    | —       | `NOP`, `IDL`     | `#base`          |
| OPN    | reg #   | `INC R4`         | `#base + n`      |
| OPB    | address | `BR label`       | `#base #addr`    |
| OPO    | 0-7     | `STK R7`         | `#base | n`      |
| OPI    | #value  | `XRI #FF`        | `#base #val`     |
| OPL    | address | `LBZ label`      | `#base #high #low` |

The assembler reads an `opcode_map[]` table (declared in `optab.h`) that maps each mnemonic to its type, base opcode, and length.

### 1.2 Command-Line Interface

```
asm1802 [-l listing_file] [-b [page] binary_file] source_file
```

| Flag     | Purpose                                           |
|----------|---------------------------------------------------|
| `-l file`| Write the assembled listing to `file` instead of stdout |
| `-b page file` | Write a raw binary dump of generated code to `file`, starting at `page` (hex digits parsed from the same argv token) |

Only one input file and one of each output type are supported. The `-l` option defaults to stdout if omitted.

## 2. High-Level Architecture

### 2.1 Two-Pass Design

```
main:
  ┌─────────────────────────┐
  │ Parse command-line args │
  ├─────────────────────────┤
  │ pass_num = 1            │
  │ location.value = 0      │
  │ pass()                  │  ← First pass: symbol table only
  ├─────────────────────────┤
  │ rewind(input)           │
  │ pass_num = 2            │
  │ location.value = 0      │
  │ outputed = 0            │
  │ pass()                  │  ← Second pass: emit code
  ├─────────────────────────┤
  │ dump binary (if -b)     │
  └─────────────────────────┘
```

**Pass 1 (collection):** Scans the entire source file, building the symbol table and validating syntax. Produces label-value entries in the listing. Never emits machine code.

**Pass 2 (emission):** Re-reads the source file. Now all symbol values are known, so it emits:
- Hex byte code in the listing
- Raw binary via `fwrite()` (if `-b` given)
- Error messages at the same locations as pass 1 for parity

The two-pass design eliminates forward-reference problems. A label defined on line 40 can be referenced on line 5 without errors.

### 2.2 Core Data Structures

```c
// Symbol table: 1024-entry open-addressed hash table
struct _symbol_table {
    char name[6];        // 5-char symbol + NUL (name[0] == 0 means empty)
    uint16_t value;      // address or constant value
} symbol_table[1024];

// Opcode map (from optab.h): external, not defined in asm1802.c
struct opcode {
    char    *name;       // mnemonic (e.g., "LDA ")
    uint8_t base;         // base opcode byte
    uint8_t mask;         // reserved
    int     len;          // instruction length in bytes
};
extern const struct opcode opcode_map[];

```

Note: `location` is a `struct _symbol_table*` used as the pseudo-symbol for the current program counter, referenced in expressions by `*`.

### 2.3 Global Variables Summary

All state is held in global variables. This is a simple, compact design that avoids dynamic allocation. Key variables:

| Variable        | Role                                          |
|-----------------|-----------------------------------------------|
| `buffer`        | Current input for one source line             |
|  line_num       | Current source line number                    |
| `line_ptr`      | Walking pointer into `buffer` during parsing  |
| `symbol`        | 5-char buffer being built during NEXT state   |
| `label`         | Pointer to symbol table entry for a new label |
| `expr`          | Pointer to the symbol table entry for a ref.  |
| `location`      | Pseudo-symbol: current location counter       |
| `value`         | Current expression value being accumulated    |
| `acon` / `aoff` | Address constant tracking (A, A.0, A.1)       |
| `outputed`      | Whether current listing line has been printed |
| `bytes`         | Hex byte count on current listing line        |
| `memory[]`      | RAM image filled during pass 2                |
|  start_page     | Starting page to dump when binary file        | 
| `last_page`     | Tracks highest used address for binary dump   |
|  pass_num       | Current pass over source                      |

## 3. The Pass() Function: State Machine

The heart of the assembler is the `pass()` function, which is a multi-state loop driven by a C switch statement. The state machine processes one source program character by character, transitioning between states based on what is encountered.

### 3.1 Pass States

| State   | Value | Purpose                                           |
|---------|-------|---------------------------------------------------|
| BEGIN   | 10    | Read next line from input via `fgets()`           |
| START   | 11    | Identify line start type (label, opcode, , , ;)   |
| NEXT    | 12    | Collect character sequence into `symbol[]`        |
| OP      | 13    | Look up `symbol[]` in opcode map or special list  |
| EQU     | 14    | Process `SYMBOL=EXPR` (equate)                    |
| CONST   | 15    | Process `,CONST` data list entry                  |
| TERM    | 16    | First token of an expression                      |
| OFFSET  | 17    | Check for `+` or `-` after symbol/*               |
| EXPR    | 18    | Second token (the constant after +/-)             |
| TEXT    | 19    | Process delimited text string                     |
| REG     | 20    | Register operand: hex digit, Rn, or symbol        |
| FINISH  | 21    | Emits final opcode/operand bytes for this stmt    |
| END     | 22    | Assembly terminated by END directive              |
| DONE    | 23    | Scan for statement separator (;, ,, etc.)         |

### 3.2 Flow Diagram

```
                    ┌─────────┐
                    │  BEGIN  │  fgets() next line
                    └────┬────┘
                         ▼
                    ┌─────────┐
                    │  START  │  Detect: comment / blank / , / ; / alpha / .
                    └────┬────┘
                         │
┌────────┬───────────────┼─────────────┬───────────┐
│        │               │             │           │
│        ▼               ▼             ▼           ▼
│   ┌──────────┐   ┌──────────┐  ┌──────────┐ ┌──────────┐
│   │  ,  -->  │   │  ; -->   │  │ alpha    │ │ .. -->   │ 
│   │  CONST   │   │  START   │  │  ; -->   │ │  newline │
│   └──────────┘   └──────────┘  └──────────┘ └──────────┘
│                                     │
│                                     ▼ 
│                          ┌──────────────────────┐
│                          │  collect chars into  │
│                          │  symbol[]            │
│                          └──────────┬───────────┘
│                                     │
│                                     ▼
│                ┌────────────────────────────────────────────┐
│                │   terminated by                            │
│                │  ; . , : = SP CR                           │
│                └───┬──────────────────────────────┬─────────┘
│                    │          │                   │     
│                    ▼          │                   ▼       
│           ┌───────────────┐   │               ┌───────┐   
│           │  :  -->       │   │               │  = -->│   
│           │  Lookup label │   │               │  EQU  │   
│           │  in symbol    │   │               └───┬───┘   
│           │  table        │   │                   │       
│           └───────┬───────┘   │                   │       
├───────────────────┘           ▼                   │       
│                         ┌──────────┐              │       
│                         │ lookup   │              │       
│                         │ opcode   │              │      
│                         │ in map   │              │      
│                         └────┬─────┘              │       
│                              │                    │        
│        ┌──────────┬──────────┴──┬────────────┐    │       
│        │          │             │            │    │       
│        ▼          ▼             ▼            ▼    │       
│    ┌───────┐  ┌───────┐  ┌─────────────┐  ┌─────┐ │       
│    │ DC    │  │ OPN   │  │ OPN/OPB/OPI │  │ EQU │ │       
│    │ CONST │  │       │  │ OPL/OPO/ORG │  │     │ │        
│    └───────┘  └───┬───┘  └────┬────────┘  └──┬──┘ │                          
│                   │           │              │    │       
│                   ▼           ▼              ▼    ▼       
│               ┌───────┐    ┌─────────────────────────┐     
│               │ REG   │    │  TERM (look             │      
│               │       │    │  for oper.)             │      
│               └───┬───┘    └───────────┬─────────────┘      
│                   │                    │                  
│                   │                    ▼                         
│                   │              ┌───────────┐             
│                   │              │ hex / dec │             
│                   │              │ / sym / * │             
│                   │              └─────┬─────┘             
│                   │                    │                  
│                   │                    ▼                 
│                   │          ┌─────────────────────┐     
│                   │          │   OFFSET            │      
│                   │          │ check for +/- sign  │      
│                   │          └──┬──────────┬───────┘      
│                   │             ▼          │                    
│                   │       ┌──────────┐     │              
│                   │       │ EXPR:    │     │              
│                   │       │ collect  │     │              
│                   │       │ constant │     │              
│                   │       └────┬─────┘     │              
│                   │            │           │              
│                   ▼            ▼           ▼                
│               ┌────────────────────────────────────┐      
│               │ FINISH: emit                       │      
│               │ opcode bytes                       │      
│               └──────────────────┬─────────────────┘      
│                                  │                        
│                                  ▼                        
│                          ┌─────────────────────────┐
│                          │  DONE:  scan            │
│                          │ for ; , . or            │
│                          │ end of line             │
│                          └───────┬─────────────────┘
│                                  │
└──────────────────────────────────┘ 


### 3.3 Key State Transitions

**NEXT -> OP (opcode collection):**
Characters are accumulated into `symbol[]` until a terminator (`.`, `;`, `,`, `:`, `=`, `#`, `*`, space, tab, newline) is seen. If `=` is found, state transitions to EQU. If `:` is found, label is defined. Otherwise, state becomes OP.

**OP -> (multiple outcomes):**
The assembler searches `opcode_map[]` first, then the special directives list (`EQU`, `PAGE`, `ORG`, `END`, `DC`). If no opcode matches, error 1 is issued.

Based on the opcode type:
- `OPR` -> emit base opcode -> FINISH
- `OPN` -> state = REG (collect register operand)
- `OPB/OPO/OPI/OPL` -> state = TERM (collect expression operand)
- `PAG` -> FINISH (PAGE directive)
- `END` -> END state

**TERM -> OFFSET -> EXPR (expression collection):**
TERM scans the first token. If it's `*`, the location counter is used as the base expression value. If it's `#`, a hex constant is scanned. If it's a digit, a decimal constant. Otherwise `scan_symbol()` looks it up as a symbol or address constant.

If the result is a symbol reference or `*`, the state transitions to OFFSET to check for a `+`/`-` modifier. EXPR then collects the trailing constant and computes `expr_value ± constant`.

**FINISH -> (opcode-type-specific emission):**

| type  | Action                                                         |
|-------|-----------------------------------------------------------------|
| DC    | Emit low byte; conditionally emit high byte for A(...) constants |
| ORG   | Set `location.value = value`, optionally define label           |
| EQ    | Assign `label->value = value`, define label in pass 1           |
| OPO   | Validate value < 8, emit `base \| value`                        |
| OPB   | Emit base + low byte; check page boundary (error 14)            |
| OPI   | Emit base + (lo or hi byte depending on aoff)                   |
| OPN   | Emit `base + value` (register number)                           |
| OPL   | Emit base + high byte + low byte                                |
| PAG   | Advance location counter to next 256-byte page                    |

## 4. Symbol Table Implementation

### 4.1 Hash Function

```c
hash = 0;
for (i = 0; i < 6; i++)
    hash = (hash << 2) + symbol[i];
hash = hash % 1024;
```

The hash is computed from all characters in the `symbol[]` buffer. Since symbols are padded with spaces, `symbol[] == "START "` and `symbol[] == " START"` both hash, but the equality check (`symbol[i] != ... & 0x7f`) ensures exact match comparison.

### 4.2 Collision Resolution

Linear probing is used. If the slot is occupied and the name doesn't match, try the next slot. The table is scanned twice (using a `loop` flag) to detect overflow.

The high bit of `name[0]` (set to `0x80` when a symbol is defined) serves as a "defined" flag, allowing the assembler to distinguish between empty slots (`name[0] == 0`) and occupied ones, while also tracking whether a symbol has been assigned a value.

### 4.3 Capacity

- 1024 entry table
- Each symbol: up to 5 printable characters (spaces are padding)
- On overflow: error code OVFL printed, lookup returns NULL
- Forward references: lookup returns pointer to empty entry slot when `define=0`, which triggers deferred resolution in later passes

## 5. Constant Parsing

Three dedicated functions handle the different constant formats. All take a `ch` parameter: if `ch == '\''`, the first character is consumed as the opening quote delimiter.

### 5.1 Hexadecimal (`hex`)

```
#hhhh    or    X'hhhh'
```

Accumulates digit pairs. When `emit_byte` is true and two digits are collected, a byte is emitted immediately and the accumulator resets. This handles multi-byte hex constants like `#ABCDEF` (emits AB, then CD, then EF).

### 5.2 Decimal (`decimal`)

```
ddddd    or    D'dddd'
```

Accumulates value as `value = value * 10 + digit`. On completion, if `emit_byte` is true, the value is emitted as one or two bytes (big-endian). Values > 65535 overflow naturally in the uint16_t accumulator.

### 5.3 Binary (`binary`)

```
B'bbbbb'
```

Accumulates bits up to 8. Emits a single byte.

### 5.4 Text (`TEXT` state)

```
T'hello world'
```

Scans characters until closing `'`, handling escape sequences (`''` for single quote). If processing a DC (data list), each character is emitted as a byte. Otherwise, the last character's ASCII value is placed in `value`.

### 5.5 Address Constants (`A(...)`, `A.0(...)`, `A.1(...)`)

Recognized in `scan_symbol()` when the leading character is `A` followed by non-alphanumeric.

| Form       | aoff | Emitted    |
|------------|------|------------|
| `A(...)`   | 3    | high + low |
| `A.0(...)` | 1    | low byte   |
| `A.1(...)` | 2    | high byte  |

Inside the parentheses, `*` references the current location counter. The result feeds back into OFFSET for `+/-` modifier processing.

## 6. Binary Output

### 6.1 Memory Image

During pass 2, every emitted byte is stored in the 64KB `memory[]` array at the index specified by `location.value`. The `last_page` variable tracks the highest address used.

### 6.2 Command-Line Page Parsing

The `-b` flag's argument is parsed as hexadecimal digits from the same argv token:

```
-bF1
```

Each hex digit shifts `start_page` left by 4 bits. After parsing completes, `start_page` is masked to 8 bits (so max page = FF` from the hex string `F1` becomes `F100` after shifting). This allows binary output to start at arbitrary page boundaries.

### 6.3 Dump Logic

After pass 2 completes:

```c
last_page = (last_page | 0x00ff) + 1;  // round up to next page
if (bin_out != NULL && start_page < last_page) {
    fwrite(&memory[start_page], last_page - start_page, 1, bin_out);
}
```

The binary dump includes all pages from `start_page` through the final assembled page, inclusive. Unassembled pages within this range are zero-filled.

### 6.4 Listing Format

The listing output follows this layout per generated line:

```
<LOC> <HEX BYTES>;    <LINE_NUM> <SOURCE>
```

Where:
- `<LOC>` is the decimal value of the location counter before this statement
- `<HEX BYTES>` are the generated hex bytes, padded with spaces
- `<LINE_NUM>` is the source line number
- `<SOURCE>` is the original source line
- Label-definition lines show the assigned address

## 7. Error Handling

### 7.1 Error Display Format

Errors display three elements:
1. The source line with a `?` character at the error position
2. The error code number on the following line
3. In pass 1: unconditionally; in pass 2: only via `dumpline()` which re-emits the line in listing format

### 7.2 Error Codes

| Code | Meaning                  | Detected In                    |
|------|--------------------------|--------------------------------|
| 1    | Unrecognized opcode/comma | NEXT (invalid char), OP (not found) |
| 2    | Symbol redefined        | OP (label already defined), EQU |
| 4    | Missing quote terminator | hex(), decimal(), binary()     |
| 5    | Binary > 8 bits         | binary()                       |
| 6    | Expected hex/decimal    | scan_symbol() (A() empty)      |
| 7    | Undefined symbol        | scan_symbol(), FINISH (EQ)     |
| 8    | Expected expression     | CONST, TERM, REG, OFFSET       |
| 9    | Invalid hex character   | (implied by isxdigit check)    |
| 10   | Missing trailing quote  | (detected at end of line)      |
| 11   | Period error            | NEXT, DONE (single `.` not `..`) |
| 12   | Leading character error | (invalid statement start)       |
| 14   | Branch out of page      | OPB (page boundary check)      |
| 15   | Invalid register number | (implied, R0 for OPI)           |
| 16   | Device number out range | OPO (value > 7)                 |
| OVFL | Symbol table overflow   | lookup_symbol()                 |

### 7.3 Error Recovery

When an error is detected, the assembler does not abort. It:
- Reports the error code to the listing
- Skips to an appropriate recovery state (usually BEGIN or FINISH)
- Continues accumulating the location counter forward
- Produces error-free output up to the point of the error

This means subsequent errors may cascade from the first (e.g., undefined symbol errors for every usage of a label that was never properly defined).

## 8. Opcode Map Integration

The `opcode_map[]` array (declared in `optab.h`) is external to `asm1802.c`. It is iterated linearly during the OP state:

```c
for (op = &opcode_map[0]; op->name != NULL && !match; op++) {
    match = (op->name[i] == symbol[i]) for all chars;
    // reject partial matches where symbol still has content
}
op--;  // step back to the last match
```

Partial matches are rejected: if `symbol[]` has non-space content beyond what the opcode name consumed, `match` is cleared. This prevents "BR" from matching "BRC" if the source says "BR Z".

For special directives, a parallel `special[]` / `svalue[]` array (left-padded with spaces to length 6) is compared against `symbol[]`.

## 9. Listing Output Mechanics

### 9.1 Output Buffering

During pass 2, emitted hex bytes are buffered per logical line:
- Maximum 6 bytes per listing line
- When `bytes == 7` (7th byte), `dumpline()` emits the previous line with `;` prefix, followed by a new line starting with the current location counter
- The first listing line for a source line uses indentation (`outputed == 0`)
- Subsequent lines just get a newline

### 9.2 Location Counter in Output

At the start of each listing line, the location counter value before processing the statement is printed as a 4-digit hex string. This allows the user to verify that labels and code land at the expected addresses.

## 10. Design Constraints

### 10.1 Memory Layout

The assembler uses a fixed 64KB memory image (`memory[]`) which matches the 1802's address space. The symbol table is fixed at 1024 entries. Both are statically allocated — no dynamic memory allocation is used.

### 10.2 Why Globals

All state is in global variables. This is a deliberate design choice that keeps the implementation simple and compact:
- Eliminates the need for a context/accumulator struct passed between functions
- `line_ptr` provides a natural cursor within the input buffer
- `value`, `label`, `expr`, and `location` are accessed freely across `scan_symbol()`, `TERM`, `OFFSET`, `EXPR`, and `FINISH`

### 10.3 Forward References

Symbols can be referenced before they are defined. In pass 1, the symbol table returns NULL for undefined symbols, and the assembler defers the value lookup to pass 2 where the symbol will have been defined. The `pass_num` flag gates this behavior: in pass 2, undefined symbol references produce error 7.

