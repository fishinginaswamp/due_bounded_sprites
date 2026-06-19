#pragma GCC optimize ("-O3")

#include "main.h"

using std::snprintf;

const int16_t SCREEN_WIDTH = 320;
const int16_t SCREEN_HEIGHT = 240;

#ifdef __cplusplus
extern "C" {
#endif
	void UART_Handler(void) {
		debug_printer.handle();
	}
#ifdef __cplusplus
}
#endif


//required scene stuff
scene_manager<> scene;
tile_map<> solid_bg;


sprite_data<32,32> sample_wall_data;
sprite sample_wall{sample_wall_data,0,0};

void setup() {
	NVIC_DisableIRQ(SysTick_IRQn);

	debug_printer = DuePrinter::begin(Due_UART, 115200);

	init_tft(true, true);
	init_dmac_irq();
	init_TRNG();
	INIT_DEBUG_PINS();
	init_pong_input();
	init_screen_refresh_timer();
	
	for(auto& px : sample_wall_data) px = 0x07e0; //blue

	scene.set_background(solid_bg);

	scene.add(&sample_wall);

	print_ram_usage();
}


volatile bool timer_tick = false;
void loop() {
	while (1) {
		while (!timer_tick) {}
		timer_tick = false;
		DEBUG_PIN_LOW(TC3_DEBUG_PIN); //for testing

		//strncpy(debug_buffer, "a\n", 3);
		//debug_printer.queue_write(debug_buffer);
		refresh_logic();
		
		//strncpy(debug_buffer, "b\n", 3);
		//debug_printer.queue_write(debug_buffer);
		scene.render();
		
		//strncpy(debug_buffer, "c\n", 3);
		//debug_printer.queue_write(debug_buffer);
		DEBUG_PIN_HIGH(TC3_DEBUG_PIN); //for testing
	}
}

void refresh_logic(){
	
}


void DMAC_Handler() {
	volatile uint32_t dummy = REG_DMAC_EBCISR;

	DEBUG_PIN_LOW(DMAC_DEBUG_PIN); //for testing
	scene.service_print_list();
	DEBUG_PIN_HIGH(DMAC_DEBUG_PIN); //for testing
}

void TC3_Handler() {
	volatile uint32_t dummy = REG_TC1_SR0;
	REG_TC1_IDR0 |= (1 << 4);
	timer_tick = true;
	REG_TC1_IER0 |= (1 << 4);
}
