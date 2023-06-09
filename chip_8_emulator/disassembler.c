#include "disassembler.h"

bool disassemble(char* filename)
{
	// TODO: Update to disassemble recursively (versus current linear implementation).
	//		 Current implementation will treat data (such as sprites) as bad op_codes.
	//       https://reverseengineering.stackexchange.com/questions/2347/what-is-the-algorithm-used-in-recursive-traversal-disassembly

	// Current assumptions for recursive disassembly:
	//		- Contiguous code block at beginning of the file.
	//		- End of code is represented by a jump to the current program_counter.
	//		- All non-code lines are considered sprite data and displayed as such.

	// TODO: Update to read directly from file (decouple from loading ROM into memory).
	uint8_t memory[4096] = { 0 };

	// TODO: Buffer to hold output file contents.
	// TODO: Set size of output file based on input file size.
	char* output[4096] = { 0 };

	// Read contents of ROM file.
	FILE* source_file;
	source_file = fopen((filename), "rb");

	// Determine file size by seeking to the end of the file and getting the current file position.
	fseek(source_file, 0, SEEK_END);
	long size = ftell(source_file);

	// Return to the beginning of the file and read the contents into memory.
	// TODO: Constant for initial memory offset.
	fseek(source_file, 0, SEEK_SET);

	// TODO: Check input file size is less than memory size.
	fread(memory + 0x200, size, 1, source_file);

	// Close the file.
	fclose(source_file);

	FILE* fp;
	char output_string[255] = "";

	// TODO: Create files in an output directory.
	fp = fopen("disassembled_rom.txt", "w");

	// TODO: Constant for initial memory offset.
	uint16_t program_counter = 0x200;

	uint16_t current_position = program_counter;

	// Variables to track call-stack for recursive traversal.
	uint16_t stack[12] = { 0 };
	uint8_t stack_index = -1;		// -1 used to show that nothing has been added to the stack.

	// Flag to check if remaining lines are code (rather than data).
	// Once end of code is reached (indicated by jump to current position)
	// remaining file contents will be treated as sprite data.
	bool is_code = true;

	while (program_counter < (sizeof(memory) - 1))
	{
		// Fetch instruction from memory using program counter.
		uint16_t instruction = (memory[program_counter] << 8) ^ memory[program_counter + 1];

		current_position = program_counter;

		if (current_position == 0xEB1)
			printf("breakpoint");

		// Increment program counter to address of next instruction in memory.
		program_counter += 2;

		// Do not output to file if memory is zero or if line has previously been disassembled.
		if (instruction == 0)
			continue;

		// TODO: Comment
		if (!output[current_position])
		{
			output[current_position] = malloc(sizeof(char) * 255);
		}
		else
		{
			// If character pointer exists, string has already been set.
			continue;
		}

		// Write current memory address to output file.
		sprintf(output_string, "%03X: ", current_position);

		// Write instruction to output file.
		sprintf(output_string + strlen(output_string), "%04X\t", instruction);

		// Decode instruction into all possible information.
		uint8_t vx = INSTR_SECOND_NIBBLE(instruction);
		uint8_t vy = INSTR_THIRD_NIBBLE(instruction);

		uint8_t n = INSTR_FOURTH_NIBBLE(instruction);
		uint8_t nn = INSTR_SECOND_BYTE(instruction);
		uint16_t nnn = INSTR_MEM_ADDR(instruction);

		// Command Mnemonics: http://devernay.free.fr/hacks/chip8/C8TECH10.HTM

		if (is_code)
		{
			// Categorize instruction based on first nibble.
			switch (INSTR_FIRST_NIBBLE(instruction)) {

			case 0x0: // 0x00E0

				switch (nn) {

				case 0xE0: // 0x00E0
					sprintf(output_string + strlen(output_string), "CLS");
					break;

				case 0xEE: // 0x00EE
					sprintf(output_string + strlen(output_string), "RET");
					program_counter = stack[stack_index--];		// ----------------------------------------------------- TODO: Current location of troubleshooting / work. ----------------------------------------------------- //

					printf("RET:  Stack[%d] = 0x%08X\n", stack_index, stack[stack_index]);	// TODO: REMOVE
					break;

				default:
					break;
				}

				break;

			case 0x1: // 0x1NNN
				sprintf(output_string + strlen(output_string), "JP 0x%03X", nnn);

				// --- Determine validity of various conditions that may signal the end of executable code has been reached. --- //
				int is_end_of_code = 0;

				// New jump target address set to current 
				is_end_of_code = is_end_of_code && (current_position == nnn);

				// Jumping back to an old target address in main loop (TODO: possibility of false positives?).
				is_end_of_code = is_end_of_code && (stack_index == 0) && output[current_position];

				// TODO: Identify other conditions that may indicate the end of executable code.

				if (is_end_of_code)
				{
					program_counter = 0x200;
					is_code = false;
				}
				else
					program_counter = nnn;

				break;

			case 0x2: // 0x2NNN
				sprintf(output_string + strlen(output_string), "CALL 0x%03X", nnn);
				stack[++stack_index] = program_counter;
				program_counter = nnn;
				printf("CALL: Stack[%d] = 0x%08X\n", stack_index, stack[stack_index]);	// TODO: REMOVE
				break;

			case 0x3: // 0x3XNN
				sprintf(output_string + strlen(output_string), "SE V%X, 0x%02X", vx, nn);
				break;

			case 0x4: // 0x4XNN
				sprintf(output_string + strlen(output_string), "SNE V%X, 0x%02X", vx, nn);
				break;

			case 0x5: // 0x5XY0
				sprintf(output_string + strlen(output_string), "SE V%X, V%X", vx, vy);
				break;

			case 0x6: // 0x6XNN
				sprintf(output_string + strlen(output_string), "LD V%X, 0x%02X", vx, nn);
				break;

			case 0x7: // 0x7XNN
				sprintf(output_string + strlen(output_string), "ADD V%X, 0x%02X", vx, nn);
				break;

			case 0x8:

				switch (n) {
				case 0x0: // 0x8XY0
					sprintf(output_string + strlen(output_string), "LD V%X, V%X", vx, vy);
					break;

				case 0x1: // 0x8XY1
					sprintf(output_string + strlen(output_string), "OR V%X, V%X", vx, vy);
					break;

				case 0x2: // 0x8XY2
					sprintf(output_string + strlen(output_string), "AND V%X, V%X", vx, vy);
					break;

				case 0x3: // 0x8XY3
					sprintf(output_string + strlen(output_string), "XOR V%X, V%X", vx, vy);
					break;

				case 0x4: // 0x8XY4
					/*
						Add the value of register VY to register VX
						Set VF to 01 if a carry occurs
						Set VF to 00 if a carry does not occur
					*/
					sprintf(output_string + strlen(output_string), "ADD V%X, V%X", vy, vx);
					break;

				case 0x5: // 0x8XY5
					/*
						Subtract the value of register VY from register VX
						Set VF to 00 if a borrow occurs
						Set VF to 01 if a borrow does not occur
					*/
					sprintf(output_string + strlen(output_string), "SUB V%X, V%X", vy, vx);
					break;

				case 0x6: // 0x8XY6
					/*
						Store the value of register VY shifted right one bit in register VX
						Set register VF to the least significant bit prior to the shift
						VY is unchanged
					*/
					sprintf(output_string + strlen(output_string), "SHR V%X {, V%X}", vy, vx);
					break;

				case 0x7: // 0x8XY5
					/*
						Set register VX to the value of VY minus VX
						Set VF to 00 if a borrow occurs
						Set VF to 01 if a borrow does not occur
					*/
					sprintf(output_string + strlen(output_string), "SUBN V%X, V%X", vx, vy);
					break;

				case 0xE: // 0x8XYE
					/*
						Store the value of register VY shifted left one bit in register VX
						Set register VF to the most significant bit prior to the shift
						VY is unchanged
					*/
					sprintf(output_string + strlen(output_string), "SHL V%X {, V%X}", vy, vx);
					break;

				default:
					break;
				}

				break;

			case 0x9: // 0x9XY0
				sprintf(output_string + strlen(output_string), "SNE V%X, V%X", vx, vy);
				break;

			case 0xA: // 0xANNN
				sprintf(output_string + strlen(output_string), "LD I, 0x%03X", nnn);
				break;

			case 0xB: // 0xBNNN
				// TODO: Currently unable to follow jump targets calculated at runtime.
				// Track instruction during runtime, identify jump targets, and re-write disassembly output file?
				sprintf(output_string + strlen(output_string), "JP V0 0x%03X\t\t !! WARNING !! - Unresolved jump to unknown address.", nnn);
				break;

			case 0xC: // 0xCXNN
				sprintf(output_string + strlen(output_string), "RND V%X, 0x%02X", vx, nn);
				break;

			case 0xD: // 0xDXYN
				sprintf(output_string + strlen(output_string), "DRW V%X, V%X, 0x%X", vx, vy, n);
				break;

			case 0xE:  // 0xEXNN

				switch (nn) {

				case 0x9E:  // 0xEX9E
					sprintf(output_string + strlen(output_string), "SKP V%X", vx);
					break;

				case 0xA1:  // 0xEXA1
					sprintf(output_string + strlen(output_string), "SKNP V%X", vx);
					break;

				default:
					break;
				}

				break;

			case 0xF:  // 0xFXNN

				switch (nn) {

				case 0x07:  // 0xFX07
					sprintf(output_string + strlen(output_string), "LD V%X, DT", vx);
					break;

				case 0x0A:  // 0xFX0A
					sprintf(output_string + strlen(output_string), "LD V%X, K", vx);
					break;

				case 0x15:  // 0xFX15
					sprintf(output_string + strlen(output_string), "LD DT, V%X", vx);
					break;

				case 0x18:  // 0xFX18
					sprintf(output_string + strlen(output_string), "LD ST, V%X", vx);
					break;

				case 0x1E:  // 0xFX1E
					sprintf(output_string + strlen(output_string), "ADD I, V%X", vx);
					break;

				case 0x29:  // 0xFX29
					sprintf(output_string + strlen(output_string), "LD F, V%X", vx);
					break;

				case 0x33:  // 0xFX33
					sprintf(output_string + strlen(output_string), "LD B, V%X", vx);
					break;

				case 0x55:  // 0xFX55
					/*
						Store the values of registers V0 to VX inclusive in memory starting at address I
						I is set to I + X + 1 after operation
					*/
					sprintf(output_string + strlen(output_string), "LD [I], V%X", vx);
					break;

				case 0x65:  // 0xFX65
					/*
						Fill registers V0 to VX inclusive with the values stored in memory starting at address I
						I is set to I + X + 1 after operation
					*/
					sprintf(output_string + strlen(output_string), "LD V%X, [I]", vx);
					break;

				default:
					break;
				}

				break;

			default:
				break;
			}

		}
		else
		{
			// All code has been disassembled.
			// Remaining inputs should be treated as sprite data.

			for (int i = 0; i < 16; i++)
			{
				if (((instruction >> (15 - i) & 0x1)))
				{

					strcat(output_string, "X");
				}
				else
				{
					strcat(output_string, " ");
				}
			}

			strcpy(output[current_position], output_string);

			continue;

		}

		strcpy(output[current_position], output_string);
	}

	for (int i = 0; i < sizeof(output) / sizeof(*output); i++)
	{
		if (output[i])
		{
			fputs(output[i], fp);
			fputs("\n", fp);
		}
	}

	// Close the file.
	fclose(fp);

	// TODO: Free all allocated memory.


	return false;
}