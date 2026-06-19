#pragma once

#include <SAM3XDUE.H>

#ifndef DUE_DOOM_MAP
#define DUE_DOOM_MAP

constexpr uint8_t MAP_WIDTH = 8;
constexpr uint8_t MAP_HEIGHT = 8;
const uint8_t world_map[MAP_WIDTH][MAP_HEIGHT] = {
    {1,1,1,1,1,1,1,1},
    {1,0,0,2,0,0,0,1},
    {1,0,0,0,0,1,0,1},
    {1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,1},
    {1,0,0,0,0,2,0,1},
    {1,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1}
};

#endif