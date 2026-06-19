#pragma once

#include <SAM3XDUE.H>
#include "raycast.hpp"

#ifndef DUE_DOOM_PLAYER
#define DUE_DOOM_PLAYER

struct Player {
    int32_t x;
    int32_t y;
    uint16_t angle;
    uint16_t fov;

    RayHit cast_ray(uint16_t angle);
    
};


#endif