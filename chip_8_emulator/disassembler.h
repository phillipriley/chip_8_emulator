#pragma once

// TODO: Resolve warnings rather than ignoring them.
#define _CRT_SECURE_NO_WARNINGS

// Includes for standard libraries.
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <string.h>

// TODO: Move to common header rather than copying and pasting.
// Macros used to parse instructions for specific nibbles, bytes, bits, etc.
#define INSTR_FIRST_NIBBLE(instr) ((instr >> 12) & 0xF)
#define INSTR_SECOND_NIBBLE(instr) ((instr >> 8) & 0xF)
#define INSTR_THIRD_NIBBLE(instr) ((instr >> 4) & 0xF)
#define INSTR_FOURTH_NIBBLE(instr) (instr & 0xF)

#define INSTR_SECOND_BYTE(instr) (instr & 0xFF)
#define INSTR_MEM_ADDR(instr) (instr & 0xFFF)

#define BIT(byte,num) ((byte >> (7-num) & 0x1))

// Create a disassembled version of the currently loaded ROM.
bool disassemble(char * filename);