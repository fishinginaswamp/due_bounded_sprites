#include "player.hpp"
#include "constants.hpp" 
#include "map.hpp"

RayHit Player::cast_ray(uint16_t angle) {
    RayHit hit;
    
    // 1. Grid position and fractional position
    int32_t mapX = this->x >> 16;
    int32_t mapY = this->y >> 16;
    
    int32_t fracX = this->x & 0xFFFF;
    int32_t fracY = this->y & 0xFFFF;

    // 2. Get ray direction from precalculated tables (Q16.16)
    int32_t rayDirX = cos_table[angle];
    int32_t rayDirY = sin_table[angle];

    // 3. Calculate Delta Distance (distance ray travels to cross 1 map unit)
    // Formula: abs(1 / rayDir). We use 64-bit to prevent overflow on division.
    int32_t deltaDistX = (rayDirX == 0) ? 0x7FFFFFFF : due_doom_abs((int32_t)(((int64_t)1 << 32) / rayDirX));
    int32_t deltaDistY = (rayDirY == 0) ? 0x7FFFFFFF : due_doom_abs((int32_t)(((int64_t)1 << 32) / rayDirY));

    int32_t stepX, stepY;
    int32_t sideDistX, sideDistY;

    // 4. Initial Side Distances
    if (rayDirX < 0) {
        stepX = -1;
        sideDistX = (fracX * (int64_t)deltaDistX) >> 16;
    } else {
        stepX = 1;
        sideDistX = ((65536 - fracX) * (int64_t)deltaDistX) >> 16;
    }

    if (rayDirY < 0) {
        stepY = -1;
        sideDistY = (fracY * (int64_t)deltaDistY) >> 16;
    } else {
        stepY = 1;
        sideDistY = ((65536 - fracY) * (int64_t)deltaDistY) >> 16;
    }

    // 5. The DDA Grid Stepping Loop
    int hit_found = 0;
    int side = 0;

    while (hit_found == 0) {
        if (sideDistX < sideDistY) {
            sideDistX += deltaDistX;
            mapX += stepX;
            side = 0;
        } else {
            sideDistY += deltaDistY;
            mapY += stepY;
            side = 1;
        }
        
        // Prevent out-of-bounds map reads
        if (mapX < 0 || mapX >= MAP_WIDTH || mapY < 0 || mapY >= MAP_HEIGHT) {
            hit.wall_type = 1; 
            break;
        }

        if (world_map[mapX][mapY] > 0) {
            hit_found = 1;
            hit.wall_type = world_map[mapX][mapY];
        }
    }

    hit.hit_side = side;

    // 6. Calculate True Euclidean Distance
    int32_t eucl_dist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);

    // 7. Calculate Exact Wall Hit X coordinate (fractional part only)
    if (side == 0) {
        hit.wall_x = this->y + (((int64_t)eucl_dist * rayDirY) >> 16);
    } else {
        hit.wall_x = this->x + (((int64_t)eucl_dist * rayDirX) >> 16);
    }
    hit.wall_x &= 0xFFFF; // Strip integer part, keep fraction

    // 8. Fix Fisheye (Project to camera plane)
    int16_t angle_diff = (angle - this->angle) & 4095;
    hit.perp_dist = ((int64_t)eucl_dist * cos_table[angle_diff]) >> 16;

    return hit;
}