#pragma once

#include <SAM3XDUE.H>
#include <constants.hpp>

#ifndef DUE_DOOM_RAYCAST
#define DUE_DOOM_RAYCAST


struct RayHit {
    int32_t perp_dist; //Q16.16 perpendicular distance
    int32_t wall_x; //Q16.16 x/y where the wall was hit
    uint8_t hit_side; //0: x-axis, 1: y-axis aligned 
    uint8_t wall_type; //from world_map


};


#endif