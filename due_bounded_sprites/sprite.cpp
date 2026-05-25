#include "sprite.h"

dma_draw_list* sprite_list::dmac_list = nullptr;


bounding_box::bounding_box(sprite& s) {
	bounding_box temp = s._get_bounding_box();
	this->xmax = temp.xmax;
	this->xmin = temp.xmin;
	this->ymax = temp.ymax;
	this->ymin = temp.ymin;
}


sprite::sprite() {
	this->page_data[0] = 0x2B00;
	this->col_data[0] = 0x2A00;
	const uint8_t xfer_size = 3;

	this->lli_list[0] = lli{ reinterpret_cast<uint32_t>(&(this->page_data[0])),
							 sprite_dma_constants::command_rollover_addr,
							 sprite_dma_constants::ctrla_halfword | xfer_size,
							 dma_src_incr_modes::src_inc_dst_inc,
							 reinterpret_cast<uint32_t>(&(this->lli_list[1])) };

	this->lli_list[1] = lli{ reinterpret_cast<uint32_t>(&(this->col_data[0])),
							 sprite_dma_constants::command_rollover_addr,
							 sprite_dma_constants::ctrla_halfword | xfer_size,
							 dma_src_incr_modes::src_inc_dst_inc,
							 reinterpret_cast<uint32_t>(&(this->lli_list[2])) };

	this->lli_list[2] = lli{ 0, // Source address gets filled later by update_dma_payload
							 sprite_dma_constants::command_rollover_addr,
							 0,
							 dma_src_incr_modes::src_inc_dst_inc,
							 0 };

	this->next = nullptr;
	this->prev = nullptr;
}

/*
sprite::sprite(volatile uint16_t* data, int16_t x0, int16_t y0, int16_t width, int16_t height) {

	int16_t x1 = x0 + width - 1;
	int16_t y1 = y0 + height - 1;

	this->_box = bounding_box{ x0, y0, x1, y1 };
	this->_old_box = this->_box;

	///format is start then end, high byte first
	this->page_data[0] = 0x2b00;
	this->page_data[1] = __builtin_bswap16(static_cast<uint16_t>(y0));
	this->page_data[2] = __builtin_bswap16(static_cast<uint16_t>(y1));
	this->col_data[0] = 0x2a00;
	this->col_data[1] = __builtin_bswap16(static_cast<uint16_t>(x0));
	this->col_data[2] = __builtin_bswap16(static_cast<uint16_t>(x1));


	const uint8_t xfer_size = 3;

	this->lli_list[0] = lli{ reinterpret_cast<uint32_t>(&(this->page_data[0])),
								sprite_dma_constants::command_rollover_addr,
								sprite_dma_constants::ctrla_halfword | xfer_size,
								dma_src_incr_modes::src_inc_dst_inc,
								reinterpret_cast<uint32_t>(&(this->lli_list[1])
) };

	this->lli_list[1] = lli{ reinterpret_cast<uint32_t>(&(this->col_data[0])),
								sprite_dma_constants::command_rollover_addr,
								sprite_dma_constants::ctrla_halfword | xfer_size,
								dma_src_incr_modes::src_inc_dst_inc,
								reinterpret_cast<uint32_t>(&(this->lli_list[2])
 ) };

	this->lli_list[2] = lli{ reinterpret_cast<uint32_t>(data),
								sprite_dma_constants::command_rollover_addr,
								sprite_dma_constants::ctrla_halfword | (1 + width * height),
								dma_src_incr_modes::src_inc_dst_inc,
								0
	};

	this->next = nullptr;
	this->prev = nullptr;
}
*/

sprite::sprite(const sprite& o) {
	for (int i = 0; i < 3; ++i) {
		this->page_data[i] = o.page_data[i];
		this->col_data[i] = o.page_data[i];
	}
	this->_box = o._box;
	this->_old_box = o._old_box;

	//the dma lists need to be rebuilt for this instance
	this->lli_list[0] = o.lli_list[0];
	this->lli_list[0].saddr = reinterpret_cast<uint32_t>(&(this->page_data[0]));
	this->lli_list[0].dscr = reinterpret_cast<uint32_t>(&(this->lli_list[1]));

	this->lli_list[1] = o.lli_list[1];
	this->lli_list[1].saddr = reinterpret_cast<uint32_t>(&(this->col_data[0]));
	this->lli_list[1].dscr = reinterpret_cast<uint32_t>(&(this->lli_list[2]));

	this->lli_list[2] = o.lli_list[2];
	this->lli_list[2].dscr = reinterpret_cast<uint32_t>(nullptr);

	this->next = nullptr;
	this->prev = nullptr;
}

bounding_box sprite::_get_bounding_box() {
	return this->_box;
}


void sprite::move_to(int16_t x0, int16_t y0) {
	int16_t dx = x0 - this->_box.xmin;
	int16_t dy = y0 - this->_box.ymin;

	this->move_rel(dx, dy);
}

void sprite::move_to(multi_sprite& o) {
	this->move_to(o().xmin, o().ymin);
}

void sprite::change_incr_mode(uint32_t mode)
{
	this->lli_list[2].ctrlb = mode;
}

void sprite::change_sprite_src(volatile uint8_t* data)
{
	this->lli_list[2].saddr = reinterpret_cast<uint32_t>(data);
}

sprite_list& sprite::operator>>(sprite_list& o)
{
	sprite& h = *(o.head);
	*this << h;
	o.head = this;
	return o;
}

void sprite::move_to(sprite& o) {
	this->move_to(o().xmin, o().ymin);
}

void sprite::move_rel(int16_t x, int16_t y) {
	if (x == 0 && y == 0)
		return;

	if (!this->is_visible) {
		this->dirty_bg_restore = false;
	}
	// if we do not want to refresh, bg is false so dirty is false
	// if we do want to refresh, bg is true so dirty is true
	this->is_dirty = this->dirty_bg_restore;


	int16_t x0 = this->_box.xmin + x;
	int16_t y0 = this->_box.ymin + y;
	int16_t x1 = this->_box.xmax + x;
	int16_t y1 = this->_box.ymax + y;

	//this->_old_box = this->_box;

	this->_box.xmin = x0;
	this->_box.ymin = y0;
	this->_box.xmax = x1;
	this->_box.ymax = y1;


	this->col_data[1] = __builtin_bswap16(static_cast<uint16_t>(x0));
	this->col_data[2] = __builtin_bswap16(static_cast<uint16_t>(x1));
	this->page_data[1] = __builtin_bswap16(static_cast<uint16_t>(y0));
	this->page_data[2] = __builtin_bswap16(static_cast<uint16_t>(y1));

}


void sprite::update_dma_payload(uint32_t src_addr, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t inc_mode) {
	int16_t x1 = x + w - 1;
	int16_t y1 = y + h - 1;

	this->col_data[1] = __builtin_bswap16(static_cast<uint16_t>(x));
	this->col_data[2] = __builtin_bswap16(static_cast<uint16_t>(x1));
	this->page_data[1] = __builtin_bswap16(static_cast<uint16_t>(y));
	this->page_data[2] = __builtin_bswap16(static_cast<uint16_t>(y1));
	const uint8_t xfer_size = 3;

	this->lli_list[0].ctrla = sprite_dma_constants::ctrla_halfword | xfer_size;
	this->lli_list[1].ctrla = sprite_dma_constants::ctrla_halfword | xfer_size;

	this->lli_list[2].saddr = src_addr;
	this->lli_list[2].ctrla = sprite_dma_constants::ctrla_halfword | (1 + w * h);
	this->lli_list[2].ctrlb = inc_mode;
	this->lli_list[2].dscr = 0;

	this->next = nullptr;

}

void sprite_list::set_dmac_list(dma_draw_list* list)
{
	sprite_list::dmac_list = list;
}

sprite_list& sprite_list::operator<<(sprite& o) {
	if (!(this->head)) {
		this->tail = &o;
		this->head = &o;
	}
	else {
		*(this->tail) << o;
		this->tail = &o;
	}
	o.terminate();
	return *this;
}

sprite_list& sprite_list::operator<<(sprite_list& o)
{
	//if this list is empty, just make it the second list
	if (!(this->head)) {
		this->head = o.head;
		this->tail = o.tail;
	}
	else {
		//if the first list is not empty but the second is, just return the first
		if (!(o.head)) {
			return *this;
		}
		//both lists are not empty, combine them
		*(this->tail) << *(o.head);
		this->tail = o.tail;
	}
	this->terminate();
	return *this;
}

sprite_list& sprite_list::operator<<(multi_sprite& o) {
	*this << o.list;
	return *this;
}

inline uint16_t get_scanline() {
	volatile uint8_t* const command = reinterpret_cast<uint8_t*>(0x60000000);
	volatile uint8_t* const data = reinterpret_cast<uint8_t*>(0x61000000);
	*command = 0x45;
	volatile uint8_t dummy = *data;
	uint8_t high_byte = *data;
	uint8_t low_byte = *data;
	return (high_byte << 8) | low_byte;
}

// can only be used if the dmac is not running
// also not working, not sure if the read signal is actually going or if my display supports it
void sprite_list::vsync_draw() {
	while (REG_DMAC_CHSR & 1) {}
	while (true) {
		uint16_t scanline = get_scanline();
		if (scanline < 50 && scanline > 10) {
			break;
		}
	}
	this->draw();
}

void sprite_list::draw() {
	if (!(this->head))
		return;

	this->tail->terminate();
	/*
	if (REG_DMAC_CHSR & 1) {
		this->ready_to_draw = true;
		return;
	}
	*/

	//REG_DMAC_CTRLB0 = (0b10 << 28);
	//REG_DMAC_DSCR0 = reinterpret_cast<uint32_t>(  & (this->head->lli_list[0]) );
	//REG_DMAC_CHER |= 1;
	sprite_list::dmac_list->add_to_draw_list(this);
}

bool operator!=(sprite_list::sprite_list_iterator& lhs, sprite_list::sprite_list_iterator& rhs) {
	return lhs.cur != rhs.cur;
}


sprite_list::sprite_list(const sprite_list& o) {
	this->head = o.head;
	this->tail = o.tail;
}

sprite_list& sprite_list::operator=(const sprite_list& o) {
	this->head = o.head;
	this->tail = o.tail;
	return *this;
}

sprite& sprite_list::front() {
	return *(this->head);
}

sprite& sprite_list::back() {
	return *(this->tail);
}

sprite& sprite_list::sprite_list_iterator::begin(sprite_list& l) {
	return *(l.head);
}
sprite& sprite_list::sprite_list_iterator::end(sprite_list& l) {
	return *(l.tail);
}
sprite_list::sprite_list_iterator& sprite_list::sprite_list_iterator::operator++() {
	this->cur = this->cur->next;
	return *this;
}
bool sprite_list::sprite_list_iterator::operator!=(const sprite_list_iterator& other) const {
	return this->cur != other.cur;
}
sprite_list::sprite_list_iterator sprite_list::begin() {
	return sprite_list::sprite_list_iterator(this->head);
}
sprite_list::sprite_list_iterator sprite_list::end() {
	return sprite_list::sprite_list_iterator(this->tail->next);
}
sprite& sprite_list::sprite_list_iterator::operator*() { return *cur; }

void sprite_list::move_rel(int16_t x, int16_t y) {
	sprite* cur = this->head;
	/*
		if this sprite is part of a larger list but not the end, its tail will be connected to another sprite
		it used to check if cur == nullptr, but multi_sprite::move_rel would move everything after it in the list too
		the alternative is to terminate the sprite_list, but that means we have to rebuild the list every time
	*/
	while (true) {
		cur->move_rel(x, y);
		if (cur == this->tail)
			break;
		cur = cur->next;
	}
}

void sprite_list::move_to(int16_t x, int16_t y) {
	sprite* cur = this->head;
	while (cur) {
		int16_t dx = x - (*cur)().xmin;
		int16_t dy = y - (*cur)().ymin;
		cur->move_rel(dx, dy);
		cur = cur->next;
	}
}

void sprite_list::clear() {
		if (!this->head) return;

		sprite* before = this->head->prev;
		sprite* after = this->tail->next;

		//fix up wapper list
		if (before) {
			before->next = after;

			if (after)
				before->lli_list[2].dscr = reinterpret_cast<uint32_t>(&(after->lli_list[0]));
			else
				before->lli_list[2].dscr = reinterpret_cast<uint32_t>(nullptr);
		}
		if (after) {
			after->prev = before;
		}

		//this wraps a wrapper doulby linked list - clear the inner wrapper
		this->head->prev = nullptr;
		this->tail->next = nullptr;
		this->terminate(); //terminate outer lsit

		//clear the "this" wrapper
		this->head = nullptr;
		this->tail = nullptr;
}

void sprite_list::terminate() {
	this->tail->terminate();
}


void dma_draw_list::service_print_list() {
	sprite_list* next_to_draw = this->queue_head;

	if (next_to_draw) {
		this->queue_head = next_to_draw->next;
		if (this->queue_head == nullptr) {
			this->queue_tail = nullptr;
		}

		REG_DMAC_DSCR0 = reinterpret_cast<uint32_t>(&(next_to_draw->head->lli_list[0]));
		REG_DMAC_CHER |= 1;
	}
}

void dma_draw_list::add_to_draw_list(sprite_list* li) {
	li->next = nullptr;

	NVIC_DisableIRQ(DMAC_IRQn);

	if (this->queue_tail == nullptr) {
		this->queue_head = li;
		this->queue_tail = li;
	}
	else {
		this->queue_tail->next = li;
		this->queue_tail = li;
	}

	if (!(REG_DMAC_CHSR & 1)) {
		// interrupt clear to prevent double firing
		volatile uint32_t dummy = REG_DMAC_EBCISR;
		NVIC_ClearPendingIRQ(DMAC_IRQn);
		service_print_list();
	}

	NVIC_EnableIRQ(DMAC_IRQn);
}

