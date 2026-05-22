#pragma GCC optimize ("-O3")

#include <SAM3XDUE.h>

#include "scene_manager.hpp"

#include <dueprinter.hpp>


const int16_t SCREEN_WIDTH = 320;
const int16_t SCREEN_HEIGHT = 240;

DuePrinter debug_printer;

static char debug_buffer[100];

#ifdef __cplusplus
extern "C" {
#endif
	void UART_Handler(void) {
		debug_printer.handle();
	}
#ifdef __cplusplus
}
#endif


// 3. The RAM calculation function
extern "C" char* sbrk(int incr);
int get_free_ram() {
	char top_of_stack;
	return &top_of_stack - reinterpret_cast<char*>(sbrk(0));
}


scene_manager scene;

sprite_data<20, 15> mini_torso_data;
sprite_data<6, 15>  mini_l_leg_data;
sprite_data<6, 15>  mini_r_leg_data;

#ifdef SAM_16_BIT_LCD_SMC
#define M_COLOR 0xF81F // Magenta RGB565
#else
#define M_COLOR __builtin_bswap16(0xF81F) // Magenta RGB565
#endif

sprite mini_torso{ mini_torso_data, 10, 10 };
sprite mini_l_leg{ mini_l_leg_data, 10, 25 };
sprite mini_r_leg{ mini_r_leg_data, 24, 25 };
sprite_list robot_list;
multi_sprite mini_robot{ robot_list, bounding_box{ 10, 10, 29, 39 } };

tile_map<> solid_bg;
sprite_data<> solid_tile_data;

sprite_data<32, 32> floor_data;
const int NUM_FLOOR_TILES = 11;
sprite* floor_tiles[NUM_FLOOR_TILES];

// --- 3. PHYSICS & WORLD STATE (Fixed-Point Q24.8) ---
const int32_t LEVEL_WIDTH = 1000; // Your world is now 1000 pixels wide!
int32_t absolute_player_x = 160;  // Start the player in the middle of the screen
int32_t camera_x = 0;

const int16_t OFFSCREEN_BUFFER = 64;
int32_t active_window_left = 0;
int32_t active_window_right = 320 + OFFSCREEN_BUFFER;

int32_t player_vy = 0;
const int32_t GRAVITY = 153;
const int32_t JUMP_FORCE = -2304;
bool is_grounded = false;

const int16_t FLOOR_Y_LIMIT = 178;




sprite_data<16, 16> dummy_data;

struct world_entity {
	uint16_t window_number;
	uint8_t type;
	int32_t absolute_x;
	int16_t y;
};

const world_entity world_sprites[] = {
	{ 0, 1, 150, 148 },
	{ 1, 1, 400, 148 },
	{ 1, 1, 550, 148 },
	{ 2, 1, 800, 148 }
};

const uint32_t TOTAL_WORLD_SPRITES = 4;
const int32_t WINDOW_WIDTH = 320;

int32_t current_window = 0;
uint32_t right_load_index = 0;
uint32_t left_load_index = 0;

sprite entity_pool[10];




bool aabb_intersect(const bounding_box& a, const bounding_box& b) {
	return (a.xmin <= b.xmax && a.xmax >= b.xmin &&
		a.ymin <= b.ymax && a.ymax >= b.ymin);
}

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

#define ENABLE_DEBUG_PINS
#ifdef ENABLE_DEBUG_PINS
void init_debug_pins() {
#define DEBUG_PIN_22 PIOB, PIN_22B
#define DEBUG_PIN_23 PIOA, PIN_23A
#define DEBUG_PIN_24 PIOA, PIN_24A
#define DEBUG_PIN_31 PIOA, PIN_31A

//#define ENABLE_DEBUG_PINS

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
#endif

void init_dmac_irq() {
	REG_DMAC_EBCIDR = ~0;
	NVIC_ClearPendingIRQ(DMAC_IRQn);
	NVIC_SetPriority(DMAC_IRQn, 1);
	NVIC_EnableIRQ(DMAC_IRQn);
	REG_DMAC_EBCIMR |= dma_draw_list::CBTC0;
	REG_DMAC_EBCIER |= dma_draw_list::CBTC0;
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


void setup() {
	NVIC_DisableIRQ(SysTick_IRQn);

	debug_printer = DuePrinter::begin(Due_UART, 115200);


	init_tft(true, true);
	init_dmac_irq();
	init_TRNG();

	uint16_t magenta = __builtin_bswap16(0xF81F);

	// Robot Colors
	for (auto& px : mini_torso_data) px = __builtin_bswap16(0x001F); // Blue torso
	for (auto& px : mini_l_leg_data) px = __builtin_bswap16(0x07E0); // Green legs
	for (auto& px : mini_r_leg_data) px = __builtin_bswap16(0x07E0);
	robot_list << mini_torso << mini_l_leg << mini_r_leg;

	mini_robot.z_prio = 10; // Robot always on top

	// --- SETUP FLOOR ---
		// 1. Fill the whole tile with a base green color
	for (auto& px : floor_data) px = __builtin_bswap16(0x05E0);

	// 2. Draw a dark green/black vertical stripe down the left edge
	for (int y = 0; y < 32; ++y) {
		// Index = (y * width) + x. We want x = 0.
		floor_data[y * 32 + 0] = __builtin_bswap16(0x0120);
	}

	// 3. Spawn the tiles as usual
	for (int i = 0; i < NUM_FLOOR_TILES; ++i) {
		floor_tiles[i] = new sprite(floor_data, i * 32, 208);
		floor_tiles[i]->z_prio = 20;
		floor_tiles[i]->set_bg_restore(false);
		scene.add(floor_tiles[i]);
	}

	for (auto& px : solid_tile_data) px = __builtin_bswap16(0x000f); // some shade of red
	solid_bg.set_palette_tile(0, solid_tile_data);
	scene.set_background(solid_bg);
	mini_robot.move_to(160, 148);
	scene.add(&mini_robot);

	for (auto& px : dummy_data) px = __builtin_bswap16(0xF800);
	for (int i = 0; i < 10; ++i) {
		entity_pool[i].update_dma_payload(dummy_data.dma_ptr(), 0, 0, 16, 16);
		entity_pool[i].is_visible = false;

		// 15 puts it behind the robot (10) but in front of the floor (20)
		entity_pool[i].z_prio = 15;

		// THIS is what was missing. The engine now knows they exist.
		scene.add(&entity_pool[i]);
	}

#ifdef ENABLE_DEBUG_PINS
	init_debug_pins();
#endif

	init_pong_input();
	//the game physics are tied to the refresh rate and this is being designed for 60hz
	init_screen_refresh_timer();
	
	print_ram_usage();
}

volatile bool timer_tick = false;
void loop() {
	while (1) {
		while (!timer_tick) {}
		timer_tick = false;
		pio_output_write_LOW(TC3_DEBUG_PIN); //for testing
		
		//strncpy(debug_buffer, "a\n", 3);
		//debug_printer.queue_write(debug_buffer);
		refresh_logic();
		
		//strncpy(debug_buffer, "b\n", 3);
		//debug_printer.queue_write(debug_buffer);
		scene.render();
		
		//strncpy(debug_buffer, "c\n", 3);
		//debug_printer.queue_write(debug_buffer);
		
		pio_output_write_HIGH(TC3_DEBUG_PIN); //for testing

	}
}

uint32_t frame_counter = 0;
bool water_frame_b = false;

void refresh_logic() {
	frame_counter++;
	volatile uint32_t piod = pio_read_port(PIOD);

	// --- 1. HORIZONTAL INPUT (World Position) ---
	int32_t old_abs_x = absolute_player_x;

	if (!(piod & P1_LEFT))  absolute_player_x -= 3;
	if (!(piod & P1_RIGHT)) absolute_player_x += 3;

	// Clamp player to the physical boundaries of the level
	if (absolute_player_x < 15) absolute_player_x = 15;
	if (absolute_player_x > LEVEL_WIDTH - 15) absolute_player_x = LEVEL_WIDTH - 15;

	// --- 2. CAMERA CALCULATION (The Mario Lock) ---
	int32_t old_cam_x = camera_x;

	camera_x = absolute_player_x - 160;

	if (camera_x < 0) camera_x = 0;
	if (camera_x > LEVEL_WIDTH - 320) camera_x = LEVEL_WIDTH - 320;

	active_window_left = camera_x - OFFSCREEN_BUFFER;
	active_window_right = camera_x + 320 + OFFSCREEN_BUFFER;

	// --- 3. CALCULATE DELTAS (Screen Movement) ---
	int32_t move_cam_x = camera_x - old_cam_x;
	int16_t old_screen_x = old_abs_x - old_cam_x;
	int16_t new_screen_x = absolute_player_x - camera_x;
	int16_t move_player_x = new_screen_x - old_screen_x;

	static int left_edge_idx = 0;
	static int right_edge_idx = NUM_FLOOR_TILES - 1;

	if (move_cam_x != 0) {
		for (int i = 0; i < NUM_FLOOR_TILES; ++i) {
			int16_t tile_dx = -move_cam_x;

			if (floor_tiles[i]->get_box().xmax + tile_dx < 0) {
				tile_dx += (NUM_FLOOR_TILES * 32);
				right_edge_idx = i;
				left_edge_idx = (i + 1) % NUM_FLOOR_TILES;
			}
			else if (floor_tiles[i]->get_box().xmin + tile_dx > 320) {
				tile_dx -= (NUM_FLOOR_TILES * 32);
				left_edge_idx = i;
				right_edge_idx = (i - 1 + NUM_FLOOR_TILES) % NUM_FLOOR_TILES;
			}

			bool is_edge = (i == left_edge_idx || i == right_edge_idx);
			floor_tiles[i]->set_bg_restore(is_edge);
			floor_tiles[i]->move_rel(tile_dx, 0);
		}

		for (int i = 0; i < 10; ++i) {
			if (entity_pool[i].is_visible) {
				entity_pool[i].move_rel(-move_cam_x, 0);
			}
		}
	}

	int32_t new_window = camera_x / WINDOW_WIDTH;
	if (new_window > current_window) {
		snprintf(debug_buffer, 100, "Increase: %d\n", new_window);
		debug_printer.queue_write(debug_buffer);
		while (right_load_index < TOTAL_WORLD_SPRITES && world_sprites[right_load_index].window_number <= new_window + 1) {
			for (int i = 0; i < 10; ++i) {
				if (!entity_pool[i].is_visible) {
					entity_pool[i].move_to(world_sprites[right_load_index].absolute_x - camera_x, world_sprites[right_load_index].y);
					entity_pool[i].is_visible = true;
					break;
				}
			}
			right_load_index++;
		}

		left_load_index = right_load_index;
		for (int i = left_load_index - 1; i >= 0; --i) {
			if (world_sprites[i].window_number >= new_window - 1) {
				left_load_index = i;
			}
			else {
				break;
			}
		}

		current_window = new_window;
	}
	else if (new_window < current_window) {
		snprintf(debug_buffer, 100, "Decrease: %d\n", new_window);
		debug_printer.queue_write(debug_buffer);
		while (left_load_index > 0 && world_sprites[left_load_index - 1].window_number >= new_window - 1) {
			left_load_index--;
			for (int i = 0; i < 10; ++i) {
				if (!entity_pool[i].is_visible) {
					entity_pool[i].move_to(world_sprites[right_load_index].absolute_x - camera_x, world_sprites[right_load_index].y);
					entity_pool[i].is_visible = true;
					break;
				}
			}
		}

		right_load_index = left_load_index;
		for (uint32_t i = right_load_index; i < TOTAL_WORLD_SPRITES; ++i) {
			if (world_sprites[i].window_number <= new_window + 1) {
				right_load_index = i + 1;
			}
			else {
				break;
			}
		}

		current_window = new_window;
	}

	// --- 5. JUMPING LOGIC ---
	if (!(piod & P1_UP) && is_grounded) {
		player_vy = JUMP_FORCE;
		is_grounded = false;
	}

	player_vy += GRAVITY;
	int16_t move_y = player_vy >> 8;

	int16_t current_y = mini_robot.get_box().ymin;
	int16_t next_y = current_y + move_y;

	if (next_y >= FLOOR_Y_LIMIT) {
		next_y = FLOOR_Y_LIMIT;
		player_vy = 0;
		is_grounded = true;
	}
	else {
		is_grounded = false;
	}

	move_y = next_y - current_y;

	// --- 6. APPLY ROBOT MOVEMENT ---
	mini_robot.move_rel(move_player_x, move_y);
}

void DMAC_Handler() {
	volatile uint32_t dummy = REG_DMAC_EBCISR;
#ifdef ENABLE_DEBUG_PINS
	pio_output_write_LOW(DMAC_DEBUG_PIN); //for testing
	scene.service_print_list();
	pio_output_write_HIGH(DMAC_DEBUG_PIN); //for testing
#else
	scene.service_print_list(); 
#endif

}

void TC3_Handler() {
	volatile uint32_t dummy = REG_TC1_SR0;
	REG_TC1_IDR0 |= (1 << 4);

	timer_tick = true;

	REG_TC1_IER0 |= (1 << 4);
}
