/*
 * Cosmac VIP - Main start-up file.
 *
 * Author:      Richard Cornwell (rich@sky-visions.com)
 * Copyright 2023, Richard Cornwell
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
#include "system.h"
#include "disassemble.h"
#include "cpu.h"
#include "roms.h"
#include <math.h>
#include <errno.h>
#include <termio.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <SDL.h>
#include <SDL_thread.h>
#ifndef _WIN32
#include <SDL_main.h>
#endif
#include <SDL_mixer.h>
#include <SDL_image.h>


/**
 * @brief Main interface between emulator and SDL.
 *
 * This file holds the interface between the emulator and SDL library.
 * This is not a class since these functions interface with C libraries.
 */

SDL_Window       *window;
SDL_Surface      *icon;
SDL_Renderer     *render;
SDL_Texture      *texture;
SDL_AudioDeviceID audio_device;
SDL_AudioSpec     request;
SDL_AudioSpec     obtained;
SDL_PixelFormat  *format;
SDL_Thread       *serial_thread;
uint8_t           key[16];
uint8_t           key2[16];

int               run_flag;
int               read_bin(char *name);
int               read_dump(char *name);

#define FREQUENCY 44100


uint32_t          screen[128*128];
int               sample_pos;         /**< Position to write audio samples */

#define CYCLES_PER_SCREEN (262*14)
#define FRAME_TIME        16.650f

int               scale;              /**< Screen scaling */
int               POWER;              /**< Game boy power state */
int               trace_flag;         /**< Trace instruction execution */
int               serial;             /**< Serial port */
FILE              *tape_file;         /**< Tape data file. */

struct termios    save_termios;
int               term_saved;
int               term;
uint16_t          bin_base = 0;

char *names[] = { "vip", "vp", "studio2", "studio3", NULL};


/**
 * @brief  main, entry to system.
 *
 * Scan arguments looking for scale and cartridge file names to load.
 */
int main(int argc, char **argv)
{
     int           i;            /* Temp */
     int           help = 0;

     /* Print banner out */
     printf("Cosmac Vip Emulator (%d,%d)\n", VERSION_MAJOR, VERSION_MINOR);
     scale = 4;
     POWER = 1;
     trace_flag = 0;
     memsize = 4096;
     memmask = 0xfff;
     system_type = VIP;

     /* Scan for arguments */
     for (i = 1; i < argc; i++) {
         if (argv[i][0] == '-') {
             char     *p = argv[i];
             char     *q;

             /* options:
              *   <file>     load dump file.
              *   -e type    Type of system to emulate.
              *   -s         enable serial in/out.
              *   -i         enable instruction trace.
              *   -h         help.
              *   -c         load chip8 into lower ram.
              *   -r         Rom cartridge.
              *   -b <file>  load binary file into ram.
              *   -d <file>  Load dump file into ram.
              *   -t <file>  attach tape file.
              *   -m <size>  size of memory in 1K blocks.
              */

             while (*++p != '\0') {
                 switch(*p) {
                 case 's':
                           serial = 1;
                           break;

                 case 'i':
                           trace_flag = 1;
                           break;

                 case 'h':
                           help = 1;
                           break;

                 case 'c':
                           load_chip8();
                           break;

                 case 'e':
                           for(system_type = 0; names[system_type] != NULL; system_type++) {
                                if (strcasecmp(argv[i+1], names[system_type]) == 0) {
                                    break;
                               }
                           }
                           if (names[system_type] == NULL) {
                               system_type = VIP;
                           }
                           switch(system_type) {
                           case VIP:
                                    break;
                           case VP:
                                    color = 1;
                                    break;
                           case RCA_STUDIO3:
                                    color = 1;
                           case RCA_STUDIO2:
                                    memmask = 0xfff;
                                    memsize = 4096;
                                    bin_base = 1024;
                           break;
                           }
                           i++;
                           break;

                 case 'm':
                           q = argv[++i];
                           memsize = 0;
                           while (*q != '\0') {
                               if (*q < '0' || *q > '9') {
                                   break;
                               }
                               memsize = (memsize * 10) + *q++ - '0';
                           }
                           /* Default size to 4K */
                           if (memsize == 0) {
                               memsize = 4;
                           }
                           /* Max size is 32K */
                           if (memsize > 32) {
                               memsize = 32;
                           }
                           /* Must be multiple of 2K */
                           if ((memsize & 1) != 0) {
                               memsize++;
                           }
                           memsize *= 1024;
                           memmask = (memsize-1);
                           break;

                 case 'r':
                           cartridge = 1;
                 case 'b':
                           if (!read_bin(argv[++i])) {
                               exit(3);
                           }
                           break;

                 case 'd':
                           if (!read_dump(argv[++i])) {
                               exit(3);
                           }
                           break;

                 case 't':
                           tape_file = fopen(argv[++i], "rw");
                           if (tape_file == NULL) {
                              fprintf(stderr, "Unable to open tape file %s: %s\n", argv[i], strerror(errno));
                              exit(1);
                           }
                           break;

                 case '1':
                 case '2':
                 case '3':
                 case '4':
                 case '5':
                 case '6':
                 case '7':
                 case '8':
                 case '9':
                           scale = *p - '0';
                           break;
                 default:
                           help = 1;
                           break;
                 }
             }
         } else {
             /* Read dump file */
             if (!read_dump(argv[i])) {
                 exit(2);
             }
         }
     }

     if (help) {
        printf("%s: options: \n"
         "       <file>     load dump file.\n"
         "       -b <file>  load binary file into ram.\n"
         "       -c         load chip8 into lower ram.\n"
         "       -d <file>  Load dump file into ram.\n"
         "       -e type    Type of system to emulate.\n"
         "       -h         help.\n"
         "       -i         enable instruction trace.\n"
         "       -m <size>  size of memory in 1K blocks.\n"
         "       -r         Rom cartridge.\n"
         "       -s         enable serial in/out.\n"
         "       -t <file>  attach tape file.\n", argv[0]);
        exit(1);
     }

     /* Set up SDL windows */
     init_window();

     reset();

     /* Run the simulation */
     run_sim();

     if (tape_file != NULL) {
         fclose(tape_file);
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
     char    *p;
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

     p = strrchr(name, '.');
     if (cartridge && p != NULL && strcmp(p, ".st2") == 0) {
         printf("Studio 2 cartridge\n");
         bin_base -= 256;
     }

     got = fread(&memory[bin_base], 1, len, in);


     if (got != len) {
         fprintf(stderr, "Unable to read file %s: %s\n", name, strerror(errno));
         fclose(in);
         return 0;
     }

     fclose(in);
     fprintf(stderr, "Read %ld bytes from %s to %04x \n", got, name, bin_base);
     bin_base += got;
     return 1;
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


     if (in == NULL) {
         fprintf(stderr, "Unable to read file %s: %s\n", name, strerror(errno));
         return 0;
     }

     while(fgets(buffer, sizeof(buffer), in) != NULL) {
        int       count = 0;
        char      *digit;
        uint16_t  address = 0;
        char      *ptr;
        uint16_t  value = 0;

        for (ptr = buffer; *ptr != '\0'; ptr++) {
            // If space skip to next char.
            if (isspace(*ptr)) {
                continue;
            }

            // Terminate scan on ;.
            if (*ptr == ';') {
                break;
            }

            digit = strchr(hex, toupper(*ptr));
            if (digit == NULL) {
                fprintf(stderr, "Invalid character %s: %s", name, buffer);
                fclose(in);
                return 0;
            }
            value = (value << 4) | (digit - hex);
            count++;
            if (count == 4) {
                address = value;
                value = 0;
                continue;
            }
            if (address > (32 * 1024)) {
                break;
            }
            if ((count > 4) && (count & 1) == 0) {
                memory[address++] = value;
                value = 0;
            }
        }
     }
     fclose(in);
     return 1;
}

/**
 * @brief Load CHIP 8 interpreter into low ram locations.
 *
 */
void
load_chip8()
{
    int i;

    if (color) {
        for (i = 0; i < (int)sizeof(chip8x_data); i++) {
           memory[i] = chip8x_data[i];
        }
    } else {
        for (i = 0; i < (int)sizeof(chip8_data); i++) {
           memory[i] = chip8_data[i];
        }
    }
    bin_base = i;
}


/**
 * @brief process terminal input.
 *
 */
int
serial_thrd(void *data)
{
    while(POWER) {
        char c;
        if (read(term, &c, 1) == 1) {
           serial_in = ((c ^ 0x7f) << 2) | 2;
        }
    }
    return 0;
}

/**
 * @brief write one character to output.
 */
void
write_console(uint16_t data)
{
     char c = (char)(data & 0x7f);
     write(term, &c, 1);
}
 
/**
 * @brief restore terminal to original state.
 */
void
console_done()
{
    if (term_saved) {
        (void)tcsetattr(0, TCSAFLUSH, &save_termios);
    }
    close(term);
}

/**
 * @brief Initialize console for serial I/O.
 *
 */
void
init_console()
{
    struct termios buf;

    term = open("/dev/tty", O_ASYNC|O_NONBLOCK|O_RDWR);
    if (term < 0) {
        return;
    }

    if (tcgetattr(0, &save_termios) < 0) { /* save original state. */
       return;
    }

    buf = save_termios;
    buf = save_termios;

    buf.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
                    /* echo off, canonical mode off, extended input
                       processing off, signal chars off */

    buf.c_iflag &= ~(BRKINT | ICRNL | ISTRIP | IXON);
                    /* no SIGINT on BREAK, CR-toNL off, input parity
                       check off, don't strip the 8th bit on input,
                       ouput flow control off */

    buf.c_cflag &= ~(CSIZE | PARENB);
                    /* clear size bits, parity checking off */

    buf.c_cflag |= CS8;
                    /* set 8 bits/char */

    buf.c_oflag &= ~(OPOST);
                    /* output processing off */

    buf.c_cc[VMIN] = 1;  /* 1 byte at a time */
    buf.c_cc[VTIME] = 1; /* timeout on input */

    if (tcsetattr(0, TCSAFLUSH, &buf) >= 0) {
        term_saved = 1;
    }
    serial_thread = SDL_CreateThread(&serial_thrd, "serial", 0);
    atexit(&console_done);
}


void
tape_write_byte(uint8_t data)
{
     fputc(hex[(data >> 4) & 0xf], tape_file);
     fputc(hex[data & 0xf], tape_file);
}

uint8_t
tape_read_byte()
{
    int       c;
    char     *digit;
    int       value = 0;
    if ((c = fgetc(tape_file)) == EOF) {
         return 0;
    }

    if (c == ':') {
        return 0;
    }
    digit = strchr(hex, toupper(c));
    if (digit == NULL) {
        fprintf(stderr, "Invalid character tape: %c", c);
        return 0;
    }
    value = (digit - hex) << 4;
    if ((c = fgetc(tape_file)) == EOF) {
         return 0;
    }

    if (c == ':') {
        return 0;
    }
    digit = strchr(hex, toupper(c));
    if (digit == NULL) {
        fprintf(stderr, "Invalid character tape: %c", c);
        return 0;
    }
    value += digit - hex;
    return (uint8_t)(value & 0xff);
}

/**
 * @brief Generate audio square wave.
 *
 */
void
audio_callback(void *user_data, Uint8 *raw_buffer, int bytes)
{
     Sint16 *buffer = (Sint16*)raw_buffer;
     int length = bytes / 2; // Two bytes per sample.
     int sample_nr = *(int *)user_data;

     int samples_per_period = FREQUENCY/440;

     for (int i = 0; i < length; i++) {
             Sint16 sample_value = 3000;
             if ((sample_nr / samples_per_period) % 2) {
                 sample_value = -3000;
             }
             *buffer++ = sample_value;
             sample_nr++;
     }
}

/**
 * @brief Initialize SDL window and sound system.
 *
 * Create an SDL window and audio device.
 */
void init_window()
{

    // Start SDL
    SDL_Init( SDL_INIT_EVERYTHING );

    window = SDL_CreateWindow("Cosmac VIP", SDL_WINDOWPOS_UNDEFINED,
                                     SDL_WINDOWPOS_UNDEFINED,
                           128*scale, 128*scale, SDL_WINDOW_RESIZABLE );
    /* Create icon for display */
#if 0
    icon = IMG_ReadXPMFromArray((char **)icon_image);
    SDL_SetWindowIcon(window, icon);
#endif

    // Request audio playback
    request.freq = FREQUENCY;
    request.format = AUDIO_S16SYS;
    request.channels = 1;
    request.samples = 2048;
    request.callback = &audio_callback;
    request.userdata = (void *)&sample_pos;
    audio_device = SDL_OpenAudioDevice(NULL, 0, &request, &obtained, 0);
    if (audio_device == 0) {
       fprintf(stderr, "Failed to get audio device\n");
       exit(1);
    }

    if (serial) {
       init_console();
    }
}

/**
 * @brief Called before start of screen display.
 *
 * Clear display for new draw.
 */
void init_screen()
{
}


/**
 * @brief Called after screen drawing complete.
 *
 * Present screen to SDL for display.
 */
void
draw_screen()
{
    SDL_UpdateTexture( texture, 0, screen, 128 * sizeof(uint32_t));
    SDL_RenderCopy( render, texture, 0, 0);
    SDL_RenderPresent( render );
}

     /* R    G    B      A */
SDL_Color palette[8] = {
     { 0x00, 0x00, 0x00, 0xff },  /* 0 Black, back 2 */
     { 0xff, 0x00, 0x00, 0xff },  /* 1 Red, back 4 */
     { 0x00, 0x00, 0xff, 0xff },  /* 2 Blue, back 1 */
     { 0xff, 0x00, 0xff, 0xff },  /* 3 Violet */
     { 0x00, 0xff, 0x00, 0xff },  /* 4 Green, back 3 */
     { 0xff, 0xff, 0x00, 0xff },  /* 5 Yellow */
     { 0x00, 0xff, 0xff, 0xff },  /* 6 Aqua */
     { 0xff, 0xff, 0xff, 0xff },  /* 7 White */
};

/* Background */
/* 2 -> 0 -> 4 -> 1 -> 2 */

/**
 * @brief Draw a pixel.
 *
 * Draw a rectangle of scale size and given color.
 *
 * @param pix  Index into Palette table to use.
 * @param row  Row of pixel
 * @param col  Column of pixel
 */
void
draw_pixel(uint8_t pix, int row, int col)
{
     screen[(row * 128) + (col * 2) + 0] = SDL_MapRGBA(format,
             palette[pix].r, palette[pix].g, palette[pix].b, 0xff);
     screen[(row * 128) + (col * 2) + 1] = SDL_MapRGBA(format,
             palette[pix].r, palette[pix].g, palette[pix].b, 0xff);
//     disp[row][col] = pix;
}


/**
 * @brief Main simulation loop.
 *
 * Initialize the audio device, create render space, then loop until
 * POWER becomes false; process any events, then run CPU simulation
 * for one display frame, queue up any audio data produced. Then wait
 * for about 16.8MS. Next determine exactly how much time has passed
 * and adjust next cycle length to keep at fixed rate.
 */
void
run_sim()
{
    SDL_Event event;
    float  time_left = 0.0f;

    // Initialize SDL for display
    POWER = 1;
    run_flag = 0;
    if (serial) {
        run_flag = 1;
        reset();
        run();
    }
    render = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(render, SDL_PIXELFORMAT_RGBA32,
                    SDL_TEXTUREACCESS_STREAMING, 128, 128);
    format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA32);
    SDL_SetRenderDrawColor( render, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear( render);
    SDL_RenderPresent( render );
    for (int i = 0; i < 16; key[i++] = 0);
    while(POWER) {
       // Process events
       uint64_t     start_time = SDL_GetPerformanceCounter();
       while(SDL_PollEvent(&event)) {
          switch(event.type) {
          case SDL_MOUSEBUTTONDOWN:
               break;
          case SDL_MOUSEBUTTONUP:
               break;
          case SDL_KEYUP:
                 switch(event.key.keysym.scancode) {
                 case SDL_SCANCODE_X:
                         key2[0] = 0;
                         break;

                 case SDL_SCANCODE_A:
                         key2[1] = 0;
                         break;

                 case SDL_SCANCODE_S:
                         key2[2] = 0;
                         break;

                 case SDL_SCANCODE_D:
                         key2[3] = 0;
                         break;

                 case SDL_SCANCODE_Q:
                         key2[4] = 0;
                         break;

                 case SDL_SCANCODE_W:
                         key2[5] = 0;
                         break;

                 case SDL_SCANCODE_E:
                         key2[6] = 0;
                         break;

                 case SDL_SCANCODE_1:
                         key2[7] = 0;
                         break;

                 case SDL_SCANCODE_2:
                         key2[8] = 0;
                         break;

                 case SDL_SCANCODE_3:
                         key2[9] = 0;
                         break;

                 case SDL_SCANCODE_KP_0:
                         key[0] = 0;
                         break;

                 case SDL_SCANCODE_KP_1:
                         key[1] = 0;
                         break;

                 case SDL_SCANCODE_KP_2:
                         key[2] = 0;
                         break;

                 case SDL_SCANCODE_KP_3:
                         key[3] = 0;
                         break;

                 case SDL_SCANCODE_KP_4:
                         key[4] = 0;
                         break;

                 case SDL_SCANCODE_KP_5:
                         key[5] = 0;
                         break;

                 case SDL_SCANCODE_KP_6:
                         key[6] = 0;
                         break;

                 case SDL_SCANCODE_KP_7:
                         key[7] = 0;
                         break;

                 case SDL_SCANCODE_KP_8:
                         key[8] = 0;
                         break;

                 case SDL_SCANCODE_KP_9:
                         key[9] = 0;
                         break;

                 case SDL_SCANCODE_KP_PERIOD:
                         key[0xa] = 0;
                         break;

                 case SDL_SCANCODE_KP_ENTER:
                         key[0xb] = 0;
                         break;

                 case SDL_SCANCODE_KP_PLUS:
                         key[0xc] = 0;
                         break;

                 case SDL_SCANCODE_KP_MINUS:
                         key[0xb] = 0;
                         break;

                 case SDL_SCANCODE_KP_MULTIPLY:
                         key[0xe] = 0;
                         break;

                 case SDL_SCANCODE_KP_DIVIDE:
                         key[0xf] = 0;
                         break;

                 case SDL_SCANCODE_F1:  // Toggle run flag.
                         run_flag = !run_flag;
                         reset();
                         if (run_flag) {
                             run();
                         } else {
                             stop();
                         }
                         break;

                 case SDL_SCANCODE_F2:  // Start tape read.
                         taperead();
                         break;

                 case SDL_SCANCODE_F3:  // Start tape write.
                         tapewrite();
                         break;

                 case SDL_SCANCODE_F4:  // Perform one instruction step.
                         run();
                         step();
                         trace();
                         stop();
                         break;

                 case SDL_SCANCODE_F5:  // Restart running.
                         run();
                         break;

                 case SDL_SCANCODE_F6:
                         POWER = 0;
                         break;
                 default:
                         break;
                 }
                 break;

          case SDL_KEYDOWN:
                 switch(event.key.keysym.scancode) {
                 case SDL_SCANCODE_0:
                         key[0] = 1;
                         break;

                 case SDL_SCANCODE_1:
                         key[1] = 1;
                         break;

                 case SDL_SCANCODE_2:
                         key[2] = 1;
                         break;

                 case SDL_SCANCODE_3:
                         key[3] = 1;
                         break;

                 case SDL_SCANCODE_4:
                         key[4] = 1;
                         break;

                 case SDL_SCANCODE_5:
                         key[5] = 1;
                         break;

                 case SDL_SCANCODE_6:
                         key[6] = 1;
                         break;

                 case SDL_SCANCODE_7:
                         key[7] = 1;
                         break;

                 case SDL_SCANCODE_8:
                         key[8] = 1;
                         break;

                 case SDL_SCANCODE_9:
                         key[9] = 1;
                         break;

                 case SDL_SCANCODE_A:
                         key[0xa] = 1;
                         break;

                 case SDL_SCANCODE_B:
                         key[0xb] = 1;
                         break;

                 case SDL_SCANCODE_C:
                         key[0xc] = 1;
                         break;

                 case SDL_SCANCODE_D:
                         key[0xd] = 1;
                         break;

                 case SDL_SCANCODE_E:
                         key[0xe] = 1;
                         break;

                 case SDL_SCANCODE_F:
                         key[0xf] = 1;
                         break;

                 case SDL_SCANCODE_N:
                         key2[0] = 1;
                         break;

                 case SDL_SCANCODE_H:
                         key2[1] = 1;
                         break;

                 case SDL_SCANCODE_J:
                         key2[2] = 1;
                         break;

                 case SDL_SCANCODE_K:
                         key2[3] = 1;
                         break;

                 case SDL_SCANCODE_L:
                         key2[4] = 1;
                         break;

                 case SDL_SCANCODE_Y:
                         key2[5] = 1;
                         break;

                 case SDL_SCANCODE_U:
                         key2[6] = 1;
                         break;

                 case SDL_SCANCODE_I:
                         key2[7] = 1;
                         break;

                 case SDL_SCANCODE_O:
                         key2[8] = 1;
                         break;

                 case SDL_SCANCODE_P:
                         key2[9] = 1;
                         break;
                 case SDL_SCANCODE_KP_0:
                         key[0] = 1;
                         break;

                 case SDL_SCANCODE_KP_1:
                         key[1] = 1;
                         break;

                 case SDL_SCANCODE_KP_2:
                         key[2] = 1;
                         break;

                 case SDL_SCANCODE_KP_3:
                         key[3] = 1;
                         break;

                 case SDL_SCANCODE_KP_4:
                         key[4] = 1;
                         break;

                 case SDL_SCANCODE_KP_5:
                         key[5] = 1;
                         break;

                 case SDL_SCANCODE_KP_6:
                         key[6] = 1;
                         break;

                 case SDL_SCANCODE_KP_7:
                         key[7] = 1;
                         break;

                 case SDL_SCANCODE_KP_8:
                         key[8] = 1;
                         break;

                 case SDL_SCANCODE_KP_9:
                         key[9] = 1;
                         break;

                 case SDL_SCANCODE_KP_PERIOD:
                         key[0xa] = 1;
                         break;

                 case SDL_SCANCODE_KP_ENTER:
                         key[0xb] = 1;
                         break;

                 case SDL_SCANCODE_KP_PLUS:
                         key[0xc] = 1;
                         break;

                 case SDL_SCANCODE_KP_MINUS:
                         key[0xb] = 1;
                         break;

                 case SDL_SCANCODE_KP_MULTIPLY:
                         key[0xe] = 1;
                         break;

                 case SDL_SCANCODE_KP_DIVIDE:
                         key[0xf] = 1;
                         break;

                 case SDL_SCANCODE_Q:
                         POWER = 0;
                         break;
                 default:
                         break;
                 }
                 break;
          case SDL_WINDOWEVENT:
                 switch (event.window.event) {
                 case SDL_WINDOWEVENT_CLOSE:
                      break;
                 }
                 break;
          case SDL_QUIT:
                 POWER = 0;
                 break;
          default:
                 break;
          }
       }

       // Run for 16.742ms, or one frame.
       while(get_cycles() < CYCLES_PER_SCREEN) {
          if (trace_flag && running && !idle) {
              if ((dma_out | dma_in) == 0) {
                  trace();
              }
          }
          step();
       }

       // If Q set turn on sound.
       if (!serial) {
           SDL_PauseAudioDevice(audio_device, !Q);
       }

       // Tell CPU how many major cycles it should have run
       reset_cycles(CYCLES_PER_SCREEN);

       // Compute how long to wait for before next screen
       uint64_t end_time = SDL_GetPerformanceCounter();
       float elapsedMS = (end_time - start_time) /
                (float)SDL_GetPerformanceFrequency() * 1000.0f;

       // Add in previous frame remainder
       elapsedMS += time_left;
       if (elapsedMS < FRAME_TIME) {
           SDL_Delay((uint32_t)floor(FRAME_TIME - elapsedMS));
       }

       // Compute amount of time delay actually waited for
       uint64_t frame_time = SDL_GetPerformanceCounter();
       float frameMS = (frame_time - start_time) /
                (float)SDL_GetPerformanceFrequency() * 1000.0f;

       // Adjust next frame to be correct
       time_left = frameMS - FRAME_TIME;
    }

    // Clean up house
    SDL_FreeFormat(format);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(render);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return;
}

