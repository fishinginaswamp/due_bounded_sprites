//#pragma GCC optimize ("-O3")

#include "main.h"

using std::snprintf;

const int16_t SCREEN_WIDTH = 320;
const int16_t SCREEN_HEIGHT = 240;

//required scene stuff
scene_manager<> scene;

sprite_data<1, 240> col_data_0;
sprite_data<1, 240> col_data_1;
sprite slice_0{col_data_0, 0, 0};
sprite slice_1{col_data_1, 1, 0};

Player player = { 229376, 229376, 0, 682 };


void setup() {
	//NVIC_DisableIRQ(SysTick_IRQn);

	debug_printer = DuePrinter::begin(Due_UART, 115200);
	
	INIT_OVERCLOCK_CPU();
	
	init_tft(true, true);
	init_dmac_irq();
	init_TRNG();
	INIT_DEBUG_PINS();
	init_pong_input();
	init_screen_refresh_timer();
	
	//idk why it is like this
	//blue: 0x07e0
	//green: 0x001f
	//red: 0xf800
	//for(auto& px : sample_wall_data) px = 0x07e0; //blue
	//scene.set_background(solid_bg);
	//scene.add(&sample_wall);

	slice_0.set_bg_restore(false);
    slice_1.set_bg_restore(false);
    scene.add(&slice_0);
    scene.add(&slice_1);

	print_ram_usage();
}


volatile bool timer_tick = false;
void loop() {
	while (1) {
		while (!timer_tick) {}
		timer_tick = false;
		DEBUG_PIN_LOW(TC3_DEBUG_PIN); //for testing

		refresh_logic();

		scene.render();		

		DEBUG_PIN_HIGH(TC3_DEBUG_PIN); //for testing
	}
}


volatile uint32_t columns_drawn = 320;
void refresh_logic() {
    // Read the entire Port D pin state at once for zero latency
    uint32_t piod = PIOD->PIO_PDSR;
    
    // --- ROTATION ---
    // 64 units out of 4096 = ~5.6 degrees per frame turn speed
    if (!(piod & P1_LEFT)) {
        player.angle = (player.angle - 64) & 4095;
    }
    if (!(piod & P1_RIGHT)) {
        player.angle = (player.angle + 64) & 4095;
    }

    // --- MOVEMENT ---
    if (!(piod & P1_UP)) {
        player.x += (cos_table[player.angle] * 3000) >> 16;
        player.y += (sin_table[player.angle] * 3000) >> 16;
    }
    if (!(piod & P1_DOWN)) {
        player.x -= (cos_table[player.angle] * 3000) >> 16;
        player.y -= (sin_table[player.angle] * 3000) >> 16;
    }
	
	static uint32_t last_fps_time = 0;
    static uint32_t frames_rendered = 0;
    
    // Check if the previous frame finished rendering
    if (columns_drawn >= 320) {
        render_frame(player, slice_0, slice_1, col_data_0, col_data_1);
        frames_rendered++;
    }

    // Print true hardware FPS exactly once a second
    if (millis() - last_fps_time >= 1000) {
        snprintf(debug_buffer, 100, "True FPS: %lu\n", frames_rendered);
        debug_printer.queue_write(debug_buffer);
        
        frames_rendered = 0;
        last_fps_time = millis();
    }
	
}


void DMAC_Handler() {
	volatile uint32_t dummy = REG_DMAC_EBCISR;

	DEBUG_PIN_LOW(DMAC_DEBUG_PIN); //for testing

	columns_drawn++;
	scene.service_print_list();

	DEBUG_PIN_HIGH(DMAC_DEBUG_PIN); //for testing
}

void TC3_Handler() {
	volatile uint32_t dummy = REG_TC1_SR0;
	REG_TC1_IDR0 |= (1 << 4);
	timer_tick = true;
	REG_TC1_IER0 |= (1 << 4);
}

void UART_Handler(void) {
	debug_printer.handle();
}
