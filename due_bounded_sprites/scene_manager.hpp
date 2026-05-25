#pragma	once
#include "sprite.h"

#ifndef SCENE_MANAGER
#define SCENE_MANAGER


class IBackground {
public:
	virtual void patch(const bounding_box& box,
		sprite_list& patch_list,
		sprite* pool,
		uint32_t& pool_i,
		uint32_t max_patches
	) = 0;

	virtual ~IBackground() = default;
};


template <uint16_t TILE_W = 16, uint16_t TILE_H = 16, uint8_t MAP_COLS = 20, uint8_t MAP_ROWS = 15, uint8_t PALETTE_SIZE = 16>
class tile_map : public IBackground {
private:
	// A palette of pointers to your raw Flash memory pixel arrays
	sprite_data<TILE_W, TILE_H>* tile_palette[PALETTE_SIZE];

	// The actual screen layout (e.g., 0 = grass, 1 = dirt, 2 = sky, etc)
	uint8_t grid[MAP_ROWS][MAP_COLS];

public:
	tile_map() {
		for (int r = 0; r < MAP_ROWS; ++r)
			for (int c = 0; c < MAP_COLS; ++c)
				grid[r][c] = 0;

		for (int i = 0; i < PALETTE_SIZE; ++i)
			tile_palette[i] = nullptr;
	}

	// Link a tile ID (0-15) to a specific pixel array in Flash
	void set_palette_tile(uint8_t id, sprite_data<TILE_W, TILE_H>& pixel_data) {
		if (id < PALETTE_SIZE) tile_palette[id] = &pixel_data;
	}

	// Set a specific coordinate in the level to a tile ID
	void set_map_tile(uint8_t col, uint8_t row, uint8_t tile_id) {
		if (col < MAP_COLS && row < MAP_ROWS) grid[row][col] = tile_id;
	}

	void patch(const bounding_box& box,
		sprite_list& patch_list,
		sprite* pool,
		uint32_t& pool_i,
		uint32_t max_patches) 
		override {

		//spacial hashing to find the overlapping tiles to replace
		int16_t start_col = box.xmin / TILE_W;
		int16_t end_col = box.xmax / TILE_W;
		int16_t start_row = box.ymin / TILE_H;
		int16_t end_row = box.ymax / TILE_H;

		if (start_col < 0) start_col = 0;
		if (end_col >= MAP_COLS) end_col = MAP_COLS - 1;
		if (start_row < 0) start_row = 0;
		if (end_row >= MAP_ROWS) end_row = MAP_ROWS - 1;

		for (int16_t r = start_row; r <= end_row; ++r) {
			for (int16_t c = start_col; c <= end_col; ++c) {
				uint8_t tile_id = grid[r][c];
				if (tile_id < PALETTE_SIZE) {
					sprite_data<TILE_W, TILE_H>* pixels = tile_palette[tile_id];
					if (pixels != nullptr) {
						if (pool_i >= max_patches) return;

						sprite& s = pool[pool_i++];

						s.update_dma_payload(
							pixels->dma_ptr(),
							c * TILE_W,
							r * TILE_H,
							TILE_W,
							TILE_H
						);

						patch_list << s;
					}
				}
			}
		}
	}

private:

};


template <uint32_t MAX_SPRITES = 100, uint32_t MAX_BG_PATCHES = 300>
class scene_manager {
private:

	//we can selectively patch background tiles
	//using a list so we can add to it asynchronously
	sprite patch_pool[MAX_BG_PATCHES];
	sprite_list active_patches;
	uint32_t patch_count = 0;

	Renderable* active_sprites[MAX_SPRITES];
	uint8_t sprite_count = 0;

	IBackground* current_bg = nullptr;

	dma_draw_list draw_list;

public:
	scene_manager() {
		sprite_list::set_dmac_list(&(this->draw_list));
	}

	void service_print_list() {
		this->draw_list.service_print_list();
	}	

	void set_background(IBackground& bg) { current_bg = &bg; }

	void draw_full_background();

	bool add(Renderable* s);
	void remove(Renderable* s);
	void clear_all();

	void force_clear();

	void render();

};

#include "scene_manager.inl"



#endif
