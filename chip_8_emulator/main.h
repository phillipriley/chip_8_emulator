#pragma once

// TODO: Resolve warnings rather than ignoring them.
#define _CRT_SECURE_NO_WARNINGS		

// Includes for standard libraries.
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

// Includes for Windows GUI functionality.
#include <windows.h>

// Includes for the standalone debug console.
#include <io.h>
#include <fcntl.h>

// Constants that define the size of the Chip-8 display.
#define DISPLAY_HEIGHT 32
#define DISPLAY_WIDTH 64
#define PIXEL_SIZE 10

// Constants that define font/sprite memory addresses.
#define FONT_ADDR_START 0x50
#define FONT_ADDR_END 0x9F

// Constants that define configureable functionality for ambiguous functionality.
#define SHIFT_COPY 0	// TODO: Implement functionality to copy VY --> VX for 8XY6/8XYE.

// TODO: Define constants that can be turned on/off for debugging.

// Macros used to parse instructions for specific nibbles, bytes, bits, etc.
#define INSTR_FIRST_NIBBLE(instr) ((instr >> 12) & 0xF)
#define INSTR_SECOND_NIBBLE(instr) ((instr >> 8) & 0xF)
#define INSTR_THIRD_NIBBLE(instr) ((instr >> 4) & 0xF)
#define INSTR_FOURTH_NIBBLE(instr) (instr & 0xF)

#define INSTR_SECOND_BYTE(instr) (instr & 0xFF)
#define INSTR_MEM_ADDR(instr) (instr & 0xFFF)

#define BIT(byte,num) ((byte >> (7-num) & 0x1))

// Macro used to provide high-visibility debug output that an instruction has not been implemented.
#define NOT_IMPLEMENTED printf("!!!!!!!!!!!!!!!! Not implemented. !!!!!!!!!!!!!!!!\n")

// Macro used to determine if a specific key is currently being pressed.
#define KEY_PRESSED(key) (GetAsyncKeyState(key) & (1 << 16))

// Handle used to create/reference the main window.
HANDLE _hwnd;

// Handle used to control execution speed.
HANDLE hRunMutex;

// ---- Chip-8-specific memory and registers. ----//
// Memory (4 kilobytes)
uint8_t memory[4096] = { 0 };

// Program Counter
uint16_t program_counter = 0x200;

// Index Register
uint16_t index_register = 0;

// Stack (addresses of 12 subroutines max)
uint16_t stack[12];
uint8_t stack_index = 0;

// Timers used for sound and delays (60Hz).
uint8_t delay_timer;
uint8_t sound_timer;

// General Purpose Registers (V0 - VF)
uint8_t v_reg[0x10];

// On/off state of display pixels.
bool display[DISPLAY_HEIGHT][DISPLAY_WIDTH];

// TODO: Flag to indicate that the display should be completely redrawn.
// bool redrawDisplay = true;

// Processes messages sent to GUI window.
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Performs fetch-decode-execute loop.
int execute();

// Draws the saved display state to the window.
void draw_display(HWND hwnd);

// Clears the saved display state and the Chip-8 display.
void clear_display();

// Loads the contents of a ROM file into memory.
void load_rom_from_file();

// Loads sprites into the required location in memory.
void load_font_sprites();

// Decrements the delay and sound timers at 60Hz.
void decrement_timers();