#pragma once
#include <Adafruit_GFX.h>

const uint8_t tiniest_num42_Bitmaps[] PROGMEM = {
  0x0C, 0x80, 0x7E, 0xF0, 0xDB, 0x97, 0xAD, 0xE7, 0x6F, 0xD5, 0xFF, 0xF6 
};

const GFXglyph tiniest_num42_Glyphs[] PROGMEM = {
  {     0,   2,   4,   3,    0,   -4 },   // 0x2D '-'
  {     1,   1,   1,   2,    0,   -1 },   // 0x2E '.'
  {     0,   0,   0,   0,    0,    0 },   // 0x2F '/'
  {     2,   2,   4,   3,    0,   -4 },   // 0x30 '0'
  {     3,   1,   4,   3,    1,   -4 },   // 0x31 '1'
  {     4,   2,   4,   3,    0,   -4 },   // 0x32 '2'
  {     5,   2,   4,   3,    0,   -4 },   // 0x33 '3'
  {     6,   2,   4,   3,    0,   -4 },   // 0x34 '4'
  {     7,   2,   4,   3,    0,   -4 },   // 0x35 '5'
  {     8,   2,   4,   3,    0,   -4 },   // 0x36 '6'
  {     9,   2,   4,   3,    0,   -4 },   // 0x37 '7'
  {    10,   2,   4,   3,    0,   -4 },   // 0x38 '8'
  {    11,   2,   4,   3,    0,   -4 }    // 0x39 '9'
};

const GFXfont tiniest_num42 PROGMEM = {(uint8_t *) tiniest_num42_Bitmaps,  (GFXglyph *)tiniest_num42_Glyphs, 0x2D, 0x39,  4};