#include "Chip8.h"
#include <iostream>
#include <fstream>

const uint8_t CH8_FONTSET[80] = {
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

void unknownOpcode(uint16_t opcode);

Chip8::Chip8() {
	// Clear first 0x200 bytes of memory
	for (int i = 0; i < 0x200; i++)
		m_memory[i] = 0;

	// Load in fontset
	for (int i = 0; i < 80; i++)
		m_memory[i] = CH8_FONTSET[i];
}

void Chip8::init() {
	// Program counter starts at 0x200
	m_pc = 0x200;
	
	// Reset index register
	m_I = 0;

	// Reset stack pointer
	m_sp = 0;

	// Clear display
	clearDisp();

	// Clear registers
	for (int i = 0; i < 0xF; i++)
		m_V[i] = 0;

	// Clear stack
	for (int i = 0; i < CH8_STACK_SIZE; i++)
		m_stack[i] = 0;

	// Clear memory
	for (int i = m_pc; i < CH8_MEM_SIZE; i++)
		m_memory[i] = 0;

	// Reset delay timer
	m_dTimer = 0;

	// Reset sound timer
	m_sTimer = 0;

	// Reset key state
	for (int i = 0; i < 0xF; i++)
		m_keys[i] = false;

	// Reset wrap flag
	m_wrapFlag = false;
}

void Chip8::emulateCycle() {

	// Reset drawing flag
	m_drawFlag = false;

	// Get opcode
	uint16_t opcode = (m_memory[m_pc] << 8) | m_memory[m_pc + 1];

	// Get V register identifiers
	uint8_t x = (opcode >> 8) & 0x0F;
	uint8_t y = (opcode >> 4) & 0x0F;

	// Decode opcode
	switch (opcode & 0xF000) {
	case 0x000:
		switch (opcode & 0x00FF) {
		case 0x00E0:
			// 00E0: Clears the screen
			clearDisp();
			m_drawFlag = true;
			incrPC();
			break;

		case 0x00EE:
			// 00EE: Return from subroutine
			m_pc = m_stack[--m_sp];
			incrPC();
			break;

		default:
			std::cerr << "Trying to call RCA 1802 at " << std::hex << (0x0FFF & opcode) << std::dec << " (?)" << std::endl;
		}
		break;

	case 0x1000:
		// 1NNN: Jumps to address NNN
		m_pc = opcode & 0x0FFF;
		break;

	case 0x2000:
		// 2NNN: Call function at NNN
		m_stack[m_sp++] = m_pc;
		m_pc = opcode & 0x0FFF;
		break;

	case 0x3000:
		// 3XNN: Skips the next instruction if VX equals NN
		if (m_V[x] == (opcode & 0x00FF))
			incrPC();
		incrPC();
		break;

	case 0x4000:
		// 4XNN: Skips the next instruction if VX doesn't equal NN
		if (m_V[x] != (opcode & 0x00FF))
			incrPC();
		incrPC();
		break;

	case 0x5000:
		// 5XY0: Skips the next instruction if VX equals VY
		if (m_V[x] == m_V[y])
			incrPC();
		incrPC();
		break;

	case 0x6000:
		// 6XNN: Sets VX to NN
		m_V[x] = opcode & 0x00FF;
		incrPC();
		break;

	case 0x7000:
		// 7XNN: Adds NN to VX (carry flag unchanged)
		m_V[x] += (opcode & 0x00FF);
		incrPC();
		break;

	case 0x8000:
		switch (opcode & 0x000F) {
		case 0x0000:
			// 8XY0: Sets VX to value of VY
			m_V[x] = m_V[y];
			incrPC();
			break;

		case 0x0001:
			// 8XY1: Sets VX to VX OR VY
			m_V[x] |= m_V[y];
			incrPC();
			break;

		case 0x0002:
			// 8XY2: Sets VX to VX AND VY
			m_V[x] &= m_V[y];
			incrPC();
			break;

		case 0x0003:
			// 8XY3: Sets VX to VX XOR VY
			m_V[x] ^= m_V[y];
			incrPC();
			break;

		case 0x0004: {
			// 8XY4: Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there isn't
			uint8_t sum = m_V[x] + m_V[y];
			if (sum < m_V[x] || sum < m_V[y])
				m_V[0xF] = 1;
			else m_V[0xF] = 0;
			m_V[x] = sum;
			incrPC();
			break;
		}

		case 0x0005: {
			// 8XY5: VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there isn't
			uint8_t dif = m_V[x] - m_V[y];
			if (dif > m_V[x])
				m_V[0xF] = 0;
			else m_V[0xF] = 1;
			m_V[x] = dif;
			incrPC();
			break;
		}

		case 0x0006:
			// 8XY6: Stores the least significant bit of VX in VF and then shifts VX to the right by 1
			m_V[0xF] = m_V[x] & 0x01;
			m_V[x] >>= 1;
			incrPC();
			break;

		case 0x0007: {
			// 8XY7: Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there isn't
			uint8_t diff = m_V[y] - m_V[x];
			if (diff > m_V[y])
				m_V[0xF] = 0;
			else m_V[0xF] = 1;
			m_V[x] = diff;
			incrPC();
			break;
		}

		case 0x000E:
			// 8XYE: Stores the most significant bit of VX in VF and then shifts VX to the left by 1
			m_V[0xF] = m_V[x] >> 7;
			m_V[x] <<= 1;
			incrPC();
			break;

		default:
			unknownOpcode(opcode);
		}
		break;

	case 0x9000:
		// 9XY0: Skips the next instruction if VX doesn't equal VY
		if (m_V[x] != m_V[y])
			incrPC();
		incrPC();

	case 0xA000:
		// ANNN: Sets I to the address NNN
		m_I = opcode & 0x0FFF;
		incrPC();
		break;

	case 0xB000:
		// BNNN: Jumps to the address NNN plus V0
		m_pc = m_V[0x0] + (opcode & 0x0FFF);
		break;

	case 0xC000: {
		// CXNN: Sets VX to the result of a bitwise AND operation on a random number between 0 and 255 and NN
		uint8_t rng = rand();
		m_V[x] = rng & (opcode & 0x00FF);
		incrPC();
		break;
	}

	case 0xD000: {
		// DXYN: Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels and a height of N pixels
		// Each row of 8 pixels is read as bit-coded starting from memory location I
		// I value doesn�t change after the execution of this instruction
		// VF is set to 1 if any screen pixels are flipped from set to unset when the sprite is drawn, and to 0 if that doesn�t happen

		// Get height of sprite
		int height = opcode & 0x0F;

		// Reset VF Register since we don't know if there was collision yet
		m_V[0xF] = 0;

		// Loop for number of rows the sprite takes up
		for (int row = 0; row < height; row++) {

			// Get one row of sprite at a time
			uint8_t spriteRow = m_memory[m_I + row];

			// Each sprite is 8 pixels wide
			for (int col = 0; col < 8; col++) {

				// Get bit to check if it's set
				uint8_t bit = spriteRow & (0x80 >> col);

				// Check if bit is set
				if (bit) {

					// Check for wraparound
					if (m_V[x] + col > CH8_WIDTH) {
						if (m_wrapFlag) {
							// TODO: Implement wrapping to other side of screen behavior
						}
						else continue;
					}

					// Check if bit is already set
					if (m_gfx[m_V[x] + col][m_V[y + row]] == 1)
						m_V[0xF] = 1;

					// XOR the bit using 1 to flip it
					m_gfx[m_V[x] + col][m_V[y + row]] ^= 1;
				}

			}
		}

		m_drawFlag = true;
		incrPC();
		break;
	}

	case 0xE000:
		switch (opcode & 0x00FF) {
		case 0x009E:
			// EX9E: Skips the next instruction if the key stored in VX is pressed
			if (m_keys[m_V[x]])
				incrPC();
			incrPC();
			break;
			
		case 0x00A1:
			// EXA1: Skips the next instruction if the key stored in VX isn't pressed
			if (!m_keys[m_V[x]])
				incrPC();
			incrPC();
			break;

		default:
			unknownOpcode(opcode);
		}
		break;

	case 0xF000:
		switch (opcode & 0x00FF) {
		case 0x0007:
			// FX07: Sets VX to the value of the delay timer
			m_V[x] = m_dTimer;
			incrPC();
			break;

		case 0x000A: {
			// FX0A: A key press is awaited, and then stored in VX. Halt all instruction until key press
			bool keyIsPressed = false;
			for (int i = 0; i < 0xF; i++)
				if (m_keys[i]) {
					keyIsPressed = true;
					m_V[x] = m_keys[i];
					i = 0xF;
				}
			if (!keyIsPressed)
				return;
			incrPC();
			break;
		}

		case 0x0015:
			// FX15: Sets the delay timer to VX
			m_dTimer = m_V[x];
			incrPC();
			break;

		case 0x0018:
			// FX18: Sets the sound timer to VX
			m_sTimer = m_V[x];
			incrPC();
			break;

		case 0x001E: {
			// FX1E: Adds VX to I. VF is set to 1 when there is a range overflow and 0 when there isn't
			int vxisum = m_V[x] + m_I;
			if (vxisum > 0x0FFF)
				m_V[0xF] = 1;
			else m_V[0xF] = 0;
			m_I = vxisum;
			incrPC();
			break;
		}

		case 0x0029:
			// FX29: Sets I to the location of the sprite for the character in VX. Characters 0-F (in hexadecimal) are represented by a 4x5 font
			m_I = m_V[x] * 5;
			incrPC();
			break;

		case 0x0033:
			// FX33: Take the decimal representation of VX, place the hundreds digit in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2
			m_memory[m_I] = m_V[x] / 100;
			m_memory[m_I + 1] = (m_V[x] % 100 ) / 10;
			m_memory[m_I + 2] = (m_V[x] % 100) % 10;
			incrPC();
			break;

		case 0x0055:
			// FX55: Stores V0 to VX (including VX) in memory starting at address I. The offset from I is increased by 1 for each value written, but I itself is left unmodified
			for (int i = 0; i <= x; i++)
				m_memory[m_I + i] = m_V[i];
			incrPC();
			break;

		case 0x0065:
			// FX65: Fills V0 to VX (including VX) with values from memory starting at address I. The offset from I is increased by 1 for each value written, but I itself is left unmodified
			for (int i = 0; i <= x; i++)
				m_V[i] = m_memory[m_I + i];
			incrPC();
			break;

		default:
			unknownOpcode(opcode);
		}
		break;

	default:
		unknownOpcode(opcode);
	}

	// Update timers
	if (m_dTimer > 0)
		m_dTimer--;
	// TODO: Implement actual beeping noise rather than text
	if (m_sTimer) {
		if (m_sTimer == 1)
			std::cerr << "BEEP!\n";
		m_sTimer--;
	}

}

int Chip8::loadRom(std::string name) {

	// First 0x200 bytes reserved for interpretter (FONT files in this emulator's case)
	const int MAX_ROM_SIZE = CH8_MEM_SIZE - 0x200;

	char tempBuffer[MAX_ROM_SIZE];

	// Read ROM file into memory
	std::ifstream rom(name, std::ios::in | std::ios::binary);

	// Check if file was read into stream correctly
	if ((rom.rdstate() & std::ifstream::failbit) != 0 || (rom.rdstate() & std::ifstream::badbit) != 0) {
		std::cerr << "Error opening " << name << std::endl;
		return -1;
	}

	// Find file size
	rom.seekg(0, std::ios_base::end);

	if (rom.tellg() > MAX_ROM_SIZE) {
		std::cerr << "File too large!";
		return -2;
	}

	// Read file into temporary buffer
	rom.read(tempBuffer, MAX_ROM_SIZE);

	// Read into memory
	for (int i = 0; i < MAX_ROM_SIZE; i++)
		m_memory[0x200 + i] = tempBuffer[i];

	return 0;
}

void Chip8::clearDisp() {
	for (int i = 0; i < CH8_WIDTH; i++)
		for (int j = 0; j < CH8_HEIGHT; j++)
			m_gfx[i][j] = 0;
}

void unknownOpcode(uint16_t opcode) {
	std::cerr << "Unknown opcode: " << std::hex << opcode << std::dec << std::endl;
}