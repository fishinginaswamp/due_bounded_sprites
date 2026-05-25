
extern dma_draw_list dmac_list;
extern void lcd_clear();

template <uint32_t MAX_SPRITES, uint32_t MAX_BG_PATCHES>
bool scene_manager<MAX_SPRITES, MAX_BG_PATCHES>::add(Renderable* s) {
	if (sprite_count < MAX_SPRITES) {
		active_sprites[sprite_count++] = s;
		return true;
	}
	return false;
}

// render() runs an insertion sort, so we don't need to preserve order when removing
// saves a bunch of copies
template <uint32_t MAX_SPRITES, uint32_t MAX_BG_PATCHES>
void scene_manager<MAX_SPRITES, MAX_BG_PATCHES>::remove(Renderable* s) {
	for (uint8_t i = 0; i < sprite_count; ++i) {
		if (active_sprites[i] == s) {
			active_sprites[i] = active_sprites[sprite_count - 1];
			sprite_count--;
			break;
		}
	}
}

template <uint32_t MAX_SPRITES, uint32_t MAX_BG_PATCHES>
void scene_manager<MAX_SPRITES, MAX_BG_PATCHES>::clear_all() {
	sprite_count = 0;
}

template <uint32_t MAX_SPRITES, uint32_t MAX_BG_PATCHES>
void scene_manager<MAX_SPRITES, MAX_BG_PATCHES>::render() {
	if (sprite_count == 0) return;

	while (REG_DMAC_CHSR & 1) {}

	this->active_patches.clear(); //new code in this causes crash
	this->patch_count = 0;

	if (this->current_bg) {
		for (uint8_t i = 0; i < sprite_count; ++i) {
			// Check if it moved AND if it allows background patching
			if(active_sprites[i]->is_visible && active_sprites[i]->is_dirty && active_sprites[i]->dirty_bg_restore) {
				current_bg->patch(
					active_sprites[i]->get_old_box(),
					this->active_patches,
					this->patch_pool,
					this->patch_count,
					MAX_BG_PATCHES
				);
			}
		}
	}

	// insertion sort by z_prio
	for (uint8_t i = 1; i < sprite_count; ++i) {
		Renderable* key = active_sprites[i];
		int16_t j = i - 1;

		while (j >= 0 && active_sprites[j]->z_prio > key->z_prio) {
			active_sprites[j + 1] = active_sprites[j];
			j--;
		}
		active_sprites[j + 1] = key;
	}

	static sprite_list frame_list;
	frame_list.clear();

	for (uint8_t i = 0; i < sprite_count; ++i) {
		if (!active_sprites[i]->is_visible) continue;

		sprite_list& obj_list = active_sprites[i]->get_render_list();
		if (!obj_list.head) continue; //this should never happen

		if (!frame_list.head) {
			frame_list.head = obj_list.head;
			frame_list.tail = obj_list.tail;
		}
		else {
			frame_list.tail->lli_list[2].dscr = (uint32_t) & (obj_list.head->lli_list[0]);
			frame_list.tail = obj_list.tail;
		}
	}

	if(active_patches.head){
		active_patches << frame_list;
		draw_list.add_to_draw_list(&active_patches);
	}
	else if (frame_list.head) {
		frame_list.tail->terminate();
		draw_list.add_to_draw_list(&frame_list);
	}

	for (uint8_t i = 0; i < sprite_count; ++i) {
		active_sprites[i]->update_old_box();
	}
}


template <uint32_t MAX_SPRITES, uint32_t MAX_BG_PATCHES>
void scene_manager<MAX_SPRITES, MAX_BG_PATCHES>::force_clear() {
		while (REG_DMAC_CHSR & 1) {}
		NVIC_DisableIRQ(DMAC_IRQn);

		volatile uint16_t* cmd = (volatile uint16_t*)sprite_dma_constants::command_addr;
		volatile uint16_t* dat = (volatile uint16_t*)sprite_dma_constants::data_addr;

#ifdef SAM_16_BIT_LCD_SMC
		*cmd = 0x002A;
		*dat = 0; 
		*dat = 0;
		*dat = 319 >> 8; 
		*dat = 319 & 0xFF;

		*cmd = 0x002B;
		*dat = 0; 
		*dat = 0;
		*dat = 239 >> 8; 
		*dat = 239 & 0xFF;

		*cmd = 0x002C;
#else
		*cmd = 0x2A00; // column address set
		*dat = 0;      // x0 = 0
		*dat = __builtin_bswap16(319); // x1 = 319

		*cmd = 0x2B00; // page address set
		*dat = 0;      // y0 = 0
		*dat = __builtin_bswap16(239); // y1 = 239

		*cmd = 0x2C00; // memory write
#endif

		static uint16_t black_pixel = 0x0000;
		const uint32_t TOTAL_PIXELS = 240 * 320;
		const uint32_t CHUNK_SIZE = 4000;
		uint32_t pixels_sent = 0;

		uint32_t ctrlb_fixed = (0b10 << 24) | (0b10 << 28);

		REG_DMAC_SADDR0 = (uint32_t)&black_pixel;
		REG_DMAC_DADDR0 = (uint32_t)sprite_dma_constants::data_addr;
		REG_DMAC_DSCR0 = 0;

		while (pixels_sent < TOTAL_PIXELS) {
			uint32_t send_count = TOTAL_PIXELS - pixels_sent;
			if (send_count > CHUNK_SIZE) send_count = CHUNK_SIZE;

			REG_DMAC_CTRLA0 = sprite_dma_constants::ctrla_halfword | send_count;
			REG_DMAC_CTRLB0 = ctrlb_fixed;

			REG_DMAC_CHER = 1;
			while (REG_DMAC_CHSR & 1) {} 

			pixels_sent += send_count;
		}

		volatile uint32_t dummy = REG_DMAC_EBCISR;
		NVIC_ClearPendingIRQ(DMAC_IRQn);

		NVIC_EnableIRQ(DMAC_IRQn);

}


template <uint32_t MAX_SPRITES, uint32_t MAX_BG_PATCHES>
void scene_manager<MAX_SPRITES, MAX_BG_PATCHES>::draw_full_background() {
	if (!current_bg) return;

	while (REG_DMAC_CHSR & 1) {}
	NVIC_DisableIRQ(DMAC_IRQn);

	this->active_patches.clear();
	this->patch_count = 0;

	bounding_box full_screen{ 0, 0, 319, 239 };
	current_bg->patch(full_screen,
		this->active_patches,
		this->patch_pool,
		this->patch_count
	);

	if (active_patches.head) {
		active_patches.terminate();
		draw_list.add_to_draw_list(&active_patches);
	}

	volatile uint32_t dummy = REG_DMAC_EBCISR;
	NVIC_ClearPendingIRQ(DMAC_IRQn);
	NVIC_EnableIRQ(DMAC_IRQn);
}

