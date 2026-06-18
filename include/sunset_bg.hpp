#pragma once
#include <stdint.h>

#define RGB565(b, g, r) ((((r) & 0x1F) << 11) | (((g) & 0x3F) << 5) | ((b) & 0x1F))
#ifdef SAM_16_BIT_LCD_SMC
#define SWAP(x) (x)
#else
#define SWAP(x) (uint16_t)((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF))
#endif

#define A SWAP(RGB565(6, 6, 20))   // Deep Blue/Purple (Top Sky)
#define B SWAP(RGB565(26, 0, 20))  // Magenta (Mid Sky)
#define C SWAP(RGB565(31, 20, 0))  // Vibrant Orange (Low Sky)
#define S SWAP(RGB565(31, 60, 0))  // Yellow (The Sun)
#define W SWAP(RGB565(0, 15, 25))  // Dark Lake Water
#define R SWAP(RGB565(0, 45, 31))  // Cyan Water Ripples

// 3. Drawing Macros to compress 256 pixels into a single readable line
#define ROW(c) c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c
#define DITH(c1, c2) c1,c2,c1,c2,c1,c2,c1,c2,c1,c2,c1,c2,c1,c2,c1,c2

// 4. The 10 Reusable Tiles (Pixel Art)
const uint16_t t_sky_top_data[256] = { ROW(A), ROW(A), ROW(A), ROW(A), ROW(A), ROW(A), ROW(A), ROW(A), ROW(A), ROW(A), ROW(A), ROW(A), ROW(A), ROW(A), ROW(A), ROW(A) };
const uint16_t t_dither_AB_data[256] = { DITH(A,B), DITH(B,A), DITH(A,B), DITH(B,A), DITH(A,B), DITH(B,A), DITH(A,B), DITH(B,A), DITH(A,B), DITH(B,A), DITH(A,B), DITH(B,A), DITH(A,B), DITH(B,A), DITH(A,B), DITH(B,A) };
const uint16_t t_sky_mid_data[256] = { ROW(B), ROW(B), ROW(B), ROW(B), ROW(B), ROW(B), ROW(B), ROW(B), ROW(B), ROW(B), ROW(B), ROW(B), ROW(B), ROW(B), ROW(B), ROW(B) };
const uint16_t t_dither_BC_data[256] = { DITH(B,C), DITH(C,B), DITH(B,C), DITH(C,B), DITH(B,C), DITH(C,B), DITH(B,C), DITH(C,B), DITH(B,C), DITH(C,B), DITH(B,C), DITH(C,B), DITH(B,C), DITH(C,B), DITH(B,C), DITH(C,B) };
const uint16_t t_sky_low_data[256] = { ROW(C), ROW(C), ROW(C), ROW(C), ROW(C), ROW(C), ROW(C), ROW(C), ROW(C), ROW(C), ROW(C), ROW(C), ROW(C), ROW(C), ROW(C), ROW(C) };

const uint16_t t_sun_data[256] = {
	ROW(C),
	C,C,C,C,C,S,S,S,S,S,S,C,C,C,C,C,
	C,C,C,S,S,S,S,S,S,S,S,S,S,C,C,C,
	C,C,S,S,S,S,S,S,S,S,S,S,S,S,C,C,
	C,C,S,S,S,S,S,S,S,S,S,S,S,S,C,C,
	C,S,S,S,S,S,S,S,S,S,S,S,S,S,S,C,
	C,S,S,S,S,S,S,S,S,S,S,S,S,S,S,C,
	C,S,S,S,S,S,S,S,S,S,S,S,S,S,S,C,
	C,S,S,S,S,S,S,S,S,S,S,S,S,S,S,C,
	C,S,S,S,S,S,S,S,S,S,S,S,S,S,S,C,
	C,S,S,S,S,S,S,S,S,S,S,S,S,S,S,C,
	C,C,S,S,S,S,S,S,S,S,S,S,S,S,C,C,
	C,C,S,S,S,S,S,S,S,S,S,S,S,S,C,C,
	C,C,C,S,S,S,S,S,S,S,S,S,S,C,C,C,
	C,C,C,C,C,S,S,S,S,S,S,C,C,C,C,C,
	ROW(C)
};

const uint16_t t_horizon_center_data[256] = {
	ROW(C), // Glowing orange horizon line
	W,W,W,W,C,C,S,S,S,S,C,C,W,W,W,W,
	ROW(W),
	W,W,W,W,W,C,C,S,S,C,C,W,W,W,W,W,
	ROW(W),
	W,W,W,W,W,W,C,C,C,C,W,W,W,W,W,W,
	ROW(W),
	W,W,W,W,W,W,W,C,C,W,W,W,W,W,W,W,
	ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W)
};

const uint16_t t_horizon_edge_data[256] = {
	ROW(C), // Glowing orange horizon line
	ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W)
};

const uint16_t t_water_ripples_data[256] = {
	ROW(W),
	W,W,W,W,R,R,R,W,W,W,W,W,W,W,W,W,
	ROW(W),
	W,W,W,W,W,W,W,W,W,R,R,R,R,W,W,W,
	ROW(W), ROW(W),
	W,R,R,R,W,W,W,W,W,W,W,W,W,W,W,W,
	ROW(W),
	W,W,W,W,W,W,W,W,W,W,W,R,R,R,W,W,
	ROW(W), ROW(W),
	W,W,W,W,W,R,R,W,W,W,W,W,W,W,W,W,
	ROW(W),
	W,W,W,W,W,W,W,W,W,W,W,W,W,W,R,R,
	ROW(W), ROW(W)
};

const uint16_t t_water_solid_data[256] = { ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W) };




const uint16_t t_horizon_center_alt_data[256] = {
	ROW(C),
	W,W,W,C,C,S,S,S,S,C,C,W,W,W,W,W, // Shifted left
	ROW(W),
	W,W,W,W,C,C,S,S,C,C,W,W,W,W,W,W, // Shifted left
	ROW(W),
	W,W,W,W,W,C,C,C,C,W,W,W,W,W,W,W, // Shifted left
	ROW(W),
	W,W,W,W,W,W,C,C,W,W,W,W,W,W,W,W, // Shifted left
	ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W), ROW(W)
};

const uint16_t t_water_ripples_alt_data[256] = {
	ROW(W),
	W,W,W,W,W,W,R,R,R,W,W,W,W,W,W,W, // Shifted right
	ROW(W),
	W,W,W,W,W,W,W,W,W,W,W,R,R,R,R,W, // Shifted right
	ROW(W), ROW(W),
	W,W,W,R,R,R,W,W,W,W,W,W,W,W,W,W, // Shifted right
	ROW(W),
	W,W,W,W,W,W,W,W,W,W,W,W,W,R,R,R, // Shifted right
	ROW(W), ROW(W),
	W,W,W,W,W,W,W,R,R,W,W,W,W,W,W,W, // Shifted right
	ROW(W),
	W,R,R,W,W,W,W,W,W,W,W,W,W,W,W,W, // Shifted left (wrap around)
	ROW(W), ROW(W)
};


