//#pragma GCC optimize ("-O3")

#include "main.h"

const int16_t SCREEN_WIDTH = 320;
const int16_t SCREEN_HEIGHT = 240;

//required scene stuff
scene_manager<100,0> scene;

sprite_data<1, 240> col_data[4];
sprite slices[4] = {
    {col_data[0], 0, 0},
    {col_data[1], 1, 0},
    {col_data[2], 2, 0},
    {col_data[3], 3, 0}
};

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

	for (int i = 0; i < 4; i++) {
        slices[i].set_bg_restore(false);
        //scene.add(&slices[i]);
    }

	for(int col_i = 0; col_i < 4; ++col_i){
		for(int i = 0; i < 120; ++i){
			col_data[col_i][i] = 0xffff;
		}
		for(int i = 120; i < 240; ++i){
			col_data[col_i][i] = 0;
		}
	}

	init_sprites(scene);

	print_ram_usage();
}


volatile bool timer_tick = false;
void loop() {
	while (1) {
		while (!timer_tick) {}
		timer_tick = false;
		DEBUG_PIN_LOW(TC3_DEBUG_PIN); //for testing

		refresh_logic();

// 1. Calculate Vector to Enemy
		static int32_t enemy_world_x = 229376; 
		static int32_t enemy_world_y = 229376;

		int32_t dx = enemy_world_x - player.x;
		int32_t dy = enemy_world_y - player.y;

		// 2. Rotate into camera space (2D Vector Projection)
		// Z is the perpendicular distance, X is horizontal offset
		int32_t Z = ((int64_t)dx * cos_table[player.angle] + (int64_t)dy * sin_table[player.angle]) >> 16;
		int32_t X = (-(int64_t)dx * sin_table[player.angle] + (int64_t)dy * cos_table[player.angle]) >> 16;

		ghost_enemy.dist = Z;

		// 3. Render handling based on visibility
		if (Z > 256) { // Enemy is in front of the camera (256 prevents divide-by-zero)
			// 4. Project to Screen Space 
			// (278 is the focal length for a 60-degree FOV at 320px width)
			int32_t projected_x = 160 + (int32_t)(((int64_t)X * 278) / Z) - 10;
			int32_t projected_y = 120 - 15; 
			
			ghost_enemy.move_to(projected_x, projected_y);
		} else {
			// Enemy is behind the camera
			ghost_enemy.move_to(-100, -100); 
		}

		ghost_enemy.build_render_list(z_buffer);

		scene.render();

		DEBUG_PIN_HIGH(TC3_DEBUG_PIN); //for testing
	}
}

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
    if (render_frame(player, slices, col_data)) {
        frames_rendered++;
    }

    // Print true hardware FPS exactly once a second
    if (millis() - last_fps_time >= 1000) {
        std::snprintf(debug_buffer, 100, "True FPS: %lu\n", frames_rendered);
        debug_printer.queue_write(debug_buffer);
        
        frames_rendered = 0;
        last_fps_time = millis();
    }
	
}


volatile int32_t columns_drawn = 320;
void DMAC_Handler() {
	volatile uint32_t dummy = REG_DMAC_EBCISR;

	DEBUG_PIN_LOW(DMAC_DEBUG_PIN); //for testing

	if(columns_drawn < 320){
		uint8_t finished_idx = columns_drawn & 3;
		clear_buffer(reinterpret_cast<uint32_t>(col_data[finished_idx].begin()));
		columns_drawn++;
	}

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
