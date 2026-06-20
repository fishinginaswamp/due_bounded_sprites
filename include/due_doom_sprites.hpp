#pragma once

#ifndef DUE_DOOM_SPRITES
#define DUE_DOOM_SPRITES

#include "sprite.h"
#include "scene_manager.hpp"
#include <SAM3XDUE.H>

inline int32_t z_buffer[320];

// Shared pixel data for the robot
inline sprite_data<1, 30> col_leg_data; // 15px torso + 15px leg
inline sprite_data<1, 15> col_mid_data; // 15px torso only

class RobotEnemy : public Renderable {
private:
    sprite columns[20];
    sprite_list active_list;
    bounding_box _box;
    bounding_box _old_box;

public:
    int32_t dist; // Distance to player

    RobotEnemy(int16_t start_x, int16_t start_y) {
        // Width is 20 (start_x to start_x + 19), Max Height is 30
        this->_box = bounding_box{ start_x, start_y, static_cast<int16_t>(start_x + 19), static_cast<int16_t>(start_y + 29) };
        this->_old_box = this->_box;
        
        for (int i = 0; i < 20; i++) {
            // Columns 0-5 and 14-19 use the 30px leg data
            if (i < 6 || i >= 14) {
                columns[i].update_dma_payload(col_leg_data.dma_ptr(), start_x + i, start_y, 1, 30);
            } 
            // Columns 6-13 use the 15px torso data
            else {
                columns[i].update_dma_payload(col_mid_data.dma_ptr(), start_x + i, start_y, 1, 15);
            }
        }
    }

    void build_render_list(const int32_t* z_buffer) {
        this->active_list.clear();
        this->is_visible = false;
        
        int16_t current_x = this->_box.xmin;

        // Ensure at least part of the sprite is on screen
        if (this->_box.xmax < 0 || current_x >= 320) return;

        for (int i = 0; i < 20; i++) {
            int16_t screen_x = current_x + i;
            
            // X-Clipping and Z-Clipping
            if (screen_x >= 0 && screen_x < 320) {
                // Only link this column to the DMA list if it is closer than the wall at this screen_x
                if (this->dist < z_buffer[screen_x]) {
                    this->active_list << columns[i];
                    this->is_visible = true;
                }
            }
        }
    }

    sprite_list& get_render_list() override { return active_list; }
    const bounding_box& get_box() const override { return _box; }
    const bounding_box& get_old_box() const override { return _old_box; }
    void update_old_box() override { _old_box = _box; is_dirty = false; }
    
    void move_to(int16_t x, int16_t y) {
        int16_t dx = x - _box.xmin;
        int16_t dy = y - _box.ymin;
        _box.move_to(x, y);
        for(int i = 0; i < 20; i++) {
            columns[i].move_rel(dx, dy);
        }
        is_dirty = true;
    }
};

// Instantiate the global enemy so main.cpp can use it
inline RobotEnemy ghost_enemy{ 160, 100 }; // Start at center screen for testing

template <uint32_t MAX_SPRITES, uint32_t MAX_BG_PATCHES>
inline void init_sprites(scene_manager<MAX_SPRITES, MAX_BG_PATCHES>& scene){
    // Define colors
    uint16_t blue_color = __builtin_bswap16(0x001F);
    uint16_t green_color = __builtin_bswap16(0x07E0);

    // Populate the shared column arrays
    for (int i = 0; i < 15; ++i) {
        col_leg_data[i] = blue_color;           // Top 15 pixels: Blue Torso
        col_leg_data[i + 15] = green_color;     // Bottom 15 pixels: Green Leg
        col_mid_data[i] = blue_color;           // Middle columns are just 15px Blue Torso
    }

    ghost_enemy.z_prio = 10;
    scene.add(&ghost_enemy);
}

#endif