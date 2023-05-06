#include "main.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int cmdShow)
{
	// Create console for debug output.
	// TODO: Output debug information to file (configureable).
	if (AllocConsole())
	{
		FILE* fi = 0;
		freopen_s(&fi, "CONOUT$", "w", stdout);
	}

	// Create window.
	static TCHAR appName[] = TEXT("Chip-8 Emulator");
	MSG msg;
	WNDCLASS wndclass;

	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = WndProc;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = hInstance;
	wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndclass.lpszMenuName = NULL;
	wndclass.lpszClassName = appName;

	if (!RegisterClass(&wndclass)) {
		MessageBox(NULL, TEXT("Could not register Window Class!"), appName, MB_ICONERROR);
	}

	// TODO: Create constants for window offset added in below?
	_hwnd = CreateWindow(appName, TEXT("Chip-8 Emulator"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, (DISPLAY_WIDTH * PIXEL_SIZE) + 16, (DISPLAY_HEIGHT * PIXEL_SIZE) + 39, NULL, NULL, hInstance, NULL);

	ShowWindow(_hwnd, cmdShow);
	UpdateWindow(_hwnd);

	// Load ROM from file and initialize memory.
	printf("Starting!\n");
	load_font_sprites();
	load_rom_from_file();

	hRunMutex = CreateMutexW(NULL, TRUE, NULL);

	// TODO: Resolve warnings on the following two lines (and elsewhere).
	(HANDLE)_beginthread(execute, 0);
	(HANDLE)_beginthread(decrement_timers, 0);

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Release mutex to kill all running threads.
	if (hRunMutex)
		ReleaseMutex(hRunMutex);

	return msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		/* TODO: Add Initialization Code? */
		return 0;
	case WM_PAINT:
		draw_display(hwnd);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		break;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

int execute() {
	do
	{
		// Fetch instruction from memory using program counter.
		uint16_t instruction = (memory[program_counter] << 8) ^ memory[program_counter + 1];

		printf("\nInstr: 0x%X\n", instruction);

		// Increment program counter to address of next instruction in memory.
		program_counter += 2;

		// Decode instruction into all possible information.
		uint8_t vx = INSTR_SECOND_NIBBLE(instruction);
		uint8_t vy = INSTR_THIRD_NIBBLE(instruction);

		uint8_t n = INSTR_FOURTH_NIBBLE(instruction);
		uint8_t nn = INSTR_SECOND_BYTE(instruction);
		uint16_t nnn = INSTR_MEM_ADDR(instruction);

		// Categorize instruction based on first nibble.
		switch (INSTR_FIRST_NIBBLE(instruction)) {
		case 0x0: // 0x00E0

			switch (nn) {
				// 0x0NNN - Not implemented.

			case 0xE0: // 0x00E0
				printf("Clear screen.\n");
				clear_display();
				break;

			case 0xEE: // 0x00EE
				printf("Return from subroutine.\n");
				program_counter = stack[stack_index--];
				break;

			default:
				NOT_IMPLEMENTED;
				break;
			}

			break;

		case 0x1: // 0x1NNN
			printf("Jump to new address.\n");
			program_counter = nnn;
			break;

		case 0x2: // 0x2NNN
			printf("Execute subroutine starting at address NNN..\n");
			stack[++stack_index] = program_counter;
			program_counter = nnn;
			break;

		case 0x3: // 0x3XNN
			printf("Skip the following instruction if the value of register VX equals NN.\n");
			if (v_reg[vx] == nn)
				program_counter += 2;
			break;

		case 0x4: // 0x4XNN
			printf("Skip the following instruction if the value of register VX is not equal to NN.\n");
			if (v_reg[vx] != nn)
				program_counter += 2;
			break;

		case 0x5: // 0x5XY0
			printf("Skip the following instruction if the value of register VX is equal to the value of register VY.\n");
			if (v_reg[vx] == v_reg[vy])
				program_counter += 2;
			break;

		case 0x6: // 0x6XNN
			printf("Set general purpose register.\n");
			v_reg[vx] = nn;
			break;

		case 0x7: // 0x7XNN
			printf("Add value to general purpose register.\n");
			v_reg[vx] += nn;
			break;

		case 0x8:

			uint8_t temp;

			switch (n) {
			case 0x0: // 0x8XY0
				printf("Store the value of register VY in register VX.\n");
				v_reg[vx] = v_reg[vy];
				break;

			case 0x1: // 0x8XY1
				printf("Set VX to VX OR VY.\n");
				v_reg[vx] |= v_reg[vy];
				break;

			case 0x2: // 0x8XY2
				printf("Set VX to VX AND VY.\n");
				v_reg[vx] &= v_reg[vy];
				break;

			case 0x3: // 0x8XY3
				printf("Set VX to VX XOR VY.\n");
				v_reg[vx] ^= v_reg[vy];
				break;

			case 0x4: // 0x8XY4
				/*
					Add the value of register VY to register VX
					Set VF to 01 if a carry occurs
					Set VF to 00 if a carry does not occur
				*/
				printf("Add the value of register VY to register VX.\n");

				// Capture carry flag prior to operation in case VF is being used.
				if ((int)(v_reg[vx] + v_reg[vy]) > 255)
					temp = 1;
				else
					temp = 0;

				v_reg[vx] += v_reg[vy];

				v_reg[0xF] = temp;

				break;

			case 0x5: // 0x8XY5
				/*
					Subtract the value of register VY from register VX
					Set VF to 00 if a borrow occurs
					Set VF to 01 if a borrow does not occur
				*/
				printf("Subtract the value of register VY from register VX.\n");

				// Capture borrow flag prior to operation in case VF is being used.
				if (v_reg[vx] >= v_reg[vy])
					temp = 1;
				else
					temp = 0;

				v_reg[vx] -= v_reg[vy];

				v_reg[0xF] = temp;

				break;

			case 0x6: // 0x8XY6
				/*
					Store the value of register VY shifted right one bit in register VX
					Set register VF to the least significant bit prior to the shift
					VY is unchanged
				*/
				printf("Store the value of register VY shifted right one bit in register VX.\n");

				temp = BIT(v_reg[vy], 7);

				if (SHIFT_COPY)
					v_reg[vx] = v_reg[vy];

				v_reg[vx] = v_reg[vx] >> 1;

				v_reg[0xF] = temp;

				break;

			case 0x7: // 0x8XY5
				/*
					Set register VX to the value of VY minus VX
					Set VF to 00 if a borrow occurs
					Set VF to 01 if a borrow does not occur
				*/
				printf("Set register VX to the value of VY minus VX.\n");

				v_reg[vx] = v_reg[vy] - v_reg[vx];

				if (v_reg[vy] > v_reg[vx])
					v_reg[0xF] = 1;
				else
					v_reg[0xF] = 0;

				break;

			case 0xE: // 0x8XYE
				/*
					Store the value of register VY shifted left one bit in register VX
					Set register VF to the most significant bit prior to the shift
					VY is unchanged
				*/
				printf("Store the value of register VY shifted right one bit in register VX.\n");

				temp = BIT(v_reg[vy], 0);

				if (SHIFT_COPY)
					v_reg[vx] = v_reg[vy];

				v_reg[vx] = v_reg[vx] << 1;

				v_reg[0xF] = temp;

				break;

			default:
				NOT_IMPLEMENTED;
				break;
			}

			break;

		case 0x9: // 0x9XY0
			printf("Skip the following instruction if the value of register VX is not equal to the value of register VY.\n");
			if (v_reg[vx] != v_reg[vy])
				program_counter += 2;
			break;

		case 0xA: // 0xANNN
			printf("Set index register.\n");
			index_register = nnn;
			break;

		case 0xB: // 0xBNNN
			printf("Jump to address NNN + V0.\n");
			program_counter = nnn + v_reg[0x0];
			break;

		case 0xC: // 0xCXNN
			printf("Set VX to a random number with a mask of NN.\n");
			v_reg[vx] = rand() & nn;
			break;

		case 0xD: // 0xDXYN
			printf("Draw screen.\n");

			int x, x_start, y, y_start;
			v_reg[0xF] = 0x0;

			// Starting positions of sprites should wrap around the display.
			x_start = v_reg[vx] % DISPLAY_WIDTH;
			y_start = v_reg[vy] % DISPLAY_HEIGHT;

			// Loop through the number of rows indicated by instruction.
			for (int i = 0; i < n; i++) {

				y = (y_start + i);

				// Sprites should clip if rendered off-screen.
				if (y >= DISPLAY_HEIGHT)
					break;

				// Loop through columns corresponding to the width of sprites (8 pixels).
				for (int j = 0; j < 8; j++) {

					x = (x_start + j);

					// Sprites should clip if rendered off-screen.
					if (x >= DISPLAY_WIDTH)
						break;

					// Check if pixel in sprite is active and set VF appropriately.
					if (BIT(memory[index_register + i], j) == 1) {

						if (display[y][x]) {
							display[y][x] = false;
							v_reg[0xF] = 0x1;
						}
						else
							display[y][x] = true;
					}
				}
			}

			break;

		case 0xE:  // 0xEXNN

			switch (nn) {

			case 0x9E:  // 0xEX9E
				printf("Skip the following instruction if the key corresponding to the hex value currently stored in register VX is pressed.\n");

				// TODO: Implement in a more efficient / readable way?
				if (v_reg[vx] == 0x1 && KEY_PRESSED(0x31))			// 1 --> 1
					program_counter += 2;
				else if (v_reg[vx] == 0x2 && KEY_PRESSED(0x32))		// 2 --> 2
					program_counter += 2;
				else if (v_reg[vx] == 0x3 && KEY_PRESSED(0x33))		// 3 --> 3
					program_counter += 2;
				else if (v_reg[vx] == 0xC && KEY_PRESSED(0x34))		// 4 --> C
					program_counter += 2;
				else if (v_reg[vx] == 0x4 && KEY_PRESSED(0x51))		// Q --> 4
					program_counter += 2;
				else if (v_reg[vx] == 0x5 && KEY_PRESSED(0x57))		// W --> 5
					program_counter += 2;
				else if (v_reg[vx] == 0x6 && KEY_PRESSED(0x45))		// E --> 6
					program_counter += 2;
				else if (v_reg[vx] == 0xD && KEY_PRESSED(0x52))		// R --> D
					program_counter += 2;
				else if (v_reg[vx] == 0x7 && KEY_PRESSED(0x41))		// A --> 7
					program_counter += 2;
				else if (v_reg[vx] == 0x8 && KEY_PRESSED(0x53))		// S --> 8
					program_counter += 2;
				else if (v_reg[vx] == 0x9 && KEY_PRESSED(0x44))		// D --> 9
					program_counter += 2;
				else if (v_reg[vx] == 0xE && KEY_PRESSED(0x46))		// F --> E
					program_counter += 2;
				else if (v_reg[vx] == 0xA && KEY_PRESSED(0x5A))		// Z --> A
					program_counter += 2;
				else if (v_reg[vx] == 0x0 && KEY_PRESSED(0x58))		// X --> 0
					program_counter += 2;
				else if (v_reg[vx] == 0xB && KEY_PRESSED(0x43))		// C --> B
					program_counter += 2;
				else if (v_reg[vx] == 0xF && KEY_PRESSED(0x56))		// V --> F
					program_counter += 2;

				break;

			case 0xA1:  // 0xEXA1
				printf("Skip the following instruction if the key corresponding to the hex value currently stored in register VX is not pressed.\n");

				if (v_reg[vx] == 0x1 && !KEY_PRESSED(0x31))			// 1 --> 1
					program_counter += 2;
				else if (v_reg[vx] == 0x2 && !KEY_PRESSED(0x32))		// 2 --> 2
					program_counter += 2;
				else if (v_reg[vx] == 0x3 && !KEY_PRESSED(0x33))		// 3 --> 3
					program_counter += 2;
				else if (v_reg[vx] == 0xC && !KEY_PRESSED(0x34))		// 4 --> C
					program_counter += 2;
				else if (v_reg[vx] == 0x4 && !KEY_PRESSED(0x51))		// Q --> 4
					program_counter += 2;
				else if (v_reg[vx] == 0x5 && !KEY_PRESSED(0x57))		// W --> 5
					program_counter += 2;
				else if (v_reg[vx] == 0x6 && !KEY_PRESSED(0x45))		// E --> 6
					program_counter += 2;
				else if (v_reg[vx] == 0xD && !KEY_PRESSED(0x52))		// R --> D
					program_counter += 2;
				else if (v_reg[vx] == 0x7 && !KEY_PRESSED(0x41))		// A --> 7
					program_counter += 2;
				else if (v_reg[vx] == 0x8 && !KEY_PRESSED(0x53))		// S --> 8
					program_counter += 2;
				else if (v_reg[vx] == 0x9 && !KEY_PRESSED(0x44))		// D --> 9
					program_counter += 2;
				else if (v_reg[vx] == 0xE && !KEY_PRESSED(0x46))		// F --> E
					program_counter += 2;
				else if (v_reg[vx] == 0xA && !KEY_PRESSED(0x5A))		// Z --> A
					program_counter += 2;
				else if (v_reg[vx] == 0x0 && !KEY_PRESSED(0x58))		// X --> 0
					program_counter += 2;
				else if (v_reg[vx] == 0xB && !KEY_PRESSED(0x43))		// C --> B
					program_counter += 2;
				else if (v_reg[vx] == 0xF && !KEY_PRESSED(0x56))		// V --> F
					program_counter += 2;

				break;

			default:
				NOT_IMPLEMENTED;
				break;
			}

			break;

		case 0xF:  // 0xFXNN

			switch (nn) {

			case 0x07:  // 0xFX07
				printf("Store the current value of the delay timer in register VX.\n");
				v_reg[vx] = delay_timer;
				break;

			case 0x0A:  // 0xFX0A
				printf("Wait for a keypress and store the result in register VX.\n");

				if (KEY_PRESSED(0x31))			// 1 --> 1
					v_reg[vx] = 0x1;
				else if (KEY_PRESSED(0x32))		// 2 --> 2
					v_reg[vx] = 0x2;
				else if (KEY_PRESSED(0x33))		// 3 --> 3
					v_reg[vx] = 0x3;
				else if (KEY_PRESSED(0x34))		// 4 --> C
					v_reg[vx] = 0xC;
				else if (KEY_PRESSED(0x51))		// Q --> 4
					v_reg[vx] = 0x4;
				else if (KEY_PRESSED(0x57))		// W --> 5
					v_reg[vx] = 0x5;
				else if (KEY_PRESSED(0x45))		// E --> 6
					v_reg[vx] = 0x6;
				else if (KEY_PRESSED(0x52))		// R --> D
					v_reg[vx] = 0xD;
				else if (KEY_PRESSED(0x41))		// A --> 7
					v_reg[vx] = 0x7;
				else if (KEY_PRESSED(0x53))		// S --> 8
					v_reg[vx] = 0x8;
				else if (KEY_PRESSED(0x44))		// D --> 9
					v_reg[vx] = 0x9;
				else if (KEY_PRESSED(0x46))		// F --> E
					v_reg[vx] = 0xE;
				else if (KEY_PRESSED(0x5A))		// Z --> A
					v_reg[vx] = 0xA;
				else if (KEY_PRESSED(0x58))		// X --> 0
					v_reg[vx] = 0x0;
				else if (KEY_PRESSED(0x43))		// C --> B
					v_reg[vx] = 0xB;
				else if (KEY_PRESSED(0x56))		// V --> F
					v_reg[vx] = 0xF;
				else {
					// Program counter should be reduced to hold execution at this instruction.
					program_counter -= 2;
				}

				break;

			case 0x15:  // 0xFX15
				printf("Set the delay timer to the value of register VX.\n");
				delay_timer = v_reg[vx];
				break;

			case 0x18:  // 0xFX18
				printf("Set the sound timer to the value of register VX.\n");
				sound_timer = v_reg[vx];
				break;

			case 0x1E:  // 0xFX1E
				printf("Add the value stored in register VX to register I.\n");
				index_register += v_reg[vx];
				break;

			case 0x29:  // 0xFX29
				printf("Set I to the memory address of the sprite data corresponding to the hexadecimal digit stored in register VX.\n");
				index_register = FONT_ADDR_START + v_reg[vx] - 0x1;
				break;

			case 0x33:  // 0xFX33
				printf("Store the binary-coded decimal equivalent of the value stored in register VX at addresses I, I + 1, and I + 2.\n");

				memory[index_register] = v_reg[vx] / 100;
				memory[index_register + 1] = (v_reg[vx] / 10) % 10;
				memory[index_register + 2] = v_reg[vx] % 10;

				break;

			case 0x55:  // 0xFX55
				/*
					Store the values of registers V0 to VX inclusive in memory starting at address I
					I is set to I + X + 1 after operation
				*/
				printf("Store the values of registers V0 to VX inclusive in memory starting at address I.\n");

				for (int i = 0; i <= vx; i++)
				{
					memory[index_register] = v_reg[i];
					index_register++;
				}

				index_register++;

				break;

			case 0x65:  // 0xFX65
				/*
					Fill registers V0 to VX inclusive with the values stored in memory starting at address I
					I is set to I + X + 1 after operation
				*/
				printf("Fill registers V0 to VX inclusive with the values stored in memory starting at address I.\n");

				for (int i = 0; i <= vx; i++)
				{
					v_reg[i] = memory[index_register];
					index_register++;
				}

				index_register++;

				break;

			default:
				NOT_IMPLEMENTED;
				break;
			}

			break;

		default:
			NOT_IMPLEMENTED;
			break;
		}
		// TODO: Update to run at 700 commands per second (configureable).
		// TODO: Update for single-step mode with user intput.
	} while (WaitForSingleObject(hRunMutex, 1) == WAIT_TIMEOUT);

	return 0;
}

/*
	TODO: Need to improve implementation to only re-draw pixels which have been updated.
	Current implementation causes major issue with speed of execution which impacts
	overall functionality/capabilities.
*/
void draw_display(HWND hwnd) {
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);

	HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
	HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
	SelectObject(hdc, whiteBrush);

	// static bool prevDisplay[DISPLAY_HEIGHT][DISPLAY_WIDTH] = { 0 };

	for (int i = 0; i < DISPLAY_HEIGHT; i++) {
		for (int j = 0; j < DISPLAY_WIDTH; j++) {

			if (display[i][j]) {
				SelectObject(hdc, whiteBrush);
			}
			else {
				SelectObject(hdc, blackBrush);
			}

			Rectangle(hdc, (j * PIXEL_SIZE),
				(i * PIXEL_SIZE),
				(j + 1) * PIXEL_SIZE,
				(i + 1) * PIXEL_SIZE);
		}
	}

	// memcpy(prevDisplay, &display, sizeof(display));
	// redrawDisplay = false;

	EndPaint(hwnd, &ps);

	RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);
}

void clear_display() {
	// Set display values for all pixels to false/off.
	for (int i = 0; i < DISPLAY_HEIGHT; i++) {
		for (int j = 0; j < DISPLAY_WIDTH; j++) {
			display[i][j] = false;
		}
	}

	// Redraw blank screen.
	draw_display(_hwnd);
}

void load_rom_from_file() {
	printf("Loading ROM from file.\n");

	// Read contents of ROM file.
	// TODO: Allow selection / configuration of ROMs (i.e. don't hardcode file names).
	// TODO: Add error handling (missing files, corruption, etc.).
	// TODO: Scan files for unsupported instructions prior to loading?
	// TODO: Generate file that contains disassembled output.
	FILE* fp;

	// ---- Chip-8 Test Suite ----//
	// Source: https://github.com/Timendus/chip8-test-suite
	//fp = fopen("roms/1-chip8-logo.ch8", "rb");
	//fp = fopen("roms/2-ibm-logo.ch8", "rb");
	//fp = fopen("roms/3-corax+.ch8", "rb");
	//fp = fopen("roms/4-flags.ch8", "rb");
	fp = fopen("roms/5-quirks.ch8", "rb");		// Requires inputs + timers.
	//fp = fopen("roms/6-keypad.ch8", "rb");	// Requires inputs + timers.

	// Additional test ROMs.
	//fp = fopen("roms/delay_timer_test.ch8", "rb");	// Source: https://github.com/mattmikolay/chip-8/tree/master/delaytimer
	//fp = fopen("roms/random_number_test.ch8", "rb");	// Source: https://github.com/mattmikolay/chip-8/tree/master/randomnumber

	// Determine file size by seeking to the end of the file and getting the current file position.
	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);

	// Return to the beginning of the file and read the contents into memory.
	// TODO: Constant for initial memory offset.
	fseek(fp, 0, SEEK_SET);
	fread(memory + 0x200, size, 1, fp);

	// Close the file.
	fclose(fp);
}

void load_font_sprites() {
	// Load font sprints into memory. 
	uint8_t fonts[] = {
		0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
		0x20, 0x60, 0x20, 0x20, 0x70, // 1
		0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
		0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
		0x90, 0x90, 0xF0, 0x10, 0x10, // 4
		0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
		0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
		0xF0, 0x10, 0x20, 0x40, 0x40, // 7
		0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
		0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
		0xF0, 0x90, 0xF0, 0x90, 0x90, // A
		0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
		0xF0, 0x80, 0x80, 0x80, 0xF0, // C
		0xE0, 0x90, 0x90, 0x90, 0xE0, // D
		0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
		0xF0, 0x80, 0xF0, 0x80, 0x80  // F
	};

	// Load sprites into memory.
	for (int i = FONT_ADDR_START; i <= FONT_ADDR_END; i++) {
		memory[i] = fonts[i - FONT_ADDR_START];
	}
}

void decrement_timers() {

	do {
		if (delay_timer > 0)
			delay_timer--;

		if (sound_timer > 0)
		{
			// TODO: Play sound.
			sound_timer--;
		}
	} while (WaitForSingleObject(hRunMutex, 16) == WAIT_TIMEOUT);	// TODO: Update to run with a period of 16.6 ms.

	return;
}