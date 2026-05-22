#pragma once
#include "dma_ll.h"
#include <initializer_list>

using std::initializer_list;

class scene_manager;
class sprite_list;
class sprite;
class multi_sprite;
class bounding_box;
class dma_draw_list;
struct sprite_list_iterator;

class Renderable {
public:
	uint16_t z_prio = 0; // lower is higher priority
	bool is_dirty = true;
	bool dirty_bg_restore = true;
	bool is_visible = true;

	virtual const bounding_box& get_box() const = 0;
	virtual const bounding_box& get_old_box() const = 0;
	virtual void update_old_box() = 0;
	virtual sprite_list& get_render_list() = 0;

	void set_bg_restore(bool enabled) { 
		this->dirty_bg_restore = enabled; 
	}

	virtual ~Renderable() = default;
};


/*
	ANYTHING that is part of a dma list needs to be volatile/forced into RAM before the transfer
	the dmac controller is not connected to internal flash (datasheet pdf page 324)
	so, to future proof, i am forcing everything to be aligned to 4-byte boundaries (padding if necessary)
	this way i can write my own memcpy to copy objects from flash to ram manually, 4 bytes at a time
*/

namespace sprite_dma_constants {
	inline const uint32_t command_addr = 0x60000000;
	inline const uint32_t command_rollover_addr = 0x60FFFFFE;
	inline const uint32_t data_addr = 0x61000000;
	//ctrla just needs the size in the lower 16 bits
	inline volatile const uint32_t col_addr_set = 0x2A;
	inline volatile const uint32_t page_addr_set = 0x2B;
	inline volatile const uint32_t mem_write = 0x2C;
	inline volatile const uint32_t ctrla_halfword = (0b01 << 24) | (0b01 << 28);
}

namespace dma_src_incr_modes {
	//const uint32_t inc = (0b10 << 28);
	//const uint32_t dec = (0b10 << 28) | (0b01 << 24);
	const uint32_t src_fixed_dst_fixed = (0b10 << 28) | (0b10 << 24);
	const uint32_t src_inc_dst_inc = 0;
}

struct bounding_box {

	bounding_box() {}
	int16_t xmin = 0, xmax = 0, ymin = 0, ymax = 0;

	bounding_box(int16_t xmin, int16_t ymin, int16_t xmax, int16_t ymax) :
		xmin(xmin), xmax(xmax), ymin(ymin), ymax(ymax) {
	}

	bounding_box(sprite& s);
	bounding_box(const bounding_box& o) :
		xmin(o.xmin), xmax(o.xmax), ymin(o.ymin), ymax(o.ymax) {
	}

	void operator=(const bounding_box& o) {
		this->xmin = o.xmin;
		this->xmax = o.xmax;
		this->ymin = o.ymin;
		this->ymax = o.ymax;
	}

	//this is maybe not necessary but i would like the syntax to be consistent
	const bounding_box& operator()() const {
		return *this;
	}

	inline bool check_collision(const bounding_box& o, int16_t x_offset = 0, int16_t y_offset = 0) const {
		return (this->xmin + x_offset <= o.xmax) && (this->xmax + x_offset >= o.xmin) &&
			(this->ymin + y_offset <= o.ymax) && (this->ymax + y_offset >= o.ymin);
	}

	inline bool check_if_move_is_within(const bounding_box& surrounding, int16_t x_offset = 0, int16_t y_offset = 0) const {
		return (this->xmin + x_offset >= surrounding.xmin) && (this->xmax + x_offset <= surrounding.xmax) &&
			(this->ymin + y_offset >= surrounding.ymin) && (this->ymax + y_offset <= surrounding.ymax);
	}

	inline bool check_x_collision(const bounding_box& o, int16_t x_offset = 0) const {
		return (this->xmin <= o.xmax + x_offset) && (this->xmax >= o.xmin + x_offset);
	}

	inline bool check_y_collision(const bounding_box& o, int16_t y_offset = 0) const {
		return (this->ymin <= o.ymax + y_offset) && (this->ymax >= o.ymin + y_offset);
	}

	inline bool check_if_x_move_is_within(const bounding_box& o, int16_t x_offset = 0) const {
		return (this->xmin + x_offset >= o.xmin) && (this->xmax + x_offset <= o.xmax);
	}
	inline bool check_if_y_move_is_within(const bounding_box& o, int16_t y_offset = 0) const{
		return (this->ymin + y_offset >= o.ymin) && (this->ymax + y_offset <= o.ymax);
	}

	void move_rel(int16_t x, int16_t y) {
		this->xmin += x;
		this->xmax += x;
		this->ymin += y;
		this->ymax += y;
	}

	void move_to(int16_t x, int16_t y) {
		int16_t x_diff = x - this->xmin;
		int16_t y_diff = y - this->ymin;
		this->xmin = x;
		this->ymin = y;
		this->xmax += x_diff;
		this->ymax += y_diff;
	}
} __attribute__((aligned(4)));


template<uint32_t WIDTH = 16, uint32_t HEIGHT = 16>
class sprite_data {
private:
	uint16_t buf[1 + (WIDTH * HEIGHT)];
	uint16_t* buf_offset;
public:

	sprite_data() {
#ifdef SAM_16_BIT_LCD_SMC
		this->buf[0] = 0x002C; // Puts 0x2C on the lower 8 pins!
#else
		this->buf[0] = 0x2C00;
#endif
		this->buf_offset = buf + 1;
	}

	uint32_t dma_ptr() {
		return reinterpret_cast<uint32_t>(&(this->buf[0]));
	}

	constexpr uint32_t pixel_count() const {
		return WIDTH * HEIGHT;
	}

	uint16_t& operator[](uint32_t i) {
		return buf_offset[i];
	}

	const uint16_t& operator[](uint32_t i) const {
		return buf_offset[i];
	}

	uint16_t* begin() {
		return &buf_offset[0]; // Start at the first pixel
	}

	uint16_t* end() {
		return &buf_offset[pixel_count()]; // The memory address just past the last pixel
	}
};


class sprite_list {
	friend class multi_sprite; //for access to head and tail
	friend class dma_draw_list; //so dmac can update the register with the *head sprite
	friend class sprite; //for sprite::operator>>
	friend struct sprite_list_iterator;
	friend class scene_manager;

	sprite* volatile head = nullptr;
	sprite* volatile tail = nullptr;
	sprite_list* volatile next = nullptr; //ONLY TO BE USED BY THE DMAC DRAW LIST FUNC

	static dma_draw_list* dmac_list;
public:
	sprite_list() = default;

	sprite_list(const sprite_list& o);
	sprite_list& operator=(const sprite_list& o);

	sprite& front();

	sprite& back();

	struct sprite_list_iterator {
		sprite* cur;
		sprite_list_iterator(sprite* s) : cur(s) {}
		sprite& operator*();
		sprite& begin(sprite_list& l);
		sprite& end(sprite_list& l);
		sprite_list_iterator& operator++();
		bool operator!=(const sprite_list_iterator& other) const;
	};

	sprite_list_iterator begin();

	sprite_list_iterator end();

	//the problem with initializer lists is the objects are const only

	static void set_dmac_list(dma_draw_list* list);

	//returns the calling sprite_list
	sprite_list& operator<<(sprite& o);

	/*
		returns the calling sprite_list, does not mangle the list being passed into it
		this means the individual lists can be moved around without affecting any other part of the list
		which is pretty critical for multi_sprites being chained with other multi_sprites
	*/
	sprite_list& operator<<(sprite_list& o);
	sprite_list& operator<<(multi_sprite& o);

	void draw();
	void vsync_draw();
	void move_rel(int16_t x, int16_t y);
	void move_to(int16_t x, int16_t y);
	void clear();
	void terminate();

}__attribute__((aligned(4)));

class sprite : public Renderable {
	friend class sprite_list; //for the sprite_list operator<< (needs the next pointer)
	friend class multi_sprite; //for the multi_sprite operator<< (needs the next pointer)
	friend class dma_draw_list; //so the dma list can update the dmac register with the start of the list
	friend struct sprite_list_iterator;
	friend class scene_manager;

	/*
		putting width/height and x/y first saved space when compiling with O3
		that's why there's multiple visibility specifiers or whatever they're called
	*/
	bounding_box _box, _old_box;

#ifdef SAM_16_BIT_LCD_SMC
	volatile uint16_t col_data[5];
	volatile uint16_t page_data[5];
#else
	volatile uint16_t col_data[3];
	volatile uint16_t page_data[3];
#endif

	sprite* next = nullptr;
	sprite* prev = nullptr; //only intended to be used in sprite_list::pop_back (which is not there??)
	volatile lli lli_list[3];

	sprite(volatile uint16_t* data, int16_t x0, int16_t y0, int16_t width, int16_t height);

	sprite_list wrapper_list;

public:
	sprite();

	bounding_box _get_bounding_box(); // making a sprite with another requires a copy of their bounding box
	inline const bounding_box& get_box() const override {
		return this->_box;
	}

	inline const bounding_box& get_old_box() const override {
		return this->_old_box;
	}

	void update_old_box() override {
		this->_old_box = this->_box;
		this->is_dirty = false;
	}

	sprite_list& get_render_list() override {
		return this->wrapper_list;
	}
	
	inline void mark_dirty(bool dirty = true) {
		this->is_dirty = dirty;
	}

	sprite operator=(const sprite& o) = delete;

	const bounding_box& operator()() {
		return this->_box;
	}

	const bounding_box& operator()() const {
		return this->_box;
	}
	
	template<uint32_t W, uint32_t H>
	sprite(sprite_data<W, H>& s_data, int16_t x0, int16_t y0)
		: sprite(reinterpret_cast<volatile uint16_t*>(s_data.dma_ptr()), x0, y0, W, H) {}
	
	//this doesn't seem to work
	sprite(const sprite&);

	void move_rel(int16_t x, int16_t y);
	void move_to(sprite& o);
	void move_to(int16_t x0, int16_t y0);
	void move_to(multi_sprite& o);
	void change_incr_mode(uint32_t mode);
	void change_sprite_src(volatile uint8_t* data);
	void update_dma_payload(uint32_t src_addr, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t inc_mode = dma_src_incr_modes::src_inc_dst_inc);
	inline void update_dma_payload(uint16_t* data, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t inc_mode = dma_src_incr_modes::src_inc_dst_inc) {
		this->update_dma_payload(reinterpret_cast<uint32_t>(data), x, y, w, h, inc_mode);
	}


	sprite& operator<<(sprite& o) {
		if (this->next) {
			o.next = this->next;
			this->next->prev = &o;
			o.lli_list[2].dscr = (uint32_t) & (o.next->lli_list[0]);
		}
		this->next = &o;
		o.prev = this;
		this->lli_list[2].dscr = (uint32_t) & (o.lli_list[0]);

		return o;
	}

	void terminate() {
		this->lli_list[2].dscr = 0;
		this->next = nullptr;
	}

	//prepend to the list (damn this is kinda dirty
	//should only be used on a non-empty list
	sprite_list& operator>>(sprite_list& o);

}__attribute__((aligned(4)));


class multi_sprite : public Renderable {
	friend class sprite_list; //for the sprite_list operator<< (needs the sprite_list& list head and tail pointers)
	friend class scene_manager;

	bounding_box _box, _old_box;
	sprite_list& list;

	multi_sprite() = default;
	multi_sprite(const multi_sprite&) = delete;
	multi_sprite& operator=(const multi_sprite&) = delete;

public:
	int16_t width, height;
	multi_sprite(sprite_list& list, const bounding_box& box) 
		: list(list), _box(box) {
		this->height = this->_box.ymax - this->_box.ymin + 1;
		this->width = this->_box.xmax - this->_box.xmin + 1;
		this->_old_box = this->_box;
	}
	multi_sprite(sprite_list& list, int16_t width, int16_t height) 
		: list(list), width(width), height(height) {
		this->_box = bounding_box{ 0, 0, width - 1, height - 1 };
		this->_old_box = this->_box;
	}

	const bounding_box& operator()() {
		return this->_box;
	}

	inline const bounding_box& get_box() const override {
		return this->_box;
	}

	inline const bounding_box& get_old_box() const override {
		return this->_old_box;
	}

	void update_old_box() override {
		this->_old_box = this->_box;
		this->is_dirty = false;
	}

	sprite_list& get_render_list() override {
		return this->list;
	}

	void draw() {
		this->list.draw();
	}

	void move_rel(int16_t x, int16_t y) {
		if (x == 0 && y == 0)
			return;

		this->is_dirty = true;
		sprite* cur = this->list.head;
		/*
			if this sprite is part of a larger list but not the end, its tail will be connected to another sprite
			it used to check if cur == nullptr, but multi_sprite::move_rel would move everything after it in the list too
			the alternative is to terminate the multi_sprite, but that means we have to rebuild the list every time
		*/
		while (true) {
			cur->move_rel(x, y);
			if (cur == this->list.tail)
				break;
			cur = cur->next;
		}
		this->_old_box = this->_box;
		this->_box.move_rel(x, y);
	}

	void move_to(int16_t x, int16_t y) {
		int16_t dx = x - (*this)().xmin;
		int16_t dy = y - (*this)().ymin;
		this->move_rel(dx, dy);
	}

	void terminate() {
		this->list.terminate();
	}

}__attribute__((aligned(4)));



//thanks AI for this cool trick
//it doesn't render anything, it just forces a bg refresh over a given area
class BackgroundAnimator : public Renderable {
private:
	bounding_box _box;
	sprite_list empty_list; // stays permanently empty

public:
	BackgroundAnimator(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
		_box = bounding_box{ x0, y0, x1, y1 };
		this->is_dirty = false;
		this->dirty_bg_restore = true; // We specifically WANT it to trigger a bg patch
	}

	const bounding_box& get_box() const override { return _box; }
	const bounding_box& get_old_box() const override { return _box; } // Doesn't move

	void update_old_box() override {
		this->is_dirty = false; // Auto-clears its flag after the scene_manager processes it
	}

	inline void mark_dirty() {
		this->is_dirty = true;
	}

	sprite_list& get_render_list() override { return empty_list; }
};


class dma_draw_list {
	sprite_list* volatile queue_head = nullptr;
	sprite_list* volatile queue_tail = nullptr;

public:
	static const uint32_t CBTC0 = (1 << 8); //chained buffer transfer complete

	dma_draw_list() = default;

	//adding to the draw list inserts the new item after the currently drawing one
	//PRECONDITION: both lists are not empty (this is not checked in this function)
	void add_to_draw_list(sprite_list* li);

	void service_print_list();

}__attribute__((aligned(4)));


