#pragma once

#include <SAM3XDUE.H>
#include "raycast.hpp"
#include "player.hpp"
#include "sprite.h"
#include "textures.hpp"
#include "constants.hpp"

#ifndef DUE_DOOM_RENDERER
#define DUE_DOOM_RENDERER


inline void clear_buffer(uint32_t buf){
    dmac_ceiling_clear.daddr = buf;
    dmac_floor_clear.daddr = buf + 240;
    REG_DMAC_DSCR4 = reinterpret_cast<uint32_t>(&dmac_ceiling_clear);
    REG_DMAC_CHER = (1 << 4);
}

inline bool render_frame(Player& player, sprite slice[], sprite_data<1, 240> col_data[]) {
    extern volatile int32_t columns_drawn;
    extern int32_t z_buffer[320];

    // Skip if the DMAC is still rendering the previous frame
    if (columns_drawn < 320) return false;
    columns_drawn = 0;

    for (int x = 0; x < 320; x++) {

        uint8_t buf_idx = x & 3;
        sprite_data<1, 240>& active_data = col_data[buf_idx];
        sprite& active_slice = slice[buf_idx];

        int32_t angle_offset = (x * player.fov) / 320 - (player.fov / 2);
        uint16_t ray_angle = (player.angle + angle_offset) & 4095;
        RayHit hit = player.cast_ray(ray_angle);

        int32_t dist = hit.perp_dist;
        if (dist < 256) dist = 256;

        z_buffer[x] = hit.perp_dist;

        int32_t line_height = (240 * 65536) / dist;
        int16_t draw_start = 120 - (line_height / 2);
        if (draw_start < 0) draw_start = 0;
        int16_t draw_end = 120 + (line_height / 2);
        if (draw_end >= 240) draw_end = 239;

        uint32_t tex_x = ((hit.wall_x * 16) >> 16) & 15;
        uint32_t tex_step = (16 << 16) / line_height;
        uint32_t tex_pos = (draw_start - 120 + (line_height / 2)) * tex_step;

        uint8_t tex_idx = (hit.wall_type > 2) ? 1 : hit.wall_type;
        const uint16_t* tex_column = wall_textures[tex_idx] + tex_x;

        uint16_t* pixel_ptr = active_data.begin() + draw_start;

        //block until ready
        while ((x - columns_drawn) >= 4) {}

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
        
        active_slice.move_to(x, 0);
        active_slice.get_render_list().draw();
    }

    return true;
}

#endif