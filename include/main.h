#include <Arduino.h>
#include <SAM3XDUE.H>
#include "scene_manager.hpp"
#include <dueprinter.hpp>
#include <cstdio>


void refresh_logic();

DuePrinter debug_printer;
static char debug_buffer[100];


void init_screen_refresh_timer(uint32_t refresh_rate = 60) {
	pmc_enable_periph_clk(ID_TC3);
	//mclk/128, wave mode
	REG_TC1_CMR0 = 3 | (0b10 << 13) | (1 << 15);
	const uint32_t refresh_count = 656220U / refresh_rate;
	REG_TC1_RC0 = refresh_count;
	REG_TC1_IER0 = (1 << 4);
	REG_TC1_IMR0 = (1 << 4);

	NVIC_ClearPendingIRQ(TC3_IRQn);
	NVIC_SetPriority(TC3_IRQn, 3);
	NVIC_EnableIRQ(TC3_IRQn);
	REG_TC1_CCR0 = 1 | (1 << 2);
}


/*
						 X-Axis
	(239 [lcd width],0)  _______ (0, 0)
						|		|
						|  LCD	|
						|       |  Y-Axis
						|       |
						|_______|
								  (0, 319 [lcd height])
	 __   __
	|P1| |P2|
	|UP| |UP|
	|__| |__|

	 __   __
	|P2| |P2|
	|DN| |DN|
	|__| |__|

*/

#define P2_UP PIN_28D
#define P2_DOWN PIN_26D

#define P1_UP PIN_27D
#define P1_DOWN PIN_25D
#define P1_LEFT P2_DOWN
#define P1_RIGHT P2_UP
void init_pong_input() {
	//PD0, PD1, PD2, PD3, pins 25, 26, 27, 28
	pmc_enable_periph_clk(ID_PIOD);
	pio_enable_pio(PIOD, PIN_25D | PIN_26D | PIN_27D | PIN_28D);
	pio_disable_output(PIOD, PIN_25D | PIN_26D | PIN_27D | PIN_28D);
	pio_enable_pullup(PIOD, PIN_25D | PIN_26D | PIN_27D | PIN_28D);
}

#define DUE_DOOM_ENABLE_DEBUG_PINS
#ifdef DUE_DOOM_ENABLE_DEBUG_PINS
void init_debug_pins() {
#define DEBUG_PIN_22 PIOB, PIN_22B
#define DEBUG_PIN_23 PIOA, PIN_23A
#define DEBUG_PIN_24 PIOA, PIN_24A
#define DEBUG_PIN_31 PIOA, PIN_31A

#define TC0_DEBUG_PIN DEBUG_PIN_22
#define TC3_DEBUG_PIN DEBUG_PIN_23
#define USART0_DEBUG_PIN DEBUG_PIN_22
#define UART_DEBUG_PIN DEBUG_PIN_22
#define DMAC_DEBUG_PIN DEBUG_PIN_24
#define CONTROLLER_BUTTON_UPDATE_PIN DEBUG_PIN_22

	//this is for testing, pin 22 DMAC, pin 23 - pong TC3, pin 24 n64 controller update
	pio_enable_pio(PIOA, PIN_23A | PIN_24A | PIN_31A);
	pio_disable_pullup(PIOA, PIN_23A | PIN_24A | PIN_31A);
	pio_enable_output(PIOA, PIN_23A | PIN_24A | PIN_31A);
	pio_output_write_HIGH(PIOA, PIN_23A | PIN_24A | PIN_31A);
	pio_enable_pio(PIOB, PIN_22B);
	pio_disable_pullup(PIOB, PIN_22B);
	pio_enable_output(PIOB, PIN_22B);
	pio_output_write_HIGH(PIOB, PIN_22B);

}

#define DEBUG_PIN_LOW(...) pio_output_write_LOW(__VA_ARGS__)
#define DEBUG_PIN_HIGH(...) pio_output_write_HIGH(__VA_ARGS__)
#define INIT_DEBUG_PINS() init_debug_pins()
#else
#define INIT_DEBUG_PINS()
#define DEBUG_PIN_LOW(pin)
#define DEBUG_PIN_HIGH(pin)
#endif

void init_dmac_irq() {
	REG_DMAC_EBCIDR = ~0;
	NVIC_ClearPendingIRQ(DMAC_IRQn);
	NVIC_SetPriority(DMAC_IRQn, 1);
	NVIC_EnableIRQ(DMAC_IRQn);
	REG_DMAC_EBCIMR |= dma_draw_list::CBTC0;
	REG_DMAC_EBCIER |= dma_draw_list::CBTC0;
}

extern "C" char* sbrk(int incr);
int get_free_ram() {
	char top_of_stack;
	return &top_of_stack - reinterpret_cast<char*>(sbrk(0));
}

static char ram_msg_buffer[64] = "Engine initialized. Free RAM: ";

void print_ram_usage() {
	int ram = get_free_ram();
	int idx = 30;
	char temp[10];
	int temp_idx = 0;
	do {
		temp[temp_idx++] = (ram % 10) + '0';
		ram /= 10;
	} while (ram > 0);
	while (temp_idx > 0) {
		ram_msg_buffer[idx++] = temp[--temp_idx];
	}
	const char* suffix = " bytes\n";
	while (*suffix) {
		ram_msg_buffer[idx++] = *suffix++;
	}
	ram_msg_buffer[idx] = '\0';
	debug_printer.queue_write(ram_msg_buffer);
}

