#pragma once

#include <SAM3XDUE.H>
#include "raycast.hpp"
#include "player.hpp"
#include "sprite.h"
#include "textures.hpp"

#ifndef DUE_DOOM_RENDERER
#define DUE_DOOM_RENDERER

inline void render_frame(Player& player, sprite& s0, sprite& s1, sprite_data<1, 240>& d0, sprite_data<1, 240>& d1) {
    extern volatile uint32_t columns_drawn;
    
    // Skip if the DMAC is still rendering the previous frame
    if (columns_drawn < 320) return;
    columns_drawn = 0;

    for (int x = 0; x < 320; x++) {
        // PING-PONG LOCK
        while ((x - columns_drawn) >= 2) {}

        bool use_buf_0 = (x % 2 == 0);
        sprite_data<1, 240>& active_data = use_buf_0 ? d0 : d1;
        sprite& active_slice = use_buf_0 ? s0 : s1;

        // --- MATH ---
        int32_t angle_offset = (x * player.fov) / 320 - (player.fov / 2);
        uint16_t ray_angle = (player.angle + angle_offset) & 4095;
        RayHit hit = player.cast_ray(ray_angle);

        int32_t dist = hit.perp_dist;
        if (dist < 256) dist = 256;

        int32_t line_height = (240 * 65536) / dist;
        int16_t draw_start = 120 - (line_height / 2);
        if (draw_start < 0) draw_start = 0;
        int16_t draw_end = 120 + (line_height / 2);
        if (draw_end >= 240) draw_end = 239;

        uint16_t wall_color = (hit.wall_type == 2) ? 0xF800 : ((hit.hit_side == 1) ? 0x001F : 0x07E0);
        uint16_t ceiling_color = 0x0000;
        uint16_t floor_color = 0xFFFF;

        /*
        // ==========================================
        // --- CHECKERBOARD
        // ==========================================
        
        // 1. Calculate X Texture Coordinate (0 to 63)
        // hit.wall_x is a Q16.16 fraction (0 to 65535). 
        // Multiply by 64 and shift down to get the exact column index.
        uint32_t tex_x = (hit.wall_x * 64) >> 16;

        // 2. Calculate vertical texture stepping speed
        uint32_t tex_step = (64 << 16) / line_height;
        
        // 3. Calculate starting texture Y position
        // This math perfectly aligns the texture even if the wall is taller than the screen 
        // and draw_start is clamped to 0.
        uint32_t tex_pos = (draw_start - 120 + (line_height / 2)) * tex_step;


        // --- DRAW CEILING ---
        for(int y = 0; y < draw_start; y++) active_data[y] = ceiling_color;
        
        // --- DRAW WALL ---
        for(int y = draw_start; y < draw_end; y++) {
            
            // Extract the integer part of the texture Y coordinate (0 to 63)
            uint32_t tex_y = (tex_pos >> 16) & 63;
            tex_pos += tex_step;
            
            // THE CHECKERBOARD TRICK:
            // Shift right by 3 to create 8x8 pixel blocks.
            // Bitwise XOR (^) perfectly alternates the pattern.
            bool is_dark = ((tex_x >> 3) & 1) ^ ((tex_y >> 3) & 1);
            
            // Assign Black if dark, otherwise the designated wall color
            active_data[y] = is_dark ? 0x0000 : wall_color;
        }
        
        // --- DRAW FLOOR ---
        for(int y = draw_end; y < 240; y++) active_data[y] = floor_color;
        */

       // ==========================================
        // --- OPTIMIZED TEXTURE MAPPING ------------
        // ==========================================
        
        uint32_t tex_x = ((hit.wall_x * 16) >> 16) & 15;
        uint32_t tex_step = (16 << 16) / line_height;
        uint32_t tex_pos = (draw_start - 120 + (line_height / 2)) * tex_step;

        uint8_t tex_idx = (hit.wall_type > 2) ? 1 : hit.wall_type;
        
        // OPTIMIZATION 1: Point directly to the correct X-column of the texture 
        // so we never have to add tex_x inside the loop.
        const uint16_t* tex_column = wall_textures[tex_idx] + tex_x;

        // OPTIMIZATION 2: Grab the raw RAM address of the active buffer
        uint16_t* pixel_ptr = active_data.begin();

        // --- DRAW CEILING ---
        for(int y = 0; y < draw_start; y++) {
            *pixel_ptr++ = ceiling_color;
        }
        
        // --- DRAW WALL ---
        // OPTIMIZATION 3: Branch Hoisting. Check the side ONCE, then run a pure math loop.
        if (hit.hit_side == 1) {
            // Shadowed Loop
            for(int y = draw_start; y < draw_end; y++) {
                uint32_t tex_y = (tex_pos >> 16) & 15;
                tex_pos += tex_step;
                
                // Grab color using only the Y offset (tex_y * 16)
                uint16_t color = tex_column[tex_y << 4];
                *pixel_ptr++ = (color >> 1) & 0x7BEF;
            }
        } else {
            // Unshadowed Loop
            for(int y = draw_start; y < draw_end; y++) {
                uint32_t tex_y = (tex_pos >> 16) & 15;
                tex_pos += tex_step;
                
                *pixel_ptr++ = tex_column[tex_y << 4];
            }
        }
        
        // --- DRAW FLOOR ---
        for(int y = draw_end; y < 240; y++) {
            *pixel_ptr++ = floor_color;
        }

        // Send to DMA
        active_slice.move_to(x, 0);
        active_slice.get_render_list().draw();
    }
}

#endif