#include "main.h"

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int cmdShow)
{
	// Create console for debug output.
	// TODO: Output debug information to file (configurable).
	if (AllocConsole())
	{
		FILE* fi = 0;
		freopen_s(&fi, "CONOUT$", "w", stdout);
	}

	// Create window.
	static TCHAR appName[] = TEXT("Chip-8 Emulator");
	MSG msg;
	WNDCLASS wndclass = { 0 };

	wndclass.style = CS_HREDRAW | CS_VREDRAW;	// Redraw the window for any movement / size adjustment.
	wndclass.lpfnWndProc = WindowProcedure;
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
	_hwnd = CreateWindow(appName, appName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, (DISPLAY_WIDTH * PIXEL_SIZE) + 16, (DISPLAY_HEIGHT * PIXEL_SIZE) + 230 + 39, NULL, NULL, hInstance, NULL);

	ShowWindow(_hwnd, cmdShow);
	UpdateWindow(_hwnd);

	// Load ROM from file and initialize memory.
	printf("Starting!\n");
	load_font_sprites();
	load_rom_from_file();

	// Set start time so that refresh rate can be emulated based on elapsed time.
	QueryPerformanceFrequency(&qpc_frequency);
	QueryPerformanceCounter(&previous_refresh_time);

	// Initialize the critical section one time only.
	// Critical section used to protect user configureable values.
	if (!InitializeCriticalSectionAndSpinCount(&critical_section,
		0x00000400))
		return;

	// TODO: Resolve warnings on the following line (and elsewhere).
	(HANDLE)_beginthread(refresh_screen, 0);

	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Set flag to stop threads from running.
	is_running = false;

	// Release resources used by the critical section object.
	DeleteCriticalSection(&critical_section);

	return msg.wParam;
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		// TODO: Center window on screen?
		return 0;
	case WM_PAINT:
		draw_display(hwnd);
		draw_system_state_text(hwnd);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
		// TODO: Capture keys that increase/decrease number of cycles per frame.
	case WM_KEYDOWN:
		if (wParam == VK_SHIFT)
		{
			single_step_mode = !single_step_mode;
		}

		if (wParam == VK_SPACE)
		{
			single_step_command_count++;
		}

	default:
		break;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

void execute_commands(int number_of_commands) {

	// TODO: Update to run at a user-configurable clock speed.

	while (number_of_commands > 0)
	{
		// Verify that program counter does not point past the end of memory.
		if ((size_t)(program_counter + 1) > sizeof(memory))
			break;	// TODO: Error handling.

		// Fetch instruction from memory using program counter.
		uint16_t instruction = (memory[program_counter] << 8) ^ memory[program_counter + 1];

		printf("\nInstr: 0x%X\n", instruction);

		// Increment program counter to address of next instruction in memory.
		program_counter += 2;

		// Clear string that holds the current command.
		strcpy(current_command_string, "");

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
				sprintf(current_command_string + strlen(current_command_string), "CLS");
				clear_display();
				break;

			case 0xEE: // 0x00EE
				sprintf(current_command_string + strlen(current_command_string), "RET (0x%04X)", stack[stack_index]);
				//program_counter = stack[stack_index--];
				program_counter = stack[stack_index];
				stack[stack_index] = 0;
				stack_index--;
				break;

			default:
				NOT_IMPLEMENTED;
				break;
			}

			break;

		case 0x1: // 0x1NNN
			sprintf(current_command_string + strlen(current_command_string), "JP 0x%03X", nnn);
			program_counter = nnn;
			break;

		case 0x2: // 0x2NNN
			sprintf(current_command_string + strlen(current_command_string), "CALL 0x%03X", nnn);

			if (stack_index < 10)
			{
				stack_index++;
				stack[stack_index] = program_counter;
				program_counter = nnn;
			}
			else
			{
				printf("ERROR: Stack overflow.");	// TODO: Error handling.
			}

			break;

		case 0x3: // 0x3XNN
			sprintf(current_command_string + strlen(current_command_string), "SE V%X, 0x%02X", vx, nn);
			if (v_reg[vx] == nn)
				program_counter += 2;
			break;

		case 0x4: // 0x4XNN
			sprintf(current_command_string + strlen(current_command_string), "SNE V%X, 0x%02X", vx, nn);
			if (v_reg[vx] != nn)
				program_counter += 2;
			break;

		case 0x5: // 0x5XY0
			sprintf(current_command_string + strlen(current_command_string), "SE V%X, V%X", vx, vy);
			if (v_reg[vx] == v_reg[vy])
				program_counter += 2;
			break;

		case 0x6: // 0x6XNN
			sprintf(current_command_string + strlen(current_command_string), "LD V%X, 0x%02X", vx, nn);
			v_reg[vx] = nn;
			break;

		case 0x7: // 0x7XNN
			sprintf(current_command_string + strlen(current_command_string), "ADD V%X, 0x%02X", vx, nn);
			v_reg[vx] += nn;
			break;

		case 0x8:

			uint8_t temp = 0;

			switch (n) {
			case 0x0: // 0x8XY0
				sprintf(current_command_string + strlen(current_command_string), "LD V%X, V%X", vx, vy);
				v_reg[vx] = v_reg[vy];
				break;

			case 0x1: // 0x8XY1
				sprintf(current_command_string + strlen(current_command_string), "OR V%X, V%X", vx, vy);
				v_reg[vx] |= v_reg[vy];
				v_reg[0xF] = temp;
				break;

			case 0x2: // 0x8XY2
				sprintf(current_command_string + strlen(current_command_string), "AND V%X, V%X", vx, vy);
				v_reg[vx] &= v_reg[vy];
				v_reg[0xF] = temp;
				break;

			case 0x3: // 0x8XY3
				sprintf(current_command_string + strlen(current_command_string), "XOR V%X, V%X", vx, vy);
				v_reg[vx] ^= v_reg[vy];
				v_reg[0xF] = temp;
				break;

			case 0x4: // 0x8XY4
				/*
					Add the value of register VY to register VX
					Set VF to 01 if a carry occurs
					Set VF to 00 if a carry does not occur
				*/
				sprintf(current_command_string + strlen(current_command_string), "ADD V%X, V%X", vy, vx);

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
				sprintf(current_command_string + strlen(current_command_string), "SUB V%X, V%X", vy, vx);

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
				sprintf(current_command_string + strlen(current_command_string), "SHR V%X {, V%X}", vy, vx);

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
				sprintf(current_command_string + strlen(current_command_string), "SUBN V%X, V%X", vx, vy);

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
				sprintf(current_command_string + strlen(current_command_string), "SHL V%X {, V%X}", vy, vx);

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
			sprintf(current_command_string + strlen(current_command_string), "SNE V%X, V%X", vx, vy);
			if (v_reg[vx] != v_reg[vy])
				program_counter += 2;
			break;

		case 0xA: // 0xANNN
			sprintf(current_command_string + strlen(current_command_string), "LD I, 0x%03X", nnn);
			index_register = nnn;
			break;

		case 0xB: // 0xBNNN
			sprintf(current_command_string + strlen(current_command_string), "JP V0 0x%03X", nnn);
			program_counter = nnn + v_reg[0x0];
			break;

		case 0xC: // 0xCXNN
			sprintf(current_command_string + strlen(current_command_string), "RND V%X, 0x%02X", vx, nn);
			v_reg[vx] = rand() & nn;
			break;

		case 0xD: // 0xDXYN
			sprintf(current_command_string + strlen(current_command_string), "DRW V%X, V%X, 0x%X", vx, vy, n);

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
						{
							display[y][x] = true;
						}
					}
				}
			}

			break;

		case 0xE:  // 0xEXNN

			switch (nn) {

			case 0x9E:  // 0xEX9E
				sprintf(current_command_string + strlen(current_command_string), "SKP V%X", vx);

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
				sprintf(current_command_string + strlen(current_command_string), "SKNP V%X", vx);

				if (v_reg[vx] == 0x1 && !KEY_PRESSED(0x31))				// 1 --> 1
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
				sprintf(current_command_string + strlen(current_command_string), "LD V%X, DT", vx);
				v_reg[vx] = delay_timer;
				break;

			case 0x0A:  // 0xFX0A
				sprintf(current_command_string + strlen(current_command_string), "LD V%X, K", vx);

				// TODO: Optimize?
				if (KEY_PRESSED(0x31))			// 1 --> 1
				{
					// Wait for key to be released.
					while (KEY_PRESSED(0x31));
					v_reg[vx] = 0x1;
				}
				else if (KEY_PRESSED(0x32))		// 2 --> 2
				{
					// Wait for key to be released.
					while (KEY_PRESSED(0x32));
					v_reg[vx] = 0x2;
				}
				else if (KEY_PRESSED(0x33))		// 3 --> 3
				{
					// Wait for key to be released.
					while (KEY_PRESSED(0x33));
					v_reg[vx] = 0x3;
				}
				else if (KEY_PRESSED(0x34))		// 4 --> C
				{
					// Wait for key to be released.
					while (KEY_PRESSED(0x34));
					v_reg[vx] = 0xC;
				}
				else if (KEY_PRESSED(0x51))		// Q --> 4
				{
					// Wait for key to be released.
					while (KEY_PRESSED(0x51));
					v_reg[vx] = 0x4;
				}
				else if (KEY_PRESSED(0x57))		// W --> 5
				{
					// Wait for key to be released.
					while (KEY_PRESSED(0x57));
					v_reg[vx] = 0x5;
				}
				else if (KEY_PRESSED(0x45))		// E --> 6
				{
					// Wait for key to be released.
					while (KEY_PRESSED(0x45));
					v_reg[vx] = 0x6;
				}
				else if (KEY_PRESSED(0x52))		// R --> D
				{
					// Wait for key to be released.
					while (KEY_PRESSED(0x52));
					v_reg[vx] = 0xD;
				}
				else if (KEY_PRESSED(0x41))		// A --> 7
				{
					// Wait for key to be released.
					while (KEY_PRESSED(0x41));
					v_reg[vx] = 0x7;
				}
				else if (KEY_PRESSED(0x53))		// S --> 8
				{
					// Wait for key to be released.
					while (KEY_PRESSED(0x53));
					v_reg[vx] = 0x8;
				}
				else if (KEY_PRESSED(0x44))		// D --> 9
				{
					// Wait for key to be released.
					while (KEY_PRESSED(0x44));
					v_reg[vx] = 0x9;
				}
				else if (KEY_PRESSED(0x46))		// F --> E
				{
					// Wait for key to be released.
					while (KEY_PRESSED(0x46));
					v_reg[vx] = 0xE;
				}
				else if (KEY_PRESSED(0x5A))		// Z --> A
				{
					// Wait for key to be released.
					while (KEY_PRESSED(0x5A));
					v_reg[vx] = 0xA;
				}
				else if (KEY_PRESSED(0x58))		// X --> 0
				{
					// Wait for key to be released.
					while (KEY_PRESSED(0x58));
					v_reg[vx] = 0x0;
				}
				else if (KEY_PRESSED(0x43))		// C --> B
				{
					// Wait for key to be released.
					while (KEY_PRESSED(0x43));
					v_reg[vx] = 0xB;
				}
				else if (KEY_PRESSED(0x56))		// V --> F
				{
					// Wait for key to be released.
					while (KEY_PRESSED(0x56));
					v_reg[vx] = 0xF;
				}
				else {
					// Program counter should be reduced to hold execution at this instruction.
					program_counter -= 2;
				}

				break;

			case 0x15:  // 0xFX15
				sprintf(current_command_string + strlen(current_command_string), "LD DT, V%X", vx);

				EnterCriticalSection(&critical_section);
				delay_timer = v_reg[vx];
				LeaveCriticalSection(&critical_section);

				break;

			case 0x18:  // 0xFX18
				sprintf(current_command_string + strlen(current_command_string), "LD ST, V%X", vx);

				EnterCriticalSection(&critical_section);
				sound_timer = v_reg[vx];
				LeaveCriticalSection(&critical_section);

				break;

			case 0x1E:  // 0xFX1E
				sprintf(current_command_string + strlen(current_command_string), "ADD I, V%X", vx);
				index_register += v_reg[vx];
				break;

			case 0x29:  // 0xFX29
				sprintf(current_command_string + strlen(current_command_string), "LD F, V%X", vx);
				index_register = FONT_ADDR_START + (v_reg[vx] * FONT_CHARACTER_SIZE_BYTES);
				break;

			case 0x33:  // 0xFX33
				sprintf(current_command_string + strlen(current_command_string), "LD B, V%X", vx);

				memory[index_register] = v_reg[vx] / 100;
				memory[index_register + 1] = (v_reg[vx] / 10) % 10;
				memory[index_register + 2] = v_reg[vx] % 10;

				break;

			case 0x55:  // 0xFX55
				/*
					Store the values of registers V0 to VX inclusive in memory starting at address I
					I is set to I + X + 1 after operation
				*/
				sprintf(current_command_string + strlen(current_command_string), "LD [I], V%X", vx);

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
				sprintf(current_command_string + strlen(current_command_string), "LD V%X, [I]", vx);

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

		printf("%s\n", current_command_string);
		strcpy(current_command_string, current_command_string);

		number_of_commands--;
	}
}

void draw_display(HWND hwnd)
{
	PAINTSTRUCT ps;
	HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
	HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));

	HDC hdc = BeginPaint(hwnd, &ps);
	SelectObject(hdc, whiteBrush);

	// TODO: Optimize to only re-draw pixels which have changed?
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

	EndPaint(hwnd, &ps);
}

void clear_display() {
	// Set display values for all pixels to false/off.
	for (int i = 0; i < DISPLAY_HEIGHT; i++) {
		for (int j = 0; j < DISPLAY_WIDTH; j++) {
			display[i][j] = false;
		}
	}
}

void load_rom_from_file() {
	printf("Loading ROM from file.\n");

	// Read contents of ROM file.
	// TODO: Allow selection / configuration of ROMs (i.e. don't hardcode file names).
	// TODO: Add error handling (missing files, corruption, etc.).
	FILE* fp;

	// ---- Chip-8 Test Suite ----//
	// Source: https://github.com/Timendus/chip8-test-suite
	//fp = fopen("roms/1-chip8-logo.ch8", "rb");
	//fp = fopen("roms/2-ibm-logo.ch8", "rb");
	//fp = fopen("roms/3-corax+.ch8", "rb");
	//fp = fopen("roms/4-flags.ch8", "rb");
	//fp = fopen("roms/5-quirks.ch8", "rb");	// TODO: "Disp. Wait" error.
	//fp = fopen("roms/6-keypad.ch8", "rb");

	// Additional test ROMs.
	//fp = fopen("roms/delay_timer_test.ch8", "rb");	// Source: https://github.com/mattmikolay/chip-8/tree/master/delaytimer
	//fp = fopen("roms/random_number_test.ch8", "rb");	// Source: https://github.com/mattmikolay/chip-8/tree/master/randomnumber

	// Non-test ROMs.
	fp = fopen("roms/snake.ch8", "rb");			// Source: https://github.com/JohnEarnest/chip8Archive/tree/master/src/snake
	//fp = fopen("roms/caveexplorer.ch8", "rb");	// Source: https://github.com/JohnEarnest/chip8Archive/tree/master/src/caveexplorer

	// Determine file size by seeking to the end of the file and getting the current file position.
	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);

	// Return to the beginning of the file and read the contents into memory.
	// TODO: Constant for initial memory offset.
	fseek(fp, 0, SEEK_SET);
	fread(memory + 0x200, size, 1, fp);

	// TODO: Move to better location after testing.
	disassemble();

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

void refresh_screen() {

	while (is_running)
	{
		// Skip execution if not enough time has passed to reach next refresh cycle.
		if (waiting_for_next_refresh_cycle())
			continue;

		// Critical section used to protect user configureable values (TODO).
		EnterCriticalSection(&critical_section);

		// Execute the appropriate number of commands per frame.
		if (single_step_mode)
		{
			execute_commands(single_step_command_count);
			single_step_command_count = 0;
		}
		else
		{
			execute_commands(cycles_per_frame);
		}

		// Decrement timer values as appropriate.
		if (delay_timer > 0)
			delay_timer--;

		if (sound_timer > 0)
		{
			// TODO: Play sound.
			sound_timer--;
		}

		LeaveCriticalSection(&critical_section);

		// Request that the main window be redrawn.
		RedrawWindow(_hwnd, NULL, NULL, RDW_INVALIDATE);
	}

	_endthread();
}

bool waiting_for_next_refresh_cycle()
{
	bool is_waiting = true;

	LARGE_INTEGER current_time = { 0 };
	LARGE_INTEGER microseconds_since_last_clock_cycle = { 0 };
	unsigned int microseconds_per_second = 1000000;

	// Determine the number of microseconds that have elapsed since the previous clock cycle.
	QueryPerformanceCounter(&current_time);
	microseconds_since_last_clock_cycle.QuadPart = current_time.QuadPart - previous_refresh_time.QuadPart;
	microseconds_since_last_clock_cycle.QuadPart *= 1000000;
	microseconds_since_last_clock_cycle.QuadPart /= qpc_frequency.QuadPart;

	// Determine if enough time has passed to reach the next clock cycle based on the period.
	if (microseconds_since_last_clock_cycle.QuadPart > (microseconds_per_second / REFRESH_RATE_HZ))
	{
		// Update saved timestamp if new clock cycle is reached.
		QueryPerformanceCounter(&previous_refresh_time);
		is_waiting = false;
	}

	return is_waiting;
}

bool disassemble()
{
	// TODO: Update to disassemble recursively (versus current linear implementation).
	//		 Current implementation will treat data (such as sprites) as bad op_codes.
	//       https://reverseengineering.stackexchange.com/questions/2347/what-is-the-algorithm-used-in-recursive-traversal-disassembly

	// TODO: Update to read directly from file (decouple from loading ROM into memory).

	FILE* fp;
	char output_string[255] = "";

	// TODO: Create files in an output directory.
	fp = fopen("disassembled_rom.txt", "w");

	// TODO: Constant for initial memory offset.
	uint16_t current_position = 0x200;

	while (current_position < (sizeof(memory) - 1))
	{
		// Fetch instruction from memory using program counter.
		uint16_t instruction = (memory[current_position] << 8) ^ memory[current_position + 1];

		// Increment program counter to address of next instruction in memory.
		current_position += 2;

		// Do not output to file if memory is zero.
		if (instruction == 0)
			continue;

		// Write current memory address to output file.
		sprintf(output_string, "%03X: ", current_position - 2);
		fputs(output_string, fp);

		// Write instruction to output file.
		sprintf(output_string, "%04X\t", instruction);
		fputs(output_string, fp);

		// Decode instruction into all possible information.
		uint8_t vx = INSTR_SECOND_NIBBLE(instruction);
		uint8_t vy = INSTR_THIRD_NIBBLE(instruction);

		uint8_t n = INSTR_FOURTH_NIBBLE(instruction);
		uint8_t nn = INSTR_SECOND_BYTE(instruction);
		uint16_t nnn = INSTR_MEM_ADDR(instruction);

		// Clear output for fall-through in error cases.
		sprintf(output_string, "");

		// Categorize instruction based on first nibble.
		switch (INSTR_FIRST_NIBBLE(instruction)) {

		case 0x0: // 0x00E0

			switch (nn) {

			case 0xE0: // 0x00E0
				sprintf(output_string, "CLS");
				break;

			case 0xEE: // 0x00EE
				sprintf(output_string, "RET");
				break;

			default:
				break;
			}

			break;

		case 0x1: // 0x1NNN
			sprintf(output_string, "JP 0x%03X", nnn);
			break;

		case 0x2: // 0x2NNN
			sprintf(output_string, "CALL 0x%03X", nnn);
			break;

		case 0x3: // 0x3XNN
			sprintf(output_string, "SE V%X, 0x%02X", vx, nn);
			break;

		case 0x4: // 0x4XNN
			sprintf(output_string, "SNE V%X, 0x%02X", vx, nn);
			break;

		case 0x5: // 0x5XY0
			sprintf(output_string, "SE V%X, V%X", vx, vy);
			break;

		case 0x6: // 0x6XNN
			sprintf(output_string, "LD V%X, 0x%02X", vx, nn);
			break;

		case 0x7: // 0x7XNN
			sprintf(output_string, "ADD V%X, 0x%02X", vx, nn);
			break;

		case 0x8:

			switch (n) {
			case 0x0: // 0x8XY0
				sprintf(output_string, "LD V%X, V%X", vx, vy);
				break;

			case 0x1: // 0x8XY1
				sprintf(output_string, "OR V%X, V%X", vx, vy);
				break;

			case 0x2: // 0x8XY2
				sprintf(output_string, "AND V%X, V%X", vx, vy);
				break;

			case 0x3: // 0x8XY3
				sprintf(output_string, "XOR V%X, V%X", vx, vy);
				break;

			case 0x4: // 0x8XY4
				/*
					Add the value of register VY to register VX
					Set VF to 01 if a carry occurs
					Set VF to 00 if a carry does not occur
				*/
				sprintf(output_string, "ADD V%X, V%X", vy, vx);
				break;

			case 0x5: // 0x8XY5
				/*
					Subtract the value of register VY from register VX
					Set VF to 00 if a borrow occurs
					Set VF to 01 if a borrow does not occur
				*/
				sprintf(output_string, "SUB V%X, V%X", vy, vx);
				break;

			case 0x6: // 0x8XY6
				/*
					Store the value of register VY shifted right one bit in register VX
					Set register VF to the least significant bit prior to the shift
					VY is unchanged
				*/
				sprintf(output_string, "SHR V%X {, V%X}", vy, vx);
				break;

			case 0x7: // 0x8XY5
				/*
					Set register VX to the value of VY minus VX
					Set VF to 00 if a borrow occurs
					Set VF to 01 if a borrow does not occur
				*/
				sprintf(output_string, "SUBN V%X, V%X", vx, vy);
				break;

			case 0xE: // 0x8XYE
				/*
					Store the value of register VY shifted left one bit in register VX
					Set register VF to the most significant bit prior to the shift
					VY is unchanged
				*/
				sprintf(output_string, "SHL V%X {, V%X}", vy, vx);
				break;

			default:
				break;
			}

			break;

		case 0x9: // 0x9XY0
			sprintf(output_string, "SNE V%X, V%X", vx, vy);
			break;

		case 0xA: // 0xANNN
			sprintf(output_string, "LD I, 0x%03X", nnn);
			break;

		case 0xB: // 0xBNNN
			sprintf(output_string, "JP V0 0x%03X", nnn);
			break;

		case 0xC: // 0xCXNN
			sprintf(output_string, "RND V%X, 0x%02X", vx, nn);
			break;

		case 0xD: // 0xDXYN
			sprintf(output_string, "DRW V%X, V%X, 0x%X", vx, vy, n);
			break;

		case 0xE:  // 0xEXNN

			switch (nn) {

			case 0x9E:  // 0xEX9E
				sprintf(output_string, "SKP V%X", vx);
				break;

			case 0xA1:  // 0xEXA1
				sprintf(output_string, "SKNP V%X", vx);
				break;

			default:
				break;
			}

			break;

		case 0xF:  // 0xFXNN

			switch (nn) {

			case 0x07:  // 0xFX07
				sprintf(output_string, "LD V%X, DT", vx);
				break;

			case 0x0A:  // 0xFX0A
				sprintf(output_string, "LD V%X, K", vx);
				break;

			case 0x15:  // 0xFX15
				sprintf(output_string, "LD DT, V%X", vx);
				break;

			case 0x18:  // 0xFX18
				sprintf(output_string, "LD ST, V%X", vx);
				break;

			case 0x1E:  // 0xFX1E
				sprintf(output_string, "ADD I, V%X", vx);
				break;

			case 0x29:  // 0xFX29
				sprintf(output_string, "LD F, V%X", vx);
				break;

			case 0x33:  // 0xFX33
				sprintf(output_string, "LD B, V%X", vx);
				break;

			case 0x55:  // 0xFX55
				/*
					Store the values of registers V0 to VX inclusive in memory starting at address I
					I is set to I + X + 1 after operation
				*/
				sprintf(output_string, "LD [I], V%X", vx);
				break;

			case 0x65:  // 0xFX65
				/*
					Fill registers V0 to VX inclusive with the values stored in memory starting at address I
					I is set to I + X + 1 after operation
				*/
				sprintf(output_string, "LD V%X, [I]", vx);
				break;

			default:
				break;
			}

			break;

		default:
			break;
		}

		fputs(output_string, fp);
		fputs("\n", fp);
	}

	// Close the file.
	fclose(fp);

	return false;
}

char** system_state_to_strings()
{
	int debug_chars = 500;
	int debug_columns = 4;

	char** result = malloc(debug_columns * sizeof(char*));

	if (!result)
		return NULL;

	// Allocate memory for each column of the debug output and set it to an empty string.
	for (int i = 0; i < debug_columns; i++)
	{
		result[i] = malloc(debug_chars * sizeof(char));

		if (!result[i])
			return NULL;

		strcpy(result[i], "");
	}

	// --- Column 1 --- //
	// Capture current state of general purpose registers.
	for (int i = 0; i < 0x10; i++)
	{
		debug_chars -= sprintf(result[0] + strlen(result[0]), "V%X: 0x%02X\n", i, v_reg[i]);
	}

	// --- Column 2 --- //
	// Capture current state of stack.
	for (int i = 0; i < 12; i++)
	{
		debug_chars -= sprintf(result[1] + strlen(result[1]), "Stack[%02d]: 0x%04X\n", i, stack[i]);
	}

	// --- Column 3 --- //
	// Capture current state of program counter / command.
	debug_chars -= sprintf(result[2] + strlen(result[2]), "PC: 0x%02X\n", program_counter);
	debug_chars -= sprintf(result[2] + strlen(result[2]), "CMD: %s\n", current_command_string);

	// Capture current state of index register.
	// TODO: Capture current state of sprite (pointed to by index register).
	debug_chars -= sprintf(result[2] + strlen(result[2]), "\nIR: 0x%02X\n", index_register);

	// Capture current state of timers.
	debug_chars -= sprintf(result[2] + strlen(result[2]), "\nDelay: 0x%02X\n", delay_timer);
	debug_chars -= sprintf(result[2] + strlen(result[2]), "Sound: 0x%02X\n", sound_timer);


	// --- Column 4 --- //
	// Capture current state of single step mode.
	debug_chars -= sprintf(result[3] + strlen(result[3]), "Single Step: %d\n(Shift = on/off)\n(Space = next)", single_step_mode);

	return result;
}

void draw_system_state_text(HWND hwnd)
{
	char** system_state_strings = system_state_to_strings();

	RECT  rect;
	PAINTSTRUCT ps;
	HFONT hFont;
	hFont = (HFONT)GetStockObject(ANSI_FIXED_FONT);
	HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));

	HDC hdc = BeginPaint(hwnd, &ps);

	SelectObject(hdc, whiteBrush);
	Rectangle(hdc, 0, 320, 640, 550);

	SelectObject(hdc, hFont);
	GetClientRect(hwnd, &rect);
	SetTextColor(hdc, RGB(0x00, 0x00, 0x00));

	SetBkMode(hdc, TRANSPARENT);
	rect.top = 330;

	// --- Column 1 --- //
	rect.left = 10;
	DrawTextA(hdc, system_state_strings[0], strlen(system_state_strings[0]), &rect, DT_NOCLIP);

	// --- Column 2 --- //
	rect.left = 110;
	DrawTextA(hdc, system_state_strings[1], strlen(system_state_strings[1]), &rect, DT_NOCLIP);

	// --- Column 3 --- //
	rect.left = 310;
	DrawTextA(hdc, system_state_strings[2], strlen(system_state_strings[2]), &rect, DT_NOCLIP);

	// --- Column 4 --- //
	rect.left = 500;
	DrawTextA(hdc, system_state_strings[3], strlen(system_state_strings[3]), &rect, DT_NOCLIP);

	EndPaint(hwnd, &ps);

	free(system_state_strings[0]);
	free(system_state_strings[1]);
	free(system_state_strings[2]);
	free(system_state_strings[3]);

	free(system_state_strings);
}



