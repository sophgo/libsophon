#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#if !defined _WIN32 
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "vppion.h"
#endif
#include <fcntl.h>
#include <errno.h>
#include "vpplib.h"
#include "1684vppcmodel.h"
#include "math.h"

const int IL_MAX = 1 << (8+SHIFT);
const int hor_scal_008_32[5][32][8] =
{ {{0, 0, 0, 256, 0,   0, 0, 0},
   {0, 0, 0, 248, 8,   0, 0, 0},
   {0, 0, 0, 240, 16,  0, 0, 0},
   {0, 0, 0, 232, 24,  0, 0, 0},
   {0, 0, 0, 224, 32,  0, 0, 0},
   {0, 0, 0, 216, 40,  0, 0, 0},
   {0, 0, 0, 208, 48,  0, 0, 0},
   {0, 0, 0, 200, 56,  0, 0, 0},
   {0, 0, 0, 192, 64,  0, 0, 0},
   {0, 0, 0, 184, 72,  0, 0, 0},
   {0, 0, 0, 176, 80,  0, 0, 0},
   {0, 0, 0, 168, 88,  0, 0, 0},
   {0, 0, 0, 160, 96,  0, 0, 0},
   {0, 0, 0, 152, 104, 0, 0, 0},
   {0, 0, 0, 144, 112, 0, 0, 0},
   {0, 0, 0, 136, 120, 0, 0, 0},
   {0, 0, 0, 128, 128, 0, 0, 0},
   {0, 0, 0, 120, 136, 0, 0, 0},
   {0, 0, 0, 112, 144, 0, 0, 0},
   {0, 0, 0, 104, 152, 0, 0, 0},
   {0, 0, 0, 96,  160, 0, 0, 0},
   {0, 0, 0, 88,  168, 0, 0, 0},
   {0, 0, 0, 80,  176, 0, 0, 0},
   {0, 0, 0, 72,  184, 0, 0, 0},
   {0, 0, 0, 64,  192, 0, 0, 0},
   {0, 0, 0, 56,  200, 0, 0, 0},
   {0, 0, 0, 48,  208, 0, 0, 0},
   {0, 0, 0, 40,  216, 0, 0, 0},
   {0, 0, 0, 32,  224, 0, 0, 0},
   {0, 0, 0, 24,  232, 0, 0, 0},
   {0, 0, 0, 16,  240, 0, 0, 0},
   {0, 0, 0, 8,   248, 0, 0, 0}},

 {{ 0,  0,   0, 256,   0,   0,  0,  0},
  {-1,  3,  -7, 255,   8,  -3,  1,  0},
  {-2,  5, -14, 255,  16,  -6,  2,  0},
  {-2,  7, -19, 252,  24,  -8,  2,  0},
  {-3,  9, -24, 249,  33, -11,  4, -1},
  {-3, 10, -29, 245,  43, -14,  5, -1},
  {-4, 12, -33, 241,  52, -17,  6, -1},
  {-4, 13, -36, 235,  62, -20,  7, -1},
  {-4, 14, -39, 229,  73, -24,  8, -1},
  {-4, 15, -41, 222,  83, -27,  9, -1},
  {-4, 16, -43, 215,  94, -30, 10, -2},
  {-4, 16, -44, 206, 105, -32, 11, -2},
  {-4, 16, -45, 198, 116, -35, 12, -2},
  {-4, 16, -44, 188, 126, -37, 13, -2},
  {-4, 16, -44, 179, 137, -39, 14, -3},
  {-3, 16, -44, 168, 148, -41, 15, -3},
  {-3, 16, -43, 158, 158, -43, 16, -3},
  {-3, 15, -41, 148, 168, -44, 16, -3},
  {-3, 14, -39, 137, 179, -44, 16, -4},
  {-2, 13, -37, 126, 188, -44, 16, -4},
  {-2, 12, -35, 116, 198, -45, 16, -4},
  {-2, 11, -32, 105, 206, -44, 16, -4},
  {-2, 10, -30,  94, 215, -43, 16, -4},
  {-1,  9, -27,  83, 222, -41, 15, -4},
  {-1,  8, -24,  73, 229, -39, 14, -4},
  {-1,  7, -20,  62, 235, -36, 13, -4},
  {-1,  6, -17,  52, 241, -33, 12, -4},
  {-1,  5, -14,  43, 245, -29, 10, -3},
  {-1,  4, -11,  33, 249, -24,  9, -3},
  { 0,  2,  -8,  24, 252, -19,  7, -2},
  { 0,  2,  -6,  16, 255, -14,  5, -2},
  { 0,  1,  -3,   8, 255,  -7,  3, -1}},

 {{ 7, -11,  14, 236,  14, -11,  7,  0 },
  { 6,  -8,   7, 237,  22, -14,	 8, -2 },
  { 5,  -6,   1, 238,  30, -17,	 9, -4 },
  { 3,  -3,  -6, 237,  38, -19,	10, -4 },
  { 2,   0, -12, 234,  47, -22,	11, -4 },
  { 1,   2, -17, 232,  56, -25,	12, -5 },
  { 0,   5, -22, 228,  65, -28,	13, -5 },
  {-1,   7, -26, 223,  74, -30,	14, -5 },
  {-2,   9, -30, 219,  84, -33,	15, -6 },
  {-2,  10, -33, 213,  93, -35,	16, -6 },
  {-3,  12, -37, 207, 103, -37,	17, -6 },
  {-4,  13, -39, 201, 113, -39,	17, -6 },
  {-5,  15, -41, 194, 123, -41,	17, -6 },
  {-5,  15, -42, 186, 133, -42,	17, -6 },
  {-5,  16, -43, 177, 142, -43,	18, -6 },
  {-6,  17, -44, 170, 152, -44,	17, -6 },
  {-6,  17, -44, 161, 161, -44,	17, -6 },
  {-6,  17, -44, 152, 170, -44,	17, -6 },
  {-6,  18, -43, 142, 177, -43,	16, -5 },
  {-6,  17, -42, 133, 186, -42,	15, -5 },
  {-6,  17, -41, 123, 194, -41,	15, -5 },
  {-6,  17, -39, 113, 201, -39,	13, -4 },
  {-6,  17, -37, 103, 207, -37,	12, -3 },
  {-6,  16, -35,  93, 213, -33,	10, -2 },
  {-6,  15, -33,  84, 219, -30,	 9, -2 },
  {-5,  14, -30,  74, 223, -26,	 7, -1 },
  {-5,  13, -28,  65, 228, -22,	 5,  0 },
  {-5,  12, -25,  56, 232, -17,	 2,  1 },
  {-4,  11, -22,  47, 234, -12,	 0,  2 },
  {-4,  10, -19,  38, 237,  -6,	-3,  3 },
  {-4,   9, -17,  30, 238,   1,	-6,  5 },
  {-2,   8, -14,  22, 237,   7,	-8,  6 }},

{{12,  -22,   30,  216,  30,   -22,  12,    0 },
 {12,  -20,   24,  216,  37,   -24,  13,   -2 },
 {11,  -18,   18,  218,  45,   -27,  14,   -5 },
 {10,  -16,   12,  217,  53,   -29,  14,   -5 },
 {10,  -13,   6 ,  214,  60,   -31,  15,   -5 },
 { 9,  -11,   0 ,  212,  68,   -32,  15,   -5 },
 { 8,   -9,  -5 ,  210,  76,   -34,  15,   -5 },
 { 7,   -6,  -10,  206,  84,   -36,  15,   -5 },
 { 6,   -4,  -15,  203,  93,   -37,  15,   -5 },
 { 5,   -2,  -19,  198,  101,  -38,  15,   -4 },
 { 4,    0,  -23,  193,  109,  -39,  15,   -4 },
 { 3,    2,  -26,  188,  117,  -39,  14,   -3 },
 { 3,    4,  -29,  182,  125,  -39,  13,   -3 },
 { 2,    5,  -31,  176,  133,  -39,  13,   -3 },
 { 1,    7,  -34,  170,  141,  -39,  12,   -2 },
 { 0,    9,  -35,  162,  148,  -38,  11,   -1 },
 {-1,   10,  -37,  156,  156,  -37,  10,   -1 },
 {-1,   11,  -38,  148,  162,  -35,  9 ,    0 },
 {-2,   12,  -39,  141,  170,  -34,  7 ,    1 },
 {-3,   13,  -39,  133,  176,  -31,  5 ,    2 },
 {-3,   13,  -39,  125,  182,  -29,  4 ,    3 },
 {-3,   14,  -39,  117,  188,  -26,  2 ,    3 },
 {-4,   15,  -39,  109,  193,  -23,  0 ,    4 },
 {-4,   15,  -38,  101,  198,  -19,  -2,    5 },
 {-5,   15,  -37,   93,  203,  -15,  -4,    6 },
 {-5,   15,  -36,   84,  206,  -10,  -6,    7 },
 {-5,   15,  -34,   76,  210,   -5,  -9,    8 },
 {-5,   15,  -32,   68,  212,    0,  -11,   9 },
 {-5,   15,  -31,   60,  214,    6,  -13,   10},
 {-5,   14,  -29,   53,  217,   12,  -16,   10},
 {-5,   14,  -27,   45,  218,   18,  -18,   11},
 {-2,   13,  -24,   37,  216,   24,  -20,   12}},

{{-9,  9, 72, 112,  72,  9, -9,  0},
 {-9,  8, 69, 111,  75, 12, -9, -1},
 {-9,  6, 68, 111,  77, 13, -9, -1},
 {-8,  5, 65, 110,  79, 15, -9, -1},
 {-8,  4, 62, 110,  81, 17, -9, -1},
 {-8,  3, 60, 110,  83, 18, -9, -1},
 {-8,  2, 59, 109,  85, 20, -9, -2},
 {-8,  1, 57, 108,  87, 22, -9, -2},
 {-7,  0, 54, 107,  89, 24, -9, -2},
 {-7, -1, 52, 107,  91, 26, -9, -3},
 {-7, -2, 50, 106,  92, 27, -8, -3},
 {-6, -3, 47, 106,  94, 29, -8, -3},
 {-6, -4, 46, 105,  96, 31, -8, -4},
 {-6, -5, 44, 103,  97, 33, -7, -4},
 {-5, -5, 42, 102,  98, 35, -7, -4},
 {-5, -6, 40, 101, 100, 37, -6, -5},
 {-5, -6, 38, 101, 101, 38, -6, -5},

 {-5, -6, 37, 100, 101, 40, -6, -5},
 {-4, -7, 35,  98, 102, 42, -5, -5},
 {-4, -7, 33,  97, 103, 44, -5, -6},
 {-4, -8, 31,  96, 105, 46, -4, -6},
 {-3, -8, 29,  94, 106, 47, -3, -6},
 {-3, -8, 27,  92, 106, 50, -2, -7},
 {-3, -9, 26,  91, 107, 52, -1, -7},
 {-2, -9, 24,  89, 107, 54,  0, -7},
 {-2, -9, 22,  87, 108, 57,  1, -8},
 {-2, -9, 20,  85, 109, 59,  2, -8},
 {-1, -9, 18,  83, 110, 60,  3, -8},
 {-1, -9, 17,  81, 110, 62,  4, -8},
 {-1, -9, 15,  79, 110, 65,  5, -8},
 {-1, -9, 13,  77, 111, 68,  6, -9},
 {-1, -9, 12,  75, 111, 69,  8, -9}}

 };


const int ver_scal_008_32[5][32][4] =
{  {{0, 256,   0, 0},
    {0, 248,   8, 0},
    {0, 240,  16, 0},
    {0, 232,  24, 0},
    {0, 224,  32, 0},
    {0, 216,  40, 0},
    {0, 208,  48, 0},
    {0, 200,  56, 0},
    {0, 192,  64, 0},
    {0, 184,  72, 0},
    {0, 176,  80, 0},
    {0, 168,  88, 0},
    {0, 160,  96, 0},
    {0, 152, 104, 0},
    {0, 144, 112, 0},
    {0, 136, 120, 0},
    {0, 128, 128, 0},
    {0, 120, 136, 0},
    {0, 112, 144, 0},
    {0, 104, 152, 0},
    {0,  96, 160, 0},
    {0,  88, 168, 0},
    {0,  80, 176, 0},
    {0,  72, 184, 0},
    {0,  64, 192, 0},
    {0,  56, 200, 0},
    {0,  48, 208, 0},
    {0,  40, 216, 0},
    {0,  32, 224, 0},
    {0,  24, 232, 0},
    {0,  16, 240, 0},
    {0,   8, 248, 0}},

   {{  0, 256,   0,   0},
    { -6, 255,   7,   0},
    {-11, 254,  14,  -1},
    {-15, 251,  22,  -2},
    {-19, 248,  30,  -3},
    {-21, 243,  38,  -4},
    {-24, 238,  48,  -6},
    {-26, 232,  57,  -7},
    {-27, 225,  67,  -9},
    {-28, 217,  78, -11},
    {-29, 210,  88, -13},
    {-28, 201,  98, -15},
    {-28, 192, 109, -17},
    {-27, 182, 120, -19},
    {-27, 173, 131, -21},
    {-25, 162, 141, -22},
    {-24, 152, 152, -24},
    {-22, 141, 162, -25},
    {-21, 131, 173, -27},
    {-19, 120, 182, -27},
    {-17, 109, 192, -28},
    {-15,  98, 201, -28},
    {-13,  88, 210, -29},
    {-11,  78, 217, -28},
    { -9,  67, 225, -27},
    { -7,  57, 232, -26},
    { -6,  48, 238, -24},
    { -4,  38, 243, -21},
    { -3,  30, 248, -19},
    { -2,  22, 251, -15},
    { -1,  14, 254, -11},
    {  0,   7, 255,  -6}},

   {{  0, 256,   0,   0},
    { -4, 255,   5,   0},
    { -7, 254,  10,  -1},
    {-10, 250,  17,  -1},
    {-13, 247,  24,  -2},
    {-14, 242,  31,  -3},
    {-16, 236,  40,  -4},
    {-17, 229,  49,  -5},
    {-18, 222,  58,  -6},
    {-19, 214,  68,  -7},
    {-19, 205,  79,  -9},
    {-19, 196,  89, -10},
    {-19, 187, 100, -12},
    {-18, 176, 111, -13},
    {-18, 166, 122, -14},
    {-17, 155, 133, -15},
    {-16, 144, 144, -16},
    {-15, 133, 155, -17},
    {-14, 122, 166, -18},
    {-13, 111, 176, -18},
    {-12, 100, 187, -19},
    {-10,  89, 196, -19},
    { -9,  79, 205, -19},
    { -7,  68, 214, -19},
    { -6,  58, 222, -18},
    { -5,  49, 229, -17},
    { -4,  40, 236, -16},
    { -3,  31, 242, -14},
    { -2,  24, 247, -13},
    { -1,  17, 250, -10},
    { -1,  10, 254,  -7},
    {  0,   5, 255,  -4}},

  {{ 10, 236,  10,   0},
    {  5, 237,  16,  -2},
    { -1, 239,  23,  -5},
    { -5, 237,  30,  -6},
    {-10, 235,  38,  -7},
    {-13, 231,  46,  -8},
    {-16, 228,  54, -10},
    {-19, 223,  64, -12},
    {-21, 218,  72, -13},
    {-22, 211,  82, -15},
    {-24, 205,  92, -17},
    {-25, 197, 102, -18},
    {-25, 189, 112, -20},
    {-25, 180, 122, -21},
    {-25, 171, 132, -22},
    {-25, 162, 142, -23},
    {-24, 152, 152, -24},
    {-23, 142, 162, -25},
    {-22, 132, 171, -25},
    {-21, 122, 180, -25},
    {-20, 112, 189, -25},
    {-18, 102, 197, -25},
    {-17,  92, 205, -24},
    {-15,  82, 211, -22},
    {-13,  72, 218, -21},
    {-12,  64, 223, -19},
    {-10,  54, 228, -16},
    { -8,  46, 231, -13},
    { -7,  38, 235, -10},
    { -6,  30, 237,  -5},
    { -5,  23, 239,  -1},
    { -2,  16, 237,   5}},

   {{ 27, 202,  27,   0},
    { 22, 205,  34,  -5},
    { 18, 208,  40, -10},
    { 13, 207,  47, -11},
    {  9, 205,  54, -12},
    {  5, 203,  61, -13},
    {  1, 201,  68, -14},
    { -3, 198,  76, -15},
    { -7, 195,  84, -16},
    { -9, 190,  92, -17},
    {-12, 186, 100, -18},
    {-14, 180, 108, -18},
    {-16, 175, 116, -19},
    {-17, 168, 124, -19},
    {-18, 162, 132, -20},
    {-19, 155, 140, -20},
    {-20, 148, 148, -20},
    {-20, 140, 155, -19},
    {-20, 132, 162, -18},
    {-19, 124, 168, -17},
    {-19, 116, 175, -16},
    {-18, 108, 180, -14},
    {-18, 100, 186, -12},
    {-17,  92, 190,  -9},
    {-16,  84, 195,  -7},
    {-15,  76, 198,  -3},
    {-14,  68, 201,   1},
    {-13,  61, 203,   5},
    {-12,  54, 205,   9},
    {-11,  47, 207,  13},
    {-10,  40, 208,  18},
    { -5,  34, 205,  22}}  };

#if 0
/*Positive number * 1024*/
/*negative number * 1024, then Complement code*/
YPbPr2RGB, BT601
Y = 0.299 R + 0.587 G + 0.114 B
U = -0.1687 R - 0.3313 G + 0.5 B + 128
V = 0.5 R - 0.4187 G - 0.0813 B + 128
R = Y + 1.4018863751529200 (Cr-128)
G = Y - 0.345806672214672 (Cb-128) - 0.714902851111154 (Cr-128)
B = Y + 1.77098255404941 (Cb-128)

YCbCr2RGB, BT601
Y = 16  + 0.257 * R + 0.504 * g + 0.098 * b
Cb = 128 - 0.148 * R - 0.291 * g + 0.439 * b
Cr = 128 + 0.439 * R - 0.368 * g - 0.071 * b
R = 1.164 * (Y - 16) + 1.596 * (Cr - 128)
G = 1.164 * (Y - 16) - 0.392 * (Cb - 128) - 0.812 * (Cr - 128)
B = 1.164 * (Y - 16) + 2.016 * (Cb - 128)
#endif

int csc_matrix_list[CSC_MAX][12] = {
//YCbCr2RGB,BT601
{0x000004a8, 0x00000000, 0x00000662, 0xfffc8450,
	0x000004a8, 0xfffffe6f, 0xfffffcc0, 0x00021e4d,
	0x000004a8, 0x00000812, 0x00000000, 0xfffbaca8 },
//YPbPr2RGB,BT601
{0x00000400, 0x00000000, 0x0000059b, 0xfffd322d,
	0x00000400, 0xfffffea0, 0xfffffd25, 0x00021dd6,
	0x00000400, 0x00000716, 0x00000000, 0xfffc74bc },
//RGB2YCbCr,BT601
{ 0x107, 0x204, 0x64, 0x4000,
	0xffffff68, 0xfffffed6, 0x1c2, 0x20000,
	0x1c2, 0xfffffe87, 0xffffffb7, 0x20000 },
//YCbCr2RGB,BT709
{ 0x000004a8, 0x00000000, 0x0000072c, 0xfffc1fa4,
	0x000004a8, 0xffffff26, 0xfffffdde, 0x0001338e,
	0x000004a8, 0x00000873, 0x00000000, 0xfffb7bec },
//RGB2YCbCr,BT709
{ 0x000000bb, 0x00000275, 0x0000003f, 0x00004000,
	0xffffff99, 0xfffffea5, 0x000001c2, 0x00020000,
	0x000001c2, 0xfffffe67, 0xffffffd7, 0x00020000 },
//RGB2YPbPr,BT601
{ 0x132, 0x259, 0x74, 0,
	0xffffff54, 0xfffffead, 0x200, 0x20000,
	0x200, 0xfffffe54, 0xffffffad, 0x20000 },
//YPbPr2RGB,BT709
{ 0x00000400, 0x00000000, 0x0000064d, 0xfffcd9be,
	0x00000400, 0xffffff40, 0xfffffe21, 0x00014fa1,
	0x00000400, 0x0000076c, 0x00000000, 0xfffc49ed },
//RGB2YPbPr,BT709
{ 0x000000da, 0x000002dc, 0x0000004a, 0,
	0xffffff8b, 0xfffffe75, 0x00000200, 0x00020000,
	0x00000200, 0xfffffe2f, 0xffffffd1, 0x00020000 },
};

void bilinear_intp (
    uint16 fx,
    uint16 fy,
    uint8 p0,
    uint8 p1,
    uint8 p2,
    uint8 p3,
    uint8* pout)
{
	// Calculate the weights for each pixel
  //printf("fx, fy: %u %u\n", fx, fy);
	uint16 fx1 = (1 << FP_WL) - fx;
	uint16 fy1 = (1 << FP_WL) - fy;

	// We perform the actual weighting of pixel values with fixed-point arithmetic
	uint16 w0 = (fx1 * fy1 + (1 << (FP_WL - 1))) >> FP_WL; // rounding
	uint16 w1 = (fx  * fy1 + (1 << (FP_WL - 1))) >> FP_WL;
	uint16 w2 = (fx1 * fy + (1 << (FP_WL - 1))) >> FP_WL;
	uint16 w3 = (fx  * fy + (1 << (FP_WL - 1))) >> FP_WL;

	uint32 out_tmp = p0 * w0 + p1 * w1 + p2 * w2 + p3 * w3;
  out_tmp = (out_tmp + (1<<(FP_WL-1))) >> FP_WL;
  if(out_tmp>=255)
    out_tmp = 255;
	pout[0] = out_tmp; // rounding

}

void nearest_intp (
    uint16 fx,
    uint16 fy,
    uint8 p0,
    uint8 p1,
    uint8 p2,
    uint8 p3,
    uint8* pout,
    uint8 nearest_mode)
{
	
	uint8 out_tmp;
	uint16 all_value=1 << FP_WL;
	uint16 middle_value=(1 << FP_WL)/2;
	if(nearest_mode==0x00)  //floor
	{
		if((fx<all_value)&&(fy<all_value))
		{
			out_tmp=p0;
		}
		else 
		{
			printf("\n nearest alth error!!");
		}
	}
	else       //round
	{
		if((fx<middle_value)&&(fy<middle_value))
		{
			out_tmp=p0;
		}
		else if((fx>=middle_value)&&(fy<middle_value))
		{
			out_tmp=p1;
		}
		else if((fx<middle_value)&&(fy>=middle_value))
		{
			out_tmp=p2;
		}
		else if((fx>=middle_value)&&(fy>=middle_value))
		{
			out_tmp=p3;
		}
	}
  if(out_tmp>=255)
    out_tmp = 255;
	pout[0] = out_tmp; // rounding
  //printf("%u %u %u %u   %u\n", p0, p1, p2, p3, *pout);

}

void scaling_top_1684 (
    VPP1684_PARAM *p_param,
    int *rgb_in,
    int *rgb_out,
    int height_o,
    int width_o,
    int height_i,
    int width_i,
    float scale_h,
    float scale_w,
    int hor_filt_sel,
    int ver_filt_sel,
    int hor_filt_init,
    int ver_filt_init,
    int hor_decimation
)
{

  int **hor_scal_coe = (int **)malloc(32 * sizeof(int*));
  for(int i=0; i<32; i++)
    hor_scal_coe[i] = (int *)malloc(8 * sizeof(int));

  for(int i=0; i<32; i++) {
    for(int j=0; j<8; j++) {
      hor_scal_coe[i][j] = hor_scal_008_32[hor_filt_sel][i][j];
    }
  }
  

  int **ver_scal_coe = (int **)malloc(32 * sizeof(int*));
  for(int i=0; i<32; i++)
    ver_scal_coe[i] = (int *)malloc(4 * sizeof(int));

  for(int i=0; i<32; i++) {
    for(int j=0; j<4; j++) {
      ver_scal_coe[i][j] = ver_scal_008_32[ver_filt_sel][i][j];
    }
  }

  const int precision = 16384;
  const int segment = 32;
  const int hor_filt_size = 8;
  const int ver_filt_size = 4;
  const int hor_ext_size = hor_filt_size/2-1;
  const int ver_ext_size = ver_filt_size/2-1;

  const int hor_step = round(scale_w*precision);
  const int ver_step = round(scale_h*precision);
  //float hor_step_dec, ver_step_dec;

  int *rgb_dec  = (int *)malloc(width_i*3 * sizeof(int));
  int *rgb_proc = (int *)malloc(height_i*width_o*3 * sizeof(int));
  int *hor_tap_data = (int *)malloc(hor_filt_size*3 * sizeof(int));
  int *ver_tap_data = (int *)malloc(ver_filt_size*3 * sizeof(int));

  int xx, yy;
  int cur_hor_pos = 0, cur_ver_pos = 0;
  int cur_hor_pos_int, cur_hor_pos_sub, cur_hor_pos_mod;
  int cur_ver_pos_int, cur_ver_pos_sub, cur_ver_pos_mod;
  int hor_sum_ch0 = 0, hor_sum_ch1 = 0, hor_sum_ch2 = 0;
  int ver_sum_ch0 = 0, ver_sum_ch1 = 0, ver_sum_ch2 = 0;
  int rgb_out_tmp[3] = {0};


  int *cur_hor_coeff = (int *)malloc(hor_filt_size * sizeof(int));
  int *cur_ver_coeff = (int *)malloc(ver_filt_size * sizeof(int));

  int width_i_dec = 0;

  if(hor_decimation==2)
    width_i_dec = width_i/4;
  else if(hor_decimation==1)
    width_i_dec = width_i/2;
  else
    width_i_dec = width_i;


  for (int y = 0; y < height_i; y++) {

    cur_hor_pos = hor_filt_init;

  for (int x = 0; x < width_i_dec; x++) {

      if (hor_decimation==0) {
       rgb_dec[x*3+0] = rgb_in[y*width_i*3+x*3+0];
       rgb_dec[x*3+1] = rgb_in[y*width_i*3+x*3+1];
       rgb_dec[x*3+2] = rgb_in[y*width_i*3+x*3+2];
      }
      else if (hor_decimation==1 ) {
       rgb_dec[x*3+0] = round((rgb_in[y*width_i*3+x*2*3+0] + rgb_in[y*width_i*3+x*2*3+3] + 1)/2);
       rgb_dec[x*3+1] = round((rgb_in[y*width_i*3+x*2*3+1] + rgb_in[y*width_i*3+x*2*3+4] + 1)/2);
       rgb_dec[x*3+2] = round((rgb_in[y*width_i*3+x*2*3+2] + rgb_in[y*width_i*3+x*2*3+5] + 1)/2);
      }
      else if (hor_decimation==2 ) {
       rgb_dec[x*3+0] = round((rgb_in[y*width_i*3+x*4*3+0] + rgb_in[y*width_i*3+x*4*3+3] + rgb_in[y*width_i*3+x*4*3+6] + rgb_in[y*width_i*3+x*4*3+9]+ 2)/4);
       rgb_dec[x*3+1] = round((rgb_in[y*width_i*3+x*4*3+1] + rgb_in[y*width_i*3+x*4*3+4] + rgb_in[y*width_i*3+x*4*3+7] + rgb_in[y*width_i*3+x*4*3+10]+ 2)/4);
       rgb_dec[x*3+2] = round((rgb_in[y*width_i*3+x*4*3+2] + rgb_in[y*width_i*3+x*4*3+5] + rgb_in[y*width_i*3+x*4*3+8] + rgb_in[y*width_i*3+x*4*3+11]+ 2)/4);
      }

  }  // hor_dec

  for (int x = 0; x < width_o; x++) {

      cur_hor_pos_int = floor(cur_hor_pos/precision);
      cur_hor_pos_sub = floor(cur_hor_pos/(precision/segment));
      cur_hor_pos_mod = cur_hor_pos_sub % segment;

      for(int i=0; i<hor_filt_size; i++) {
        if(cur_hor_pos_int-hor_ext_size+i < 0)
          xx = 0;
        else if(cur_hor_pos_int-hor_ext_size+i > width_i_dec-1)
          xx = width_i_dec-1;
        else
          xx = cur_hor_pos_int-hor_ext_size+i;

        cur_hor_coeff[i] = hor_scal_coe[cur_hor_pos_mod][i];
//        cur_hor_coeff[i] = hor_scal_008_32[hor_filt_sel][cur_hor_pos_mod][i];

        hor_tap_data[i]                 = rgb_dec[xx*3+0];
        hor_tap_data[i+hor_filt_size]   = rgb_dec[xx*3+1];
        hor_tap_data[i+hor_filt_size*2] = rgb_dec[xx*3+2];

      }

      hor_sum_ch0 = 0;
      hor_sum_ch1 = 0;
      hor_sum_ch2 = 0;

      for(int i=0; i<hor_filt_size; i++) {
        hor_sum_ch0 += cur_hor_coeff[i]*hor_tap_data[i];
        hor_sum_ch1 += cur_hor_coeff[i]*hor_tap_data[i+hor_filt_size];
        hor_sum_ch2 += cur_hor_coeff[i]*hor_tap_data[i+hor_filt_size*2];
      }

      rgb_out_tmp[0] = round((float)(hor_sum_ch0)/256);
      rgb_out_tmp[1] = round((float)(hor_sum_ch1)/256);
      rgb_out_tmp[2] = round((float)(hor_sum_ch2)/256);

      rgb_proc[y*width_o*3+x*3+0] = (rgb_out_tmp[0]>255)?255:(rgb_out_tmp[0]<0) ? 0 : rgb_out_tmp[0];
      rgb_proc[y*width_o*3+x*3+1] = (rgb_out_tmp[1]>255)?255:(rgb_out_tmp[1]<0) ? 0 : rgb_out_tmp[1];
      rgb_proc[y*width_o*3+x*3+2] = (rgb_out_tmp[2]>255)?255:(rgb_out_tmp[2]<0) ? 0 : rgb_out_tmp[2];

      cur_hor_pos += hor_step;
    }
  }



 // printf("\n----- Processing Vertical Scaling -----\n");

  cur_ver_pos = ver_filt_init;

  for (int y = 0; y < height_o; y++) {

  for (int x = 0; x < width_o; x++) {

      cur_ver_pos_int = floor(cur_ver_pos/precision);
      cur_ver_pos_sub = floor(cur_ver_pos/(precision/segment));
      cur_ver_pos_mod = cur_ver_pos_sub % segment;

      for(int i=0; i<ver_filt_size; i++) {
        if(cur_ver_pos_int-ver_ext_size+i < 0)
          yy = 0;
        else if(cur_ver_pos_int-ver_ext_size+i > height_i-1)
          yy = height_i-1;
        else
          yy = cur_ver_pos_int-ver_ext_size+i;

       // cur_ver_coeff[i] = ver_scal_008_32[ver_filt_sel][cur_ver_pos_mod][i];
        cur_ver_coeff[i] = ver_scal_coe[cur_ver_pos_mod][i];

        ver_tap_data[i]                 = rgb_proc[yy*width_o*3+x*3+0];
        ver_tap_data[i+ver_filt_size]   = rgb_proc[yy*width_o*3+x*3+1];
        ver_tap_data[i+ver_filt_size*2] = rgb_proc[yy*width_o*3+x*3+2];

      //printf("ver_tap_data0 =%d ver_tap_data1 =%d ver_tap_data2 =%d  \n  ",ver_tap_data[i],ver_tap_data[i+hor_filt_size],ver_tap_data[i+ver_filt_size*2]);
      //printf("cur_ver_coeff =%d  \n  ",cur_ver_coeff[i]);

      }

      ver_sum_ch0 = 0;
      ver_sum_ch1 = 0;
      ver_sum_ch2 = 0;

      for(int i=0; i<ver_filt_size; i++) {
        ver_sum_ch0 += cur_ver_coeff[i]*ver_tap_data[i];
        ver_sum_ch1 += cur_ver_coeff[i]*ver_tap_data[i+ver_filt_size];
        ver_sum_ch2 += cur_ver_coeff[i]*ver_tap_data[i+ver_filt_size*2];
      }

    //printf("\n VER cur_pos_y = %d, cur_pos_x=%d, , Ver_sum_ch0= %x, ver_sum_ch1= %x , ver_sum_ch2= %x  \n",y,x,ver_sum_ch0/256, ver_sum_ch1/256, ver_sum_ch2/256);
      rgb_out_tmp[0] = round((float)(ver_sum_ch0)/256);
      rgb_out_tmp[1] = round((float)(ver_sum_ch1)/256);
      rgb_out_tmp[2] = round((float)(ver_sum_ch2)/256);

      rgb_out[y*width_o*3+x*3+0] = (rgb_out_tmp[0]>255)?255:(rgb_out_tmp[0]<0)?0:rgb_out_tmp[0];
      rgb_out[y*width_o*3+x*3+1] = (rgb_out_tmp[1]>255)?255:(rgb_out_tmp[1]<0)?0:rgb_out_tmp[1];
      rgb_out[y*width_o*3+x*3+2] = (rgb_out_tmp[2]>255)?255:(rgb_out_tmp[2]<0)?0:rgb_out_tmp[2];
    }

    cur_ver_pos += ver_step;
  }


  free(cur_ver_coeff);
  free(cur_hor_coeff);

  free(ver_tap_data);
  free(hor_tap_data);
  free(rgb_proc);
  free(rgb_dec);

  for(int i=0; i<32; i++)
    free (ver_scal_coe[i]);

  free(ver_scal_coe);

  for(int i=0; i<32; i++)
    free (hor_scal_coe[i]);

  free(hor_scal_coe);
}

void scaling_top (
    bool yuvIn,
    bool yuvOut,
    int  algo_sel,
    int bili_mode,
    int nearest_mode,
    int *rgb_in,
    int *rgb_out,
    int height_o,
    int width_o,
    float scale_h,
    float scale_w,
    int i_HScaleRev,
    int i_WScaleRev)
{
//  printf("\n----- Start VPP-1684 Bilinear Scaling  -----\n");

  int height = round(height_o*scale_h);
  int width  = round(width_o*scale_w);
  const int ofs_i = height*width;
  const int ofs_o = height_o*width_o;
  //int extrabytes = (4 - (width_o * 3) % 4) % 4;
  int py, px, py_plus, px_plus;
  int py_tmp, px_tmp;
  //float fy, fx;
  int fy_x, fx_x;
  uint8 pout;
//  printf("Param: bili_mode Dst h & w, Src h & w : %d %d %d %d %d\n", bili_mode, height_o, width_o, height, width);
//  printf("Param: scale_h, scale_w = %f %f\n", scale_h, scale_w);

  for (int y = 0; y < height_o; y++) {
		for (int x = 0; x < width_o; x++) {
      // calculation by floating point
      //py = int(float(y) * scale_h);
      //px = int(float(x) * scale_w);
      //fy = float(y) * scale_h - py;
      //fx = float(x) * scale_w - px;

      // calculation by fixed point
      if(bili_mode == 0) {//no use
        // py = y*scale_h, px = x*scale_w
        py = y * (i_HScaleRev >> 14) + ((y * (i_HScaleRev & 0x00003fff)) >> 14);
        px = x * (i_WScaleRev >> 14) + ((x * (i_WScaleRev & 0x00003fff)) >> 14);
        fy_x = (y * (i_HScaleRev & 0x00003fff)) >> (14-FP_WL);
        fx_x = (x * (i_WScaleRev & 0x00003fff)) >> (14-FP_WL);

      }else if(bili_mode == 1) {
        // py = (y+0.5)*scale_h-0.5 = (y+0.5)*(a+o.b)-0.5 = y*a + 0.5*[(a-1)+(2y+1)*0.b]
        // px = (x+0.5)*scale_w-0.5 = (x+0.5)*(c+o.d)-0.5 = x*c + 0.5*[(c-1)+(2x+1)*0.d]
        py = (y*2+1) * (i_HScaleRev >> 14) - 1;
        py_tmp = ((y*2+1) * (i_HScaleRev & 0x00003fff)) >> 14;
        //if(py + py_tmp<0)
        //  printf("-----py < 0 : %d %d %d %d-----\n", py, py_tmp, py + py_tmp, (py + py_tmp) >> 1);
        py = (py + py_tmp) >> 1;
        px = (x*2+1) * (i_WScaleRev >> 14) - 1;
        px_tmp = ((x*2+1) * (i_WScaleRev & 0x00003fff)) >> 14;
        //if(px + px_tmp<0)
        //  printf("-----px < 0 : %d %d %d %d-----\n", px, px_tmp, px + px_tmp, (px + px_tmp) >> 1);
        px = (px + px_tmp) >> 1;
        //py = y * (i_HScaleRev >> 14);
        //px = x * (i_WScaleRev >> 14);
        //py += ( ( (i_HScaleRev >> 14) - 1 + (((y*2+1) * (i_HScaleRev & 0x00003fff)) >> 14) ) >> 1 );
        //px += ( ( (i_WScaleRev >> 14) - 1 + (((x*2+1) * (i_WScaleRev & 0x00003fff)) >> 14) ) >> 1 );
        //fy_x = ( ( (i_HScaleRev & 0x003fc000) - (1<<14) + ((y*2+1) * (i_HScaleRev & 0x00003fff)) ) >> 1 ) >> (14-FP_WL);
        //fx_x = ( ( (i_WScaleRev & 0x003fc000) - (1<<14) + ((x*2+1) * (i_WScaleRev & 0x00003fff)) ) >> 1 ) >> (14-FP_WL);

        if(py<0)
          fy_x = 0;
        else
          fy_x = ( ( (i_HScaleRev & 0x003fc000) - (1<<14) + ((y*2+1) * (i_HScaleRev & 0x00003fff)) ) >> 1 ) >> (14-FP_WL);

        if(px<0)
          fx_x = 0;
        else
          fx_x = ( ( (i_WScaleRev & 0x003fc000) - (1<<14) + ((x*2+1) * (i_WScaleRev & 0x00003fff)) ) >> 1 ) >> (14-FP_WL);

//        if(fy_x<0 && py >=0)
//          printf("-----fy_x py : %d %d-----\n", fy_x, py);
//        if(fx_x<0 && px >=0)
//          printf("-----fx_x px : %d %d-----\n", fx_x, px);

      }else {
        printf("ERROR: Invalid bilinear mode parameter\n");
        exit(-1);
      }

      if(py<0) {
        //printf("-----py < 0 : %d-----\n", py);
        py = 0;
        py_plus = 0;
      }else {
        py = (py < height) ? py : height-1;
        py_plus = (py < height - 1) ? py + 1 : py;
      }
      if(px<0) {
        //printf("-----px < 0 : %d-----\n", px);
        px = 0;
        px_plus = 0;
      }else {
        px = (px < width) ? px : width-1;
        px_plus = (px < width - 1) ? px + 1 : px;
      }

	 // printf("-----fx_x px : %d %d-----\n", fx_x, px);
	  // printf("-----fy_x py : %d %d-----\n", fy_x, py);
      int chan = (yuvIn)?1:3;
      for (int c = 0; c < chan; c++) {
	  	if(algo_sel==0x02)
	  	{
        bilinear_intp(
          //uint16(fx*(1<<FP_WL)), // calculation by floating point
          //uint16(fy*(1<<FP_WL)), // calculation by floating point
          (uint16)(fx_x & 0x00003fff), // calculation by fixed point
          (uint16)(fy_x & 0x00003fff), // calculation by fixed point
          (uint8)(rgb_in[py*width*chan+px*chan+c]),
          (uint8)(rgb_in[py*width*chan+px_plus*chan+c]),
          (uint8)(rgb_in[py_plus*width*chan+px*chan+c]),
          (uint8)(rgb_in[py_plus*width*chan+px_plus*chan+c]),
          &pout);
	  	}
		else if(algo_sel==0x03)
		{
			nearest_intp (
     //uint16(fx*(1<<FP_WL)), // calculation by floating point
          //uint16(fy*(1<<FP_WL)), // calculation by floating point
          (uint16)(fx_x & 0x00003fff), // calculation by fixed point
          (uint16)(fy_x & 0x00003fff), // calculation by fixed point
          (uint8)(rgb_in[py*width*chan+px*chan+c]),
          (uint8)(rgb_in[py*width*chan+px_plus*chan+c]),
          (uint8)(rgb_in[py_plus*width*chan+px*chan+c]),
          (uint8)(rgb_in[py_plus*width*chan+px_plus*chan+c]),
          &pout,
          nearest_mode);

		}
		else
		{
			printf("\nsorry,not find\n");
		}
        //printf("%d %d\n", rgb_in[py*width*3+px*3+c], rgb_in[py*width*3+px_plus*3+c]);
        if(yuvIn)
          rgb_out[y*width_o+x] = (int)(pout);
        else
          rgb_out[y*(width_o*3)+x*3+c] = (int)(pout);

        // UV Scaling if needed
        if(yuvOut && y%2==0 && x%2==0) {
          //py = int(float(y/2) * scale_h);
          //px = int(float(x/2) * scale_w);
          //py = y/2 * (i_HScaleRev >> 14) + ((y/2 * (i_HScaleRev & 0x00003fff)) >> 14);
          //px = x/2 * (i_WScaleRev >> 14) + ((x/2 * (i_WScaleRev & 0x00003fff)) >> 14);
          //fy_x = (y/2 * (i_HScaleRev & 0x00003fff)) >> (14-FP_WL);
          //fx_x = (x/2 * (i_WScaleRev & 0x00003fff)) >> (14-FP_WL);


          if(bili_mode == 0) {
            // py = y*scale_h, px = x*scale_w
            py = y/2 * (i_HScaleRev >> 14) + ((y/2 * (i_HScaleRev & 0x00003fff)) >> 14);
            px = x/2 * (i_WScaleRev >> 14) + ((x/2 * (i_WScaleRev & 0x00003fff)) >> 14);
            fy_x = (y/2 * (i_HScaleRev & 0x00003fff)) >> (14-FP_WL);
            fx_x = (x/2 * (i_WScaleRev & 0x00003fff)) >> (14-FP_WL);

          }else if(bili_mode == 1) {
            // py = (y+0.5)*scale_h-0.5 = (y+0.5)*(a+o.b)-0.5 = y*a + 0.5*[(a-1)+(2y+1)*0.b]
            // px = (x+0.5)*scale_w-0.5 = (x+0.5)*(c+o.d)-0.5 = x*c + 0.5*[(c-1)+(2x+1)*0.d]
            py = (y+1) * (i_HScaleRev >> 14) - 1;
            py_tmp = ((y+1) * (i_HScaleRev & 0x00003fff)) >> 14;
            //if(py + py_tmp<0)
            //  printf("-----UV py < 0 : %d %d %d %d-----\n", py, py_tmp, py + py_tmp, (py + py_tmp) >> 1);
            py = (py + py_tmp) >> 1;
            px = (x+1) * (i_WScaleRev >> 14) - 1;
            px_tmp = ((x+1) * (i_WScaleRev & 0x00003fff)) >> 14;
            //if(px + px_tmp<0)
            //  printf("-----UV px < 0 : %d %d %d %d-----\n", px, px_tmp, px + px_tmp, (px + px_tmp) >> 1);
            px = (px + px_tmp) >> 1;
            //py = y * (i_HScaleRev >> 14);
            //px = x * (i_WScaleRev >> 14);
            //py += ( ( (i_HScaleRev >> 14) - 1 + (((y*2+1) * (i_HScaleRev & 0x00003fff)) >> 14) ) >> 1 );
            //px += ( ( (i_WScaleRev >> 14) - 1 + (((x*2+1) * (i_WScaleRev & 0x00003fff)) >> 14) ) >> 1 );
            //fy_x = ( ( (i_HScaleRev & 0x003fc000) - (1<<14) + ((y+1) * (i_HScaleRev & 0x00003fff)) ) >> 1 ) >> (14-FP_WL);
            //fx_x = ( ( (i_WScaleRev & 0x003fc000) - (1<<14) + ((x+1) * (i_WScaleRev & 0x00003fff)) ) >> 1 ) >> (14-FP_WL);

            if(py<0)
              fy_x = 0;
            else
              fy_x = ( ( (i_HScaleRev & 0x003fc000) - (1<<14) + ((y+1) * (i_HScaleRev & 0x00003fff)) ) >> 1 ) >> (14-FP_WL);

            if(px<0)
              fx_x = 0;
            else
              fx_x = ( ( (i_WScaleRev & 0x003fc000) - (1<<14) + ((x+1) * (i_WScaleRev & 0x00003fff)) ) >> 1 ) >> (14-FP_WL);

          }

          if(py<0) {
            //printf("-----py < 0 : %d-----\n", py);
            py = 0;
            py_plus = 0;
          }else {
            py = (py < height/2) ? py : height/2-1;
            py_plus = (py < height/2 - 1) ? py + 1 : py;
          }
          if(px<0) {
            //printf("-----px < 0 : %d-----\n", px);
            px = 0;
            px_plus = 0;
          }else {
            px = (px < width/2) ? px : width/2-1;
            px_plus = (px < width/2 - 1) ? px + 1 : px;
          }

          // U scaling
          bilinear_intp(
            (uint16)(fx_x & 0x00003fff), // calculation by fixed point
            (uint16)(fy_x & 0x00003fff), // calculation by fixed point
            (uint8)(rgb_in[ofs_i+py*width/2+px]),
            (uint8)(rgb_in[ofs_i+py*width/2+px_plus]),
            (uint8)(rgb_in[ofs_i+py_plus*width/2+px]),
            (uint8)(rgb_in[ofs_i+py_plus*width/2+px_plus]),
            &pout);
          rgb_out[ofs_o+y/2*width_o/2+x/2] = (int)(pout);
          // V scaling
          bilinear_intp(
            (uint16)(fx_x & 0x00003fff), // calculation by fixed point
            (uint16)(fy_x & 0x00003fff), // calculation by fixed point
            (uint8)(rgb_in[ofs_i+ofs_i/4+py*width/2+px]),
            (uint8)(rgb_in[ofs_i+ofs_i/4+py*width/2+px_plus]),
            (uint8)(rgb_in[ofs_i+ofs_i/4+py_plus*width/2+px]),
            (uint8)(rgb_in[ofs_i+ofs_i/4+py_plus*width/2+px_plus]),
            &pout);
          rgb_out[ofs_o+ofs_o/4+y/2*width_o/2+x/2] = (int)(pout);
        }
        //printf("%d\n", rgb_out[y*(width_o*3+extrabytes)+x*3+c]);
      }
		}
	}

//  printf("\n----- End VPP-1682 Bilinear Scaling  -----\n");
}

void csc (
    DEF_CSC_MATRIX *csc_matrix,
    int  Y,
    int  U,
    int  V,
    int* R,
    int* G,
    int* B)
{
    int tempR  , tempG  , tempB  ;
    int tempY0 , tempY1 , tempY2 ;
    int tempCb0, tempCb1, tempCb2;
    int tempCr0, tempCr1, tempCr2;

    tempY0  = Y*csc_matrix->CSC_COE00;
    tempY1  = Y*csc_matrix->CSC_COE10;
    tempY2  = Y*csc_matrix->CSC_COE20;

    tempCb0 = U*csc_matrix->CSC_COE01;
    tempCb1 = U*csc_matrix->CSC_COE11;
    tempCb2 = U*csc_matrix->CSC_COE21;

    tempCr0 = V*csc_matrix->CSC_COE02;
    tempCr1 = V*csc_matrix->CSC_COE12;
    tempCr2 = V*csc_matrix->CSC_COE22;

    tempR = tempY0 + tempCb0 + tempCr0 + csc_matrix->CSC_ADD0;
    tempG = tempY1 + tempCb1 + tempCr1 + csc_matrix->CSC_ADD1;
    tempB = tempY2 + tempCb2 + tempCr2 + csc_matrix->CSC_ADD2;

    //printf("temp: Y(%03x)(%d) U(%03x)(%d) V(%03x)(%d) +%08x(%d) %08x %08x %08x\n", Y,reg_CSC_CI00,U,reg_CSC_CI01,V,reg_CSC_CI02, reg_CSC_SI0,reg_CSC_SI0,tempR, tempG, tempB);

    tempR = sign(tempR) * ((iabs(tempR)+ (iabs(tempR)&(1<<(CSC_WL-1)))) >> CSC_WL);
    tempG = sign(tempG) * ((iabs(tempG)+ (iabs(tempG)&(1<<(CSC_WL-1)))) >> CSC_WL);
    tempB = sign(tempB) * ((iabs(tempB)+ (iabs(tempB)&(1<<(CSC_WL-1)))) >> CSC_WL);

    //printf("temp: %08x   %08x   %08x\n", tempR, tempG, tempB);
  
    tempR = clip(tempR, 0, IL_MAX - 1);
    tempG = clip(tempG, 0, IL_MAX - 1);
    tempB = clip(tempB, 0, IL_MAX - 1);

    //printf("temp: %08x   %08x   %08x\n", tempR, tempG, tempB);
    
    R[0] = tempR;
    G[0] = tempG;
    B[0] = tempB;

}

void csc_wrapper (
    DEF_CSC_MATRIX *csc_matrix,
    int *yuv,
    int *rgb,
    int height,
    int width,
    int format_yuv)
{

  int Y, U, V;
  int R, G, B;

  int upos = height*width;
  int vpos = (format_yuv==FMT_I420)?(upos+upos/4):(format_yuv==FMT_NV12)?(upos+1):(upos*2);
  int i = 0;


  if( (format_yuv==FMT_I420) || (format_yuv==FMT_NV12) ) {
    for (int y = 0; y<height; y++) {
  
      if (!(y % 2)) {
        for (int x = 0; x<width; x+=2) {
          Y = yuv[i++]    ;
          U = yuv[upos++] ;
          V = yuv[vpos++] ;
  
          csc(csc_matrix, Y, U, V, &R, &G, &B);

  
          rgb[y*width*3+x*3+2] = R;
          rgb[y*width*3+x*3+1] = G;
          rgb[y*width*3+x*3+0] = B;

          // only update Y
          Y = yuv[i++];
          csc(csc_matrix, Y, U, V, &R, &G, &B);


          rgb[y*width*3+(x+1)*3+2] = R;
          rgb[y*width*3+(x+1)*3+1] = G;
          rgb[y*width*3+(x+1)*3+0] = B;

          if(format_yuv==FMT_NV12){
            upos++; vpos++;
          }
  
          //printf("%d %d %d   %d %d %d\n", Y, U, V, R, G, B);
        }
      }
      else {
  
        int upos_tmp, vpos_tmp;
        upos_tmp = (format_yuv==FMT_I420)?(upos-width/2):(format_yuv==FMT_NV12)?(upos-width):(upos-width);
        vpos_tmp = (format_yuv==FMT_I420)?(vpos-width/2):(format_yuv==FMT_NV12)?(vpos-width):(vpos-width);
  
        for (int x = 0; x < width; x+=2) {
  
          Y = yuv[i++];
          U = yuv[upos_tmp++];
          V = yuv[vpos_tmp++];
  
          csc(csc_matrix, Y, U, V, &R, &G, &B);


          rgb[y*width*3+x*3+2] = R;
          rgb[y*width*3+x*3+1] = G;
          rgb[y*width*3+x*3+0] = B;

  
          // only update Y
          Y = yuv[i++];
          csc(csc_matrix, Y, U, V, &R, &G, &B);

  
          rgb[y*width*3+(x+1)*3+2] = R;
          rgb[y*width*3+(x+1)*3+1] = G;
          rgb[y*width*3+(x+1)*3+0] = B;

          if(format_yuv==FMT_NV12){
            upos_tmp++; vpos_tmp++;
          }
        }
      }
    }  
  }
  else { //RGB2YUV
    for (int y = 0; y<height; y++) {
      for (int x = 0; x<width; x++) {
	//buf yuv is rgb_i packed 
	Y = yuv[y*width*3+3*x+2];
	U = yuv[y*width*3+3*x+1];
	V = yuv[y*width*3+3*x+0];

        csc(csc_matrix, Y, U, V, &R, &G, &B);

        rgb[y*width*3+x*3+2] = R;
        rgb[y*width*3+x*3+1] = G;
        rgb[y*width*3+x*3+0] = B;

      }
    }
  }

}

void delete_stride(uint8 *stride_ch, uint8 *nonstride_ch0, int height, int width, uint32 stride)
{
  int i;

  for(i = 0; i < height; i++)
      memcpy((nonstride_ch0 + i *width),(stride_ch + i * stride), width);
  return;
}


void add_stride(uint8 *stride_ch, uint8 *nonstride_ch0, int height, int width, uint32 stride)
{
  int i;

  for(i = 0; i < height; i++)
      memcpy((stride_ch + i * stride), (nonstride_ch0 + i *width), width);

  return;
}
void write_output_bin_file(int format_out, bool A_swap_out, uint8 *video_out,
  uint8 *out_ch0, uint8 *out_ch1, uint8 *out_ch2,int height, int width, uint32 dst_stride, uint32 uv_stride)
{
  uint8 *file_out;
  int i;
  uint8 *ch0_tmp, *ch1_tmp, *ch2_tmp;

  ch0_tmp = (uint8 *)malloc(height * width * 4* sizeof(uint8));
  ch1_tmp = (uint8 *)malloc(height * width * sizeof(uint8));
  ch2_tmp = (uint8 *)malloc(height * width * sizeof(uint8));

  if( format_out==FMT_I420 ) {

    for(i = 0; i<height * width; i++)
      ch0_tmp[i] = video_out[i];

    for(i = 0; i < height * width /4; i++)
      ch1_tmp[i] = video_out[i+ height * width];

    for(i = 0; i < height  *width /4; i++)
      ch2_tmp[i] = video_out[i + height*width* 5 / 4];

    add_stride(out_ch0, ch0_tmp, height, width, dst_stride);
    add_stride(out_ch1, ch1_tmp, (height / 2), (width / 2), uv_stride);
    add_stride(out_ch2, ch2_tmp, (height / 2), (width / 2), uv_stride);
  }
  else if( format_out==FMT_YUV422P ) {

    for(i = 0; i<height*width; i++)
      ch0_tmp[i] = video_out[i];

    for(i = 0; i < height*width /2; i++)
      ch1_tmp[i] = video_out[i + height*width];

    for(i = 0; i < height*width /2; i++)
      ch2_tmp[i] = video_out[i + height*width* 3 / 2];

    add_stride(out_ch0, ch0_tmp, height, width, dst_stride);
    add_stride(out_ch1, ch1_tmp, height, (width / 2), uv_stride);
    add_stride(out_ch2, ch2_tmp, height , (width / 2), uv_stride);

  }
  else if( format_out==FMT_Y ) {
    for(i = 0; i<height*width; i++)
      ch0_tmp[i] = video_out[i];

    add_stride(out_ch0, ch0_tmp, height, width, dst_stride);
  } else if( format_out==FMT_RGBP ) { //RGBP

    file_out = (uint8 *)malloc(height*width*3 * sizeof(uint8));
    for(int c=0; c<=2; c++) {
      for(i=0; i<height*width; i++)
        file_out[height*width*c+i] = video_out[i*3+2-c];
    }

    for(i = 0; i < height*width; i++)
      ch0_tmp[i] = file_out[i];

    for(i = 0; i < height * width; i++)
      ch1_tmp[i] = file_out[i + height * width];

    for(i = 0; i < height * width; i++)
      ch2_tmp[i] = file_out[i + height * width * 2];

    add_stride(out_ch0, ch0_tmp, height, width, dst_stride);
    add_stride(out_ch1, ch1_tmp, height, width, dst_stride);
    add_stride(out_ch2, ch2_tmp, height, width, dst_stride);
    free(file_out);
  } else if( format_out==FMT_BGR ||
             format_out==FMT_RGB ) {

    if( format_out==FMT_BGR ) {
      for(int i=0; i<height*width; i++) {
        ch0_tmp[i*3+0] = video_out[i*3+0];
        ch0_tmp[i*3+1] = video_out[i*3+1];
        ch0_tmp[i*3+2] = video_out[i*3+2];
      }
    } else {
      for(int i=0; i<height*width; i++) {
        ch0_tmp[i*3+0] = video_out[i*3+2];
        ch0_tmp[i*3+1] = video_out[i*3+1];
        ch0_tmp[i*3+2] = video_out[i*3+0];
      }
    }

    add_stride(out_ch0, ch0_tmp, height, (width*3), dst_stride);

  } else if( format_out==FMT_ABGR ||
             format_out==FMT_ARGB ) {
     if( format_out==FMT_ARGB ) {
      if(A_swap_out) {
        for(int i=0; i<height*width; i++) {
          ch0_tmp[i*4+0] = video_out[i*3+2];
          ch0_tmp[i*4+1] = video_out[i*3+1];
          ch0_tmp[i*4+2] = video_out[i*3+0];
          ch0_tmp[i*4+3] = 0;
        }
      } else {
        for(int i=0; i<height*width; i++) {
          ch0_tmp[i*4+0] = 0;
          ch0_tmp[i*4+1] = video_out[i*3+2];
          ch0_tmp[i*4+2] = video_out[i*3+1];
          ch0_tmp[i*4+3] = video_out[i*3+0];
        }
      }
    } else {
      if(A_swap_out) {
        for(int i=0; i<height*width; i++) {
          ch0_tmp[i*4+0] = video_out[i*3+0];
          ch0_tmp[i*4+1] = video_out[i*3+1];
          ch0_tmp[i*4+2] = video_out[i*3+2];
          ch0_tmp[i*4+3] = 0;
        }
      } else {
        for(int i=0; i<height*width; i++) {
          ch0_tmp[i*4+0] = 0;
          ch0_tmp[i*4+1] = video_out[i*3+0];
          ch0_tmp[i*4+2] = video_out[i*3+1];
          ch0_tmp[i*4+3] = video_out[i*3+2];
        }
      }
    }
    add_stride(out_ch0, ch0_tmp, height, (width*4), dst_stride);

  } else {
    printf("ERROR: Invalid output binary file format\n");
  }

  free(ch0_tmp);
  free(ch1_tmp);
  free(ch2_tmp);

  return;
}

void write_input_bin_file(int format_in, int32 *video_in,int A_swap_in,
     uint8 *in_ch0, uint8 *in_ch1, uint8 *in_ch2, int height, int width, uint32 stride,uint32 uv_stride)
{
  uint8 *ch0_tmp, *ch1_tmp, *ch2_tmp;

  ch0_tmp = (uint8 *)malloc(height * width * 4 * sizeof(uint8));
  ch1_tmp = (uint8 *)malloc(height * width * sizeof(uint8));
  ch2_tmp = (uint8 *)malloc(height * width * sizeof(uint8));

  if( format_in==FMT_I420 ||
      format_in==FMT_NV12 ||
      format_in==FMT_Y ) {

    delete_stride(in_ch0, ch0_tmp, height, width, stride);

    for(int i=0; i<height; i++) {
      for(int j=0; j<width; j++) {
        video_in[i*width+j] = ch0_tmp[i*width+j] << SHIFT;
      }
    }

    if(format_in==FMT_I420) {
      int ofs = height*width;

      delete_stride(in_ch1, ch1_tmp, (height/2), (width/2), uv_stride);
      delete_stride(in_ch2, ch2_tmp, (height/2), (width/2), uv_stride);


      for(int i=0; i<height/2; i++) {
        for(int j=0; j<width/2; j++) {
          video_in[ofs+i*width/2+j] = ch1_tmp[i*width/2+j] << SHIFT;
        }
      }
      ofs += (height*width/4);
      for(int i=0; i<height/2; i++) {
        for(int j=0; j<width/2; j++) {
          video_in[ofs+i*width/2+j] = ch2_tmp[i*width/2+j] << SHIFT;
        }
      }

    } else if(format_in==FMT_NV12) {
      int ofs = height*width;

      delete_stride(in_ch1, ch1_tmp, (height/2), width, uv_stride);

      for(int i=0; i<height/2; i++) {
        for(int j=0; j<width; j++) {
          video_in[ofs+i*width+j] = ch1_tmp[i*width+j] << SHIFT;
        }
      }
    }
  } else if( format_in==FMT_RGBP ) {

    delete_stride(in_ch0, ch0_tmp, height, width, stride);
    delete_stride(in_ch1, ch1_tmp, height, width, stride);
    delete_stride(in_ch2, ch2_tmp, height, width, stride);

    for(int i=0; i<height; i++) {
      for(int j=0; j<width; j++) {
        video_in[i*width*3+j*3+2] = ch0_tmp[i*width+j];
        video_in[i*width*3+j*3+1] = ch1_tmp[i*width+j];
        video_in[i*width*3+j*3+0] = ch2_tmp[i*width+j];
      }
    }

  } else if( format_in==FMT_BGR ||
             format_in==FMT_RGB ) {

    delete_stride(in_ch0, ch0_tmp, height, (width*3), stride);

    for(int i=0; i<height; i++) {
      for(int j=0; j<width; j++) {
        if( format_in==FMT_BGR ) {
          video_in[i*width*3+j*3+2] = ch0_tmp[i*width*3+j*3+2];
          video_in[i*width*3+j*3+1] = ch0_tmp[i*width*3+j*3+1];
          video_in[i*width*3+j*3+0] = ch0_tmp[i*width*3+j*3+0];
        } else {
          video_in[i*width*3+j*3+2] = ch0_tmp[i*width*3+j*3+0];
          video_in[i*width*3+j*3+1] = ch0_tmp[i*width*3+j*3+1];
          video_in[i*width*3+j*3+0] = ch0_tmp[i*width*3+j*3+2];
        }
      }
    }
  } else if( format_in==FMT_ABGR ||
             format_in==FMT_ARGB ) {

    delete_stride(in_ch0, ch0_tmp, height, (width*4), stride);

    for(int i=0; i<height; i++) {
      for(int j=0; j<width; j++) {
        if( format_in==FMT_ARGB ) {
          if(A_swap_in) {
            video_in[i*width*3+j*3+2] = ch0_tmp[i*width*4+j*4+0];
            video_in[i*width*3+j*3+1] = ch0_tmp[i*width*4+j*4+1];
            video_in[i*width*3+j*3+0] = ch0_tmp[i*width*4+j*4+2];
          } else {
            video_in[i*width*3+j*3+2] = ch0_tmp[i*width*4+j*4+1];
            video_in[i*width*3+j*3+1] = ch0_tmp[i*width*4+j*4+2];
            video_in[i*width*3+j*3+0] = ch0_tmp[i*width*4+j*4+3];
          }
        } else {
          if(A_swap_in) {
            video_in[i*width*3+j*3+2] = ch0_tmp[i*width*4+j*4+2];
            video_in[i*width*3+j*3+1] = ch0_tmp[i*width*4+j*4+1];
            video_in[i*width*3+j*3+0] = ch0_tmp[i*width*4+j*4+0];
          } else {
            video_in[i*width*3+j*3+2] = ch0_tmp[i*width*4+j*4+3];
            video_in[i*width*3+j*3+1] = ch0_tmp[i*width*4+j*4+2];
            video_in[i*width*3+j*3+0] = ch0_tmp[i*width*4+j*4+1];
          }
        }
      }
    }

  } else {
    printf("ERROR: Invalid input binary file format\n");
  }

  free(ch0_tmp);
  free(ch1_tmp);
  free(ch2_tmp);
}

void vpp_top_1684 (
    VPP1684_PARAM *p_param          ,
    int           index             ,
    int           format_in         ,
    int           format_out        ,
    int32         *video_in         ,
    uint8         *video_out        ,
    int           height            ,
    int           width             ,
    int           csc_mode,
    int           csc_matrix_list[CSC_MAX][12]
)
{
  int sca_ctrl=0;
  const int src_st_x   = p_param->SRC_CROP_ST[index] >> 16;
  const int src_st_y   = p_param->SRC_CROP_ST[index] & 0xffff;
  const int src_crop_w = p_param->SRC_CROP_SIZE[index] >> 16;
  const int src_crop_h = p_param->SRC_CROP_SIZE[index] & 0xffff;
  const int dst_st_x   = p_param->DST_CROP_ST[index] >> 16;
  const int dst_st_y   = p_param->DST_CROP_ST[index] & 0xffff;
  const int dst_crop_w = p_param->DST_CROP_SIZE[index] >> 16;
  const int dst_crop_h = p_param->DST_CROP_SIZE[index] & 0xffff;

  const int hor_filt_sel      = (p_param->SCL_CTRL[index] >>  8) & 0x000f;
  const int ver_filt_sel      = (p_param->SCL_CTRL[index] >> 12) & 0x000f;
  const int hor_decimation    = (p_param->SCL_CTRL[index] >>  4) & 0x0003;
  int scl_bil_edge_mode;
  int algo_sel=0;
  int nearest_mode=0;
 
 if (hor_filt_sel == 0x0005){
   sca_ctrl = 0;       //Bilinear 16384  center
   algo_sel=0x02;
   scl_bil_edge_mode = 1; 
  }
 else if (hor_filt_sel == 0x0006){ 
   sca_ctrl = 0;  
   algo_sel=0x02; //blinear no center
   scl_bil_edge_mode = 0;
  }
  else if (hor_filt_sel == 0x0007){//Nearest floor
   sca_ctrl = 0;
   algo_sel=0x03;
   scl_bil_edge_mode = 0;
   nearest_mode=0;
  }
  else if (hor_filt_sel == 0x0008){  //Nearest round
   sca_ctrl = 0;
   algo_sel=0x03;
   scl_bil_edge_mode = 0;
   nearest_mode=1;
  }
  else {
   scl_bil_edge_mode = 0;
   sca_ctrl = 1; 
  }

  float hor_filt_init         = (p_param->SCL_INIT[index]      ) & 0x3fff;
  float ver_filt_init         = (p_param->SCL_INIT[index] >> 16) & 0x3fff;


  float scale_h = (p_param->SCL_Y[index] >> 14) & 0x000000ff;
  float scale_w = (p_param->SCL_X[index] >> 14) & 0x000000ff;
  scale_h += (float)(p_param->SCL_Y[index] & 0x00003fff) / (1<<14);
  scale_w += (float)(p_param->SCL_X[index] & 0x00003fff) / (1<<14);

  int src_csc_en =(p_param->SRC_CTRL & 0x800) >> 11;

  if(csc_mode != CSC_USER_DEFINED) {
    p_param->CSC_MATRIX.CSC_COE00 = csc_matrix_list[csc_mode][0]  ; //& 0x1fff   ;
    p_param->CSC_MATRIX.CSC_COE01 = csc_matrix_list[csc_mode][1]  ; //& 0x1fff   ;
    p_param->CSC_MATRIX.CSC_COE02 = csc_matrix_list[csc_mode][2]  ; //& 0x1fff   ;
    p_param->CSC_MATRIX.CSC_ADD0  = csc_matrix_list[csc_mode][3]  ; //& 0x1fffff ;
  
    p_param->CSC_MATRIX.CSC_COE10 = csc_matrix_list[csc_mode][4]  ; //& 0x1fff   ;
    p_param->CSC_MATRIX.CSC_COE11 = csc_matrix_list[csc_mode][5]  ; //& 0x1fff   ;
    p_param->CSC_MATRIX.CSC_COE12 = csc_matrix_list[csc_mode][6]  ; //& 0x1fff   ;
    p_param->CSC_MATRIX.CSC_ADD1  = csc_matrix_list[csc_mode][7]  ; //& 0x1fffff ;
  
    p_param->CSC_MATRIX.CSC_COE20 = csc_matrix_list[csc_mode][8]  ; //& 0x1fff   ;
    p_param->CSC_MATRIX.CSC_COE21 = csc_matrix_list[csc_mode][9]  ; //& 0x1fff   ;
    p_param->CSC_MATRIX.CSC_COE22 = csc_matrix_list[csc_mode][10] ; //& 0x1fff   ;
    p_param->CSC_MATRIX.CSC_ADD2  = csc_matrix_list[csc_mode][11] ; //& 0x1fffff ;
  }

  if(round(dst_crop_w*scale_w) != src_crop_w) {
    printf("ERROR: The scale_w ratio is not consistent with crop size x\n");
    printf("Param: src_w / dst_w = %d / %d != %03f\n", src_crop_w, dst_crop_w, scale_w);
    exit(-1);
  }
  if(round(dst_crop_h*scale_h) != src_crop_h) {
    printf("ERROR: The scale_h ratio is not consistent with crop size y\n");
    printf("Param: src_h / dst_h = %d / %d != %03f\n", src_crop_h, dst_crop_h, scale_h);
    exit(-1);
  }

  int32 *rgb_i, *rgb_o;

  rgb_i = (int32 *)malloc(src_crop_h*src_crop_w*3 * sizeof(int32));
  rgb_o = (int32 *)malloc(dst_crop_h*dst_crop_w*3 * sizeof(int32));

  memset(rgb_i, 0, (sizeof(int32)*(src_crop_h*src_crop_w*3)) );
  memset(rgb_o, 0, (sizeof(int32)*(dst_crop_h*dst_crop_w*3)) );


  int32 *y_i, *u_i, *v_i;
  int32 *y_o, *u_o, *v_o;
  y_i = (int32 *)malloc(src_crop_h*src_crop_w*3 * sizeof(int32));
  u_i = (int32 *)malloc(src_crop_h*src_crop_w*3 * sizeof(int32));
  v_i = (int32 *)malloc(src_crop_h*src_crop_w*3 * sizeof(int32));
  y_o = (int32 *)malloc(dst_crop_h*dst_crop_w*3 * sizeof(int32));

  u_o = (int32 *)malloc(dst_crop_h*dst_crop_w*3 * sizeof(int32));
  v_o = (int32 *)malloc(dst_crop_h*dst_crop_w*3 * sizeof(int32));

  int proc_st_y = (src_st_y%2==0)?src_st_y:src_st_y-1;
  int proc_st_x = (src_st_x%2==0)?src_st_x:src_st_x-1;
  int proc_h, proc_w;

  // -----crop and CSC for YUV input-----
  if( format_in==FMT_I420 ||
      format_in==FMT_NV12 ||
      format_in==FMT_Y ) {

    int32 *yuv_proc, *rgb_proc;

    // -----extend processing size-----
    proc_h = (src_st_y%2==0)?(src_crop_h%2==0)?src_crop_h:src_crop_h+1:(src_crop_h%2==0)?src_crop_h+2:src_crop_h+1;
    proc_w = (src_st_x%2==0)?(src_crop_w%2==0)?src_crop_w:src_crop_w+1:(src_crop_w%2==0)?src_crop_w+2:src_crop_w+1;
    yuv_proc = (int32 *)malloc(proc_h*proc_w*3/2 * sizeof(int32));
    rgb_proc = (int32 *)malloc(proc_h*proc_w*3 * sizeof(int32));

    memset(yuv_proc, 0, (sizeof(int32)*(proc_h*proc_w*3/2)) );
    memset(rgb_proc, 0, (sizeof(int32)*(proc_h*proc_w*3  )) );
    
    if(proc_h%2!=0 || proc_w%2!=0){
      printf("ERROR: Invalid YUV process size\n");
      exit(-1);
    }

    //--------------------------------------------------------------------------
    // -----crop Y data-----
    //--------------------------------------------------------------------------
    proc_st_y = (src_st_y%2==0)?src_st_y:src_st_y-1;
    proc_st_x = (src_st_x%2==0)?src_st_x:src_st_x-1;

    for(int i=0; i<proc_h; i++) {
      for(int j=0; j<proc_w; j++) {
        yuv_proc[i*proc_w+j] = (video_in[(i+proc_st_y)*width+j+proc_st_x] >> SHIFT) << SHIFT;
      }
    }

    int ofs_proc = proc_h*proc_w;
    int ofs_in   = height*width;

    //--------------------------------------------------------------------------
    // -----crop UV data-----
    //--------------------------------------------------------------------------

    if( format_in==FMT_I420 ) {
      // U data
      for(int i=0; i<proc_h/2; i++) {
        for(int j=0; j<proc_w/2; j++) {
          yuv_proc[ofs_proc+i*proc_w/2+j] = (video_in[ofs_in+(i+(proc_st_y>>1))*width/2+j+(proc_st_x>>1)] >> SHIFT) << SHIFT;
        }
      }

      ofs_proc += (proc_h*proc_w/4);
      ofs_in += (height*width/4);

      // V data
      for(int i=0; i<proc_h/2; i++) {
        for(int j=0; j<proc_w/2; j++) {
          yuv_proc[ofs_proc+i*proc_w/2+j] = (video_in[ofs_in+(i+(proc_st_y>>1))*width/2+j+(proc_st_x>>1)] >> SHIFT) << SHIFT;
        }
      }
    //--------------------------------------------------------------------------
    } else if( format_in==FMT_NV12 ) {

      for(int i=0; i<proc_h/2; i++) {
        for(int j=0; j<proc_w; j++) {
          yuv_proc[ofs_proc+i*proc_w+j] = (video_in[ofs_in+(i+(proc_st_y>>1))*width+j+proc_st_x] >> SHIFT) << SHIFT;
        }
      }
    }

    //--------------------------------------------------------------------------
    // CSC
    //--------------------------------------------------------------------------

    if(format_out==FMT_I420) {

      if( src_st_y%2!=0 || src_st_x%2!=0 || src_crop_h%2!=0 || src_crop_w%2!=0 ||
          dst_st_y%2!=0 || dst_st_x%2!=0 || dst_crop_h%2!=0 || dst_crop_w%2!=0 ) {
        printf("ERROR: Invalid YUV planar output format\n");
        printf("ERROR: SRC's & DST's start point & crop size should be EVEN for YUV420 in&out!!\n");
    	exit(-1);
      }

      //------------------------------------------------------------------------
      ofs_proc = proc_h*proc_w ;
      ofs_in   = height*width  ;
      int ofs_crop = src_crop_h*src_crop_w;
      int ofs_i = (src_st_y%2==0)?0:1;
      int ofs_j = (src_st_x%2==0)?0:1;

      //------------------------------------------------------------------------
      // Y data
      for(int i=0; i<src_crop_h; i++) {
        for(int j=0; j<src_crop_w; j++) {
          if(sca_ctrl==0) {
             y_i[i*src_crop_w+j+0] = yuv_proc[(ofs_i+i)*proc_w+(ofs_j+j)] >> SHIFT;
          }
          else {
            y_i[i*src_crop_w*3+j*3+0] = yuv_proc[(ofs_i+i)*proc_w+(ofs_j+j)] >> SHIFT;
            y_i[i*src_crop_w*3+j*3+1] = 0;
            y_i[i*src_crop_w*3+j*3+2] = 0;
          }
        }
      }

      //------------------------------------------------------------------------
      if( format_in==FMT_I420 ) {
        // U data
        for(int i=0; i<src_crop_h/2; i++) {
          for(int j=0; j<src_crop_w/2; j++) {
            if(sca_ctrl==0) {
              u_i[i*src_crop_w/2+j+0] = video_in[ofs_in+(i+(src_st_y>>1))*width/2+j+(src_st_x>>1)] >> SHIFT;
             } else {
              u_i[i*src_crop_w/2*3+j*3+0] = video_in[ofs_in+(i+(src_st_y>>1))*width/2+j+(src_st_x>>1)] >> SHIFT;
              u_i[i*src_crop_w/2*3+j*3+1] = 0;
              u_i[i*src_crop_w/2*3+j*3+2] = 0;
             }
          }
        }

        ofs_crop += (src_crop_h*src_crop_w/4);
        ofs_in += (height*width/4);

//----------------------------------------------------------------------
        // V data
        for(int i=0; i<src_crop_h/2; i++) {
          for(int j=0; j<src_crop_w/2; j++) {
              if(sca_ctrl==0) {
                v_i[i*src_crop_w/2+j+0] = video_in[ofs_in+(i+(src_st_y>>1))*width/2+j+(src_st_x>>1)] >> SHIFT;
              } else {
                v_i[i*src_crop_w/2*3+j*3+0] = video_in[ofs_in+(i+(src_st_y>>1))*width/2+j+(src_st_x>>1)] >> SHIFT;
                v_i[i*src_crop_w/2*3+j*3+1] = 0;
                v_i[i*src_crop_w/2*3+j*3+2] = 0;
              }
          }
        }
      } else if(format_in == FMT_NV12) {
        // U data
        for(int i=0; i<src_crop_h/2; i++) {
          for(int j=0; j<src_crop_w; j+=2) {
            if(sca_ctrl==0) {
              u_i[i*src_crop_w/2+(j>>1)+0] = video_in[ofs_in+(i+(src_st_y>>1))*width+j+src_st_x] >> SHIFT;
            } else {
              u_i[i*src_crop_w/2*3+(j>>1)*3+0] = video_in[ofs_in+(i+(src_st_y>>1))*width+j+src_st_x] >> SHIFT;
              u_i[i*src_crop_w/2*3+(j>>1)*3+1] = 0;
              u_i[i*src_crop_w/2*3+(j>>1)*3+2] = 0;
            }
          }
        }

        ofs_crop += (src_crop_h*src_crop_w/4);
        // V data
        for(int i=0; i<src_crop_h/2; i++) {
          for(int j=1; j<src_crop_w; j+=2) {
			 if(sca_ctrl==0)
			 {  v_i[i*src_crop_w/2+(j>>1)+0] = video_in[ofs_in+(i+(src_st_y>>1))*width+j+src_st_x] >> SHIFT;
			 }
			 else
			 {  v_i[i*src_crop_w/2*3+(j>>1)*3+0] = video_in[ofs_in+(i+(src_st_y>>1))*width+j+src_st_x] >> SHIFT;
			    v_i[i*src_crop_w/2*3+(j>>1)*3+1] = 0;
             	v_i[i*src_crop_w/2*3+(j>>1)*3+2] = 0;
             }
          }
        }
      }

    }
    //--------------------------------------------------------------------------
    else if(format_out==FMT_Y) {

      int ofs_i = (src_st_y%2==0)?0:1;
      int ofs_j = (src_st_x%2==0)?0:1;

      for(int i=0; i<src_crop_h; i++) {
        for(int j=0; j<src_crop_w; j++) {

        
            
           if(sca_ctrl==0)
            {
			 	y_i[i*src_crop_w+j+0] = yuv_proc[(ofs_i+i)*proc_w+(ofs_j+j)] >> SHIFT;
            }
			else
			{
				y_i[i*src_crop_w*3+j*3+0] = yuv_proc[(ofs_i+i)*proc_w+(ofs_j+j)] >> SHIFT;
				y_i[i*src_crop_w*3+j*3+1] = 0;
           		y_i[i*src_crop_w*3+j*3+2] = 0;
          	}
        }
      }
    //--------------------------------------------------------------------------
    } else {

      csc_wrapper(&p_param->CSC_MATRIX, yuv_proc, rgb_proc, proc_h, proc_w, format_in);

      int ofs_i = (src_st_y%2==0)?0:1;
      int ofs_j = (src_st_x%2==0)?0:1;      

      for(int i=0; i<src_crop_h; i++) {
        for(int j=0; j<src_crop_w; j++) {

        rgb_i[i*src_crop_w*3+j*3+2] = rgb_proc[(ofs_i+i)*proc_w*3+(ofs_j+j)*3+2] >> SHIFT;
        rgb_i[i*src_crop_w*3+j*3+1] = rgb_proc[(ofs_i+i)*proc_w*3+(ofs_j+j)*3+1] >> SHIFT;
        rgb_i[i*src_crop_w*3+j*3+0] = rgb_proc[(ofs_i+i)*proc_w*3+(ofs_j+j)*3+0] >> SHIFT;

        }
      }
    }

    free(yuv_proc);
    free(rgb_proc);
  }
  //----------------------------------------------------------------------------
  else if(format_in==FMT_RGBP  ||
          format_in==FMT_BGR  ||
          format_in==FMT_RGB ||
          format_in==FMT_ABGR  ||
          format_in==FMT_ARGB) {

    proc_h = src_crop_h;
    proc_w = src_crop_w;

  //  printf("CROP for RGB input\n");

    for(int i=0; i<src_crop_h; i++) {
      for(int j=0; j<src_crop_w; j++) {
        rgb_i[i*src_crop_w*3+j*3+2] = video_in[(i+src_st_y)*width*3+(j+src_st_x)*3+2]; //R
        rgb_i[i*src_crop_w*3+j*3+1] = video_in[(i+src_st_y)*width*3+(j+src_st_x)*3+1]; //G
        rgb_i[i*src_crop_w*3+j*3+0] = video_in[(i+src_st_y)*width*3+(j+src_st_x)*3+0]; //B
      }
    }

    if(src_csc_en == 1) {
      int32 *rgb_proc;
      rgb_proc = (int32 *)malloc(sizeof(int32) * proc_h * proc_w*3);

      memset(rgb_proc, 0, (sizeof(int32)*(proc_h*proc_w*3)) );

   //   printf("CSC: RGB2YUV\n");
      csc_wrapper(&p_param->CSC_MATRIX, rgb_i, rgb_proc, proc_h, proc_w, format_in); //rgb_proc is yuv444

      //copy back to rgb_i
      for(int i=0; i<src_crop_h; i++) {
	   for(int j=0; j<src_crop_w; j++) {
          rgb_i[i*src_crop_w*3+j*3+2] = rgb_proc[i*src_crop_w*3+j*3+2];
          rgb_i[i*src_crop_w*3+j*3+1] = rgb_proc[i*src_crop_w*3+j*3+1];
          rgb_i[i*src_crop_w*3+j*3+0] = rgb_proc[i*src_crop_w*3+j*3+0];
	   }
      }

      free(rgb_proc);
    }

  //----------------------------------------------------------------------------
  } else {
    printf("ERROR: Invalid input format\n");
    exit(-1);
  }

  //----------------------------------------------------------------------------
  //scale
  //----------------------------------------------------------------------------
  if(sca_ctrl==0) {

    // -----Scaling-----1684
    if(format_out==FMT_I420) {

        if((format_in ==FMT_RGB) || (format_in ==FMT_RGBP) || (format_in ==FMT_BGR)){
        for(int i=0; i<src_crop_h; i++) {
          for(int j=0; j<src_crop_w; j++) {
            y_i[i*src_crop_w+j+0] = rgb_i[i*src_crop_w*3+j*3+2];
            y_i[i*src_crop_w+j+1] = 0;
            y_i[i*src_crop_w+j+2] = 0;

            u_i[i*src_crop_w+j+0] = rgb_i[i*src_crop_w*3+j*3+1];
            u_i[i*src_crop_w+j+1] = 0;
            u_i[i*src_crop_w+j+2] = 0;

            v_i[i*src_crop_w+j+0] = rgb_i[i*src_crop_w*3+j*3+0];
            v_i[i*src_crop_w+j+1] = 0;
            v_i[i*src_crop_w+j+2] = 0;
          }
        }
      }


	  	scaling_top(1, 0, algo_sel,scl_bil_edge_mode,nearest_mode, y_i, y_o, dst_crop_h, dst_crop_w, scale_h, scale_w, p_param->SCL_Y[index], p_param->SCL_X[index]);

      if( (format_in == FMT_I420) || (format_in == FMT_NV12) ) {
//	printf("scale for YUV420 to YUV420SP\n"); 
       
	  	scaling_top(1, 0,algo_sel, scl_bil_edge_mode,nearest_mode, u_i, u_o, dst_crop_h/2, dst_crop_w/2, scale_h, scale_w, p_param->SCL_Y[index], p_param->SCL_X[index]);
		
		scaling_top(1, 0,algo_sel, scl_bil_edge_mode,nearest_mode, v_i, v_o, dst_crop_h/2, dst_crop_w/2, scale_h, scale_w, p_param->SCL_Y[index], p_param->SCL_X[index]);
			
      } else if((format_in ==FMT_RGB) || (format_in ==FMT_RGBP) || (format_in ==FMT_BGR)) {
//	printf("scale for RGB,YUV444P to YUV420SP\n");

		if(scale_w*2 == 1) {
        	p_param->SCL_X[index]  = 0x4000;
      	} else {
       	 p_param->SCL_X[index]  = (int)(scale_w*2) << 14;
         float scale_w_frac  = round((scale_w*2 - (int)(scale_w*2))*(1<<14));
         p_param->SCL_X[index] += (int)(scale_w_frac);
      	}

		if(scale_h*2 == 1) {
        	p_param->SCL_Y[index]  = 0x4000;
      	} else {
       	 p_param->SCL_Y[index]  = (int)(scale_h*2) << 14;
        	float scale_h_frac  = round((scale_h*2 - (int)(scale_h*2))*(1<<14));
       	 p_param->SCL_Y[index] += (int)(scale_h_frac);
      	}
	  
		scaling_top(1, 0,algo_sel, scl_bil_edge_mode,nearest_mode, u_i, u_o, dst_crop_h/2, dst_crop_w/2, scale_h*2, scale_w*2, p_param->SCL_Y[index], p_param->SCL_X[index]);
	
		scaling_top(1, 0,algo_sel,scl_bil_edge_mode,nearest_mode, v_i, v_o, dst_crop_h/2, dst_crop_w/2, scale_h*2, scale_w*2, p_param->SCL_Y[index], p_param->SCL_X[index]);
		
       }
	  

		for(int i=0; i<dst_crop_h; i++) {
		  for(int j=0; j<dst_crop_w; j++) {
			  rgb_o[i*dst_crop_w+j] = y_o[i*(dst_crop_w)+j];
		  }
		}
			
	
	  int ofs = dst_crop_h*dst_crop_w;
      for(int i=0; i<dst_crop_h/2; i++) {
        for(int j=0; j<dst_crop_w/2; j++) {
          rgb_o[ofs+i*dst_crop_w/2+j] = u_o[i*(dst_crop_w/2)+j]; 
        }
      }

      ofs += dst_crop_h*dst_crop_w/4;
      for(int i=0; i<dst_crop_h/2; i++) {
        for(int j=0; j<dst_crop_w/2; j++) {
          rgb_o[ofs+i*dst_crop_w/2+j] = v_o[i*(dst_crop_w/2)+j];
        }
      }
	
	  
    }
	else if(format_out==FMT_YUV422P) {

        if((format_in ==FMT_RGB) || (format_in ==FMT_RGBP) || (format_in ==FMT_BGR)){
        for(int i=0; i<src_crop_h; i++) {
          for(int j=0; j<src_crop_w; j++) {
            y_i[i*src_crop_w+j+0] = rgb_i[i*src_crop_w*3+j*3+2];
            y_i[i*src_crop_w+j+1] = 0;
            y_i[i*src_crop_w+j+2] = 0;

            u_i[i*src_crop_w+j+0] = rgb_i[i*src_crop_w*3+j*3+1];
            u_i[i*src_crop_w+j+1] = 0;
            u_i[i*src_crop_w+j+2] = 0;

            v_i[i*src_crop_w+j+0] = rgb_i[i*src_crop_w*3+j*3+0];
            v_i[i*src_crop_w+j+1] = 0;
            v_i[i*src_crop_w+j+2] = 0;
          }
        }
      }

	  	scaling_top(1, 0, algo_sel,scl_bil_edge_mode,nearest_mode, y_i, y_o, dst_crop_h, dst_crop_w, scale_h, scale_w, p_param->SCL_Y[index], p_param->SCL_X[index]);

      if( (format_in == FMT_I420) || (format_in == FMT_NV12) ) {
//	printf("scale for YUV420 to YUV422P,no support\n"); 
      } else if((format_in ==FMT_RGB) || (format_in ==FMT_RGBP) || (format_in ==FMT_BGR)){
//	printf("scale for RGB,YUV444P to YUV422P\n");
		if(scale_w*2 == 1) {
        	p_param->SCL_X[index]  = 0x4000;
      	} else {
       	 p_param->SCL_X[index]  = (int)(scale_w*2) << 14;
         float scale_w_frac  = round((scale_w*2 - (int)(scale_w*2))*(1<<14));
         p_param->SCL_X[index] += (int)(scale_w_frac);
      	}

		scaling_top(1, 0,algo_sel, scl_bil_edge_mode,nearest_mode, u_i, u_o, dst_crop_h, dst_crop_w/2, scale_h, scale_w*2, p_param->SCL_Y[index], p_param->SCL_X[index]);

		scaling_top(1, 0,algo_sel,scl_bil_edge_mode,nearest_mode, v_i, v_o, dst_crop_h, dst_crop_w/2, scale_h, scale_w*2, p_param->SCL_Y[index], p_param->SCL_X[index]);

       }
		for(int i=0; i<dst_crop_h; i++) {
		  for(int j=0; j<dst_crop_w; j++) {
			  rgb_o[i*dst_crop_w+j] = y_o[i*(dst_crop_w)+j];
		  }
		}
	  int ofs = dst_crop_h*dst_crop_w;
      for(int i=0; i<dst_crop_h; i++) {
        for(int j=0; j<dst_crop_w/2; j++) {
          rgb_o[ofs+i*dst_crop_w/2+j] = u_o[i*(dst_crop_w/2)+j]; 
        }
      }

      ofs += dst_crop_h*dst_crop_w/2;
      for(int i=0; i<dst_crop_h; i++) {
        for(int j=0; j<dst_crop_w/2; j++) {
          rgb_o[ofs+i*dst_crop_w/2+j] = v_o[i*(dst_crop_w/2)+j];
        }
      }
	
	  
    }
	  else if(format_out==FMT_Y) { //===============Y  ONLY========================//
//		  printf("format_out==FMT_Y\n");

	  scaling_top(1, 0, algo_sel,scl_bil_edge_mode,nearest_mode, y_i, y_o, dst_crop_h, dst_crop_w, scale_h, scale_w, p_param->SCL_Y[index], p_param->SCL_X[index]);

      for(int i=0; i<dst_crop_h; i++) {

        for(int j=0; j<dst_crop_w; j++) {
          rgb_o[i*dst_crop_w+j] = y_o[i*(dst_crop_w)+j];
        }
      }
    }else {  //===============RGB YUV444========================//
		
		  scaling_top(0, 0,algo_sel, scl_bil_edge_mode,nearest_mode, rgb_i, rgb_o, dst_crop_h, dst_crop_w, scale_h, scale_w, p_param->SCL_Y[index], p_param->SCL_X[index]);
	}

  }else {
    if(format_out==FMT_I420) {
        if((format_in ==FMT_RGB) || (format_in ==FMT_RGBP) || (format_in ==FMT_BGR)){
        for(int i=0; i<src_crop_h; i++) {
          for(int j=0; j<src_crop_w; j++) {
            y_i[i*src_crop_w*3+j*3+0] = rgb_i[i*src_crop_w*3+j*3+2];
            y_i[i*src_crop_w*3+j*3+1] = 0;
            y_i[i*src_crop_w*3+j*3+2] = 0;

            u_i[i*src_crop_w*3+j*3+0] = rgb_i[i*src_crop_w*3+j*3+1];
            u_i[i*src_crop_w*3+j*3+1] = 0;
            u_i[i*src_crop_w*3+j*3+2] = 0;

            v_i[i*src_crop_w*3+j*3+0] = rgb_i[i*src_crop_w*3+j*3+0];
            v_i[i*src_crop_w*3+j*3+1] = 0;
            v_i[i*src_crop_w*3+j*3+2] = 0;
          }
        }
      }

               scaling_top_1684(p_param, y_i, y_o, dst_crop_h, dst_crop_w, src_crop_h, src_crop_w, scale_h, scale_w, hor_filt_sel, ver_filt_sel, hor_filt_init, ver_filt_init, hor_decimation);

      if( (format_in == FMT_I420) || (format_in == FMT_NV12) ) {
//	printf("scale for YUV420 to YUV420SP\n"); 
        
              scaling_top_1684(p_param, u_i, u_o, dst_crop_h/2, dst_crop_w/2, src_crop_h/2, src_crop_w/2, scale_h, scale_w, hor_filt_sel, ver_filt_sel, hor_filt_init, ver_filt_init, hor_decimation);
              scaling_top_1684(p_param, v_i, v_o, dst_crop_h/2, dst_crop_w/2, src_crop_h/2, src_crop_w/2, scale_h, scale_w, hor_filt_sel, ver_filt_sel, hor_filt_init, ver_filt_init, hor_decimation);
        
      } else if((format_in ==FMT_RGB) || (format_in ==FMT_RGBP) || (format_in ==FMT_BGR)){
//	printf("scale for RGB,YUV444P to YUV420SP\n");
             
              scaling_top_1684(p_param, u_i, u_o, dst_crop_h/2, dst_crop_w/2, src_crop_h, src_crop_w, (scale_h*2), (scale_w*2), hor_filt_sel, ver_filt_sel, hor_filt_init, ver_filt_init, hor_decimation);
              scaling_top_1684(p_param, v_i, v_o, dst_crop_h/2, dst_crop_w/2, src_crop_h, src_crop_w, (scale_h*2), (scale_w*2), hor_filt_sel, ver_filt_sel, hor_filt_init, ver_filt_init, hor_decimation);
      }

      for(int i=0; i<dst_crop_h; i++) {
        for(int j=0; j<dst_crop_w; j++) {
          rgb_o[i*dst_crop_w+j] = y_o[i*(dst_crop_w*3)+j*3+0];
        }
      }

      int ofs = dst_crop_h*dst_crop_w;
      for(int i=0; i<dst_crop_h/2; i++) {
        for(int j=0; j<dst_crop_w/2; j++) {
          rgb_o[ofs+i*dst_crop_w/2+j] = u_o[i*(dst_crop_w/2*3)+j*3+0];
        }
      }

      ofs += dst_crop_h*dst_crop_w/4;
      for(int i=0; i<dst_crop_h/2; i++) {
        for(int j=0; j<dst_crop_w/2; j++) {
          rgb_o[ofs+i*dst_crop_w/2+j] = v_o[i*(dst_crop_w/2*3)+j*3+0];
        }
      }
    }
	  else if(format_out==FMT_YUV422P) {

        if((format_in ==FMT_RGB) || (format_in ==FMT_RGBP) || (format_in ==FMT_BGR)){
        for(int i=0; i<src_crop_h; i++) {
          for(int j=0; j<src_crop_w; j++) {
            y_i[i*src_crop_w*3+j*3+0] = rgb_i[i*src_crop_w*3+j*3+2];
            y_i[i*src_crop_w*3+j*3+1] = 0;
            y_i[i*src_crop_w*3+j*3+2] = 0;

            u_i[i*src_crop_w*3+j*3+0] = rgb_i[i*src_crop_w*3+j*3+1];
            u_i[i*src_crop_w*3+j*3+1] = 0;
            u_i[i*src_crop_w*3+j*3+2] = 0;

            v_i[i*src_crop_w*3+j*3+0] = rgb_i[i*src_crop_w*3+j*3+0];
            v_i[i*src_crop_w*3+j*3+1] = 0;
            v_i[i*src_crop_w*3+j*3+2] = 0;
          }
        }
      }

	  	 scaling_top_1684(p_param, y_i, y_o, dst_crop_h, dst_crop_w, src_crop_h, src_crop_w, scale_h, scale_w, hor_filt_sel, ver_filt_sel, hor_filt_init, ver_filt_init, hor_decimation);
      if( (format_in == FMT_I420) || (format_in == FMT_NV12) ) {
//	printf("scale for YUV420 to YUV422P,no support\n"); 
      } else if((format_in ==FMT_RGB) || (format_in ==FMT_RGBP) || (format_in ==FMT_BGR)){
//	printf("scale for RGB,YUV444P to YUV422P\n");

		scaling_top_1684(p_param, u_i, u_o, dst_crop_h, dst_crop_w/2, src_crop_h, src_crop_w, scale_h, (scale_w*2), hor_filt_sel, ver_filt_sel, hor_filt_init, ver_filt_init, hor_decimation);
        scaling_top_1684(p_param, v_i, v_o, dst_crop_h, dst_crop_w/2, src_crop_h, src_crop_w, scale_h, (scale_w*2), hor_filt_sel, ver_filt_sel, hor_filt_init, ver_filt_init, hor_decimation);
    }
	  

		for(int i=0; i<dst_crop_h; i++) {
		  for(int j=0; j<dst_crop_w; j++) {
			  rgb_o[i*dst_crop_w+j] = y_o[i*(dst_crop_w*3)+j*3+0];
		  }
		}
			
	
	  int ofs = dst_crop_h*dst_crop_w;
      for(int i=0; i<dst_crop_h; i++) {
        for(int j=0; j<dst_crop_w/2; j++) {
          rgb_o[ofs+i*dst_crop_w/2+j] = u_o[i*(dst_crop_w/2*3)+j*3+0];
        }
      }

      ofs += dst_crop_h*dst_crop_w/2;
      for(int i=0; i<dst_crop_h; i++) {
        for(int j=0; j<dst_crop_w/2; j++) {
          rgb_o[ofs+i*dst_crop_w/2+j] = v_o[i*(dst_crop_w/2*3)+j*3+0];
        }
      }
	
	  
    }
	  else if(format_out==FMT_Y) { //===============Y  ONLY========================//
            scaling_top_1684(p_param, y_i, y_o, dst_crop_h, dst_crop_w, src_crop_h, src_crop_w, scale_h, scale_w, hor_filt_sel, ver_filt_sel, hor_filt_init, ver_filt_init, hor_decimation);
        

      for(int i=0; i<dst_crop_h; i++) {

        for(int j=0; j<dst_crop_w; j++) {
          rgb_o[i*dst_crop_w+j] = y_o[i*(dst_crop_w*3)+j*3+0];
        }
      }
    }else {  //===============RGB YUV444========================//
          scaling_top_1684(p_param, rgb_i, rgb_o, dst_crop_h, dst_crop_w, src_crop_h, src_crop_w, scale_h, scale_w, hor_filt_sel, ver_filt_sel, hor_filt_init, ver_filt_init, hor_decimation);
    }

}//  scl_bil_enable

  for(int i=0; i<dst_crop_h*dst_crop_w*3; i++) {
    video_out[i] = rgb_o[i];
  }

  free(v_o);
  free(u_o);
  free(y_o);
  free(v_i);
  free(u_i);
  free(y_i);
  free(rgb_o);
  free(rgb_i);
}

int vpp1684_cmodel_test(struct vpp_batch_n *batch)
{
    uint8 *in_ch0, *in_ch1, *in_ch2;
    uint8 *out_ch0, *out_ch1, *out_ch2;
    int32 *video_in;
    uint8 *video_out;
    VPP1684_PARAM p_param;
    int format_in = 0, format_out = 0;
    uint32 srcUV, dstUV;
    const int crop_num = batch->num;
    uint32 dst_stride, src_stride;

    if(batch->cmd->src_uv_stride == 0) {
        srcUV = (((batch->cmd->src_format == YUV420) || (batch->cmd->src_format == YUV422)) && (batch->cmd->src_plannar == 1))
              ? (batch->cmd->src_stride / 2) : (batch->cmd->src_stride);
    } else {
        srcUV = batch->cmd->src_uv_stride;
    }
    if(batch->cmd->dst_uv_stride == 0) {
        dstUV = (((batch->cmd->dst_format == YUV420) || ((batch->cmd->dst_format == YUV422))) && (batch->cmd->dst_plannar == 1))
              ? (batch->cmd->dst_stride /2 ) : (batch->cmd->dst_stride);
    } else {
        dstUV = batch->cmd->dst_uv_stride;
    }

    memset(&p_param, 0, sizeof(VPP1684_PARAM));
    p_param.SRC_CROP_ST = (uint32*)malloc(sizeof(uint32) *crop_num);
    p_param.SRC_CROP_SIZE = (uint32*)malloc(sizeof(uint32) *crop_num);
    p_param.DST_CROP_ST = (uint32*)malloc(sizeof(uint32) *crop_num);
    p_param.DST_CROP_SIZE = (uint32*)malloc(sizeof(uint32) *crop_num);
    p_param.DST_RY_BASE = (uint32*)malloc(sizeof(uint32) *crop_num);
    p_param.DST_GU_BASE = (uint32*)malloc(sizeof(uint32) *crop_num);
    p_param.DST_BV_BASE = (uint32*)malloc(sizeof(uint32) *crop_num);
    p_param.DST_STRIDE = (uint32*)malloc(sizeof(uint32) *crop_num);
    p_param.DST_RY_BASE_EXT = (uint32*)malloc(sizeof(uint32) *crop_num);
    p_param.DST_GU_BASE_EXT = (uint32*)malloc(sizeof(uint32) *crop_num);
    p_param.DST_BV_BASE_EXT = (uint32*)malloc(sizeof(uint32) *crop_num);
    p_param.SCL_CTRL = (uint32*)malloc(sizeof(uint32) *crop_num);
    p_param.SCL_INIT = (uint32*)malloc(sizeof(uint32) *crop_num);
    p_param.SCL_X = (uint32*)malloc(sizeof(uint32) *crop_num);
    p_param.SCL_Y = (uint32*)malloc(sizeof(uint32) *crop_num);

    p_param.USER_DEF = (((1 & 0x00000001)<<2));
    p_param.PROJECT = 1684;
    p_param.CROP_NUM = batch->num;
    p_param.SCA_CTRL = 1;
    p_param.SRC_CTRL = ((batch->cmd->src_csc_en & 0x1) << 11) | ((batch->cmd->src_plannar & 0x1) << 8) |((batch->cmd->src_endian & 0x1) << 5) \
                      | ((batch->cmd->src_endian_a & 0x1) << 4) |(batch->cmd->src_format & 0xf);
    p_param.SRC_SIZE = ((batch->cmd->cols & 0xffff) << 16) | (batch->cmd->rows & 0xffff);
    p_param.SRC_CROP_ST[0] = ((batch->cmd->src_axisX & 0xfffff) << 16) | (batch->cmd->src_axisY  & 0xffff);
    p_param.SRC_CROP_SIZE[0] = ((batch->cmd->src_cropW & 0xfffff) << 16) | (batch->cmd->src_cropH & 0xffff);
    p_param.SRC_STRIDE = (batch->cmd->src_stride << 16) | (srcUV & 0xffff);

    p_param.SRC_RY_BASE = (batch->cmd->src_addr0 & 0xffffffff);
    p_param.SRC_RY_BASE_EXT = (batch->cmd->src_addr0 >> 32) & 0x7;
    p_param.SRC_GU_BASE = (batch->cmd->src_addr1 & 0xffffffff);
    p_param.SRC_GU_BASE_EXT = (batch->cmd->src_addr1 >> 32) & 0x7;
    p_param.SRC_BV_BASE = (batch->cmd->src_addr2 & 0xffffffff);
    p_param.SRC_BV_BASE_EXT = (batch->cmd->src_addr0 >> 32) & 0x7;

    p_param.DST_CTRL = ((batch->cmd->dst_plannar & 0x1) << 8) |((batch->cmd->dst_endian  & 0x1) << 5) \
                                      | ((batch->cmd->dst_endian_a & 0x1) << 4) |(batch->cmd->dst_format & 0xf) | ((0 & 0xff) <<16);

    p_param.DST_CROP_ST[0] = ((batch->cmd->dst_axisX & 0xfffff) << 16) | (batch->cmd->dst_axisY & 0xffff);
    p_param.DST_CROP_SIZE[0] = ((batch->cmd->dst_cropW & 0xfffff) << 16) | (batch->cmd->dst_cropH & 0xffff);
    p_param.DST_STRIDE[0] = (batch->cmd->dst_stride  << 16) | (dstUV & 0xffff);

    p_param.DST_RY_BASE[0] = (batch->cmd->dst_addr0 & 0xffffffff);
    p_param.DST_RY_BASE_EXT[0] = (batch->cmd->dst_addr0 >> 32) & 0x7;
    p_param.DST_GU_BASE[0] = (batch->cmd->dst_addr1 & 0xffffffff);
    p_param.DST_GU_BASE_EXT[0] = (batch->cmd->dst_addr1 >> 32) & 0x7;
    p_param.DST_BV_BASE[0] = (batch->cmd->dst_addr2 & 0xffffffff);
    p_param.DST_BV_BASE_EXT[0] = (batch->cmd->dst_addr2 >> 32) & 0x7;

    p_param.SCL_CTRL[0] = ((batch->cmd->ver_filter_sel & 0xf) << 12) | ((batch->cmd->hor_filter_sel & 0xf) << 8) | (1 & 0x1);
    p_param.SCL_INIT[0] = ((batch->cmd->scale_y_init & 0x3fff) << 16) | (batch->cmd->scale_x_init & 0x3fff);

    p_param.SCL_X[0] = floor(batch->cmd->src_cropW * 16384 / batch->cmd->dst_cropW);
    p_param.SCL_Y[0] = floor(batch->cmd->src_cropH * 16384 / batch->cmd->dst_cropH);
    p_param.PADDING_CTRL = 0x00808080;
    p_param.DST_SIZE = 0x0;

    if(batch->cmd->csc_type == CSC_USER_DEFINED) {
        p_param.CSC_MATRIX.CSC_COE00 = batch->cmd->matrix.csc_coe00; //& 0x1fff   ;
        p_param.CSC_MATRIX.CSC_COE01 = batch->cmd->matrix.csc_coe01; //& 0x1fff   ;
        p_param.CSC_MATRIX.CSC_COE02 = batch->cmd->matrix.csc_coe02; //& 0x1fff   ;
        p_param.CSC_MATRIX.CSC_ADD0  = batch->cmd->matrix.csc_add0; //& 0x1fffff ;

        p_param.CSC_MATRIX.CSC_COE10 = batch->cmd->matrix.csc_coe10; //& 0x1fff   ;
        p_param.CSC_MATRIX.CSC_COE11 = batch->cmd->matrix.csc_coe11; //& 0x1fff   ;
        p_param.CSC_MATRIX.CSC_COE12 = batch->cmd->matrix.csc_coe12; //& 0x1fff   ;
        p_param.CSC_MATRIX.CSC_ADD1  = batch->cmd->matrix.csc_add1; //& 0x1fffff ;

        p_param.CSC_MATRIX.CSC_COE20 = batch->cmd->matrix.csc_coe20; //& 0x1fff   ;
        p_param.CSC_MATRIX.CSC_COE21 = batch->cmd->matrix.csc_coe21; //& 0x1fff   ;
        p_param.CSC_MATRIX.CSC_COE22 = batch->cmd->matrix.csc_coe22; //& 0x1fff   ;
        p_param.CSC_MATRIX.CSC_ADD2  = batch->cmd->matrix.csc_add2; //& 0x1fffff ;
    }
  // -----decode needed parameters-----
  const int  in_format        =  p_param.SRC_CTRL & 0x000f;
  const bool in_endian_mode   = (p_param.SRC_CTRL >> 5) & 0x0001;
  const bool in_planar        = (p_param.SRC_CTRL >> 8) & 0x0001;
  const bool in_endian_mode_A = (p_param.SRC_CTRL >> 4) & 0x0001;
  const bool A_swap_in        = (in_endian_mode_A) ? true : false;

  dst_stride = batch->cmd->dst_stride;
  src_stride = batch->cmd->src_stride;

  if(((p_param.SRC_CTRL >> 9) & 0x0003) != 0) {
    printf("[CFG_WRONG] v6 no boda\n");
    p_param.SRC_CTRL &=0xfffff9ff;
  }

  switch(in_format) {
    case YUV420:
      if(in_planar) {
        format_in = FMT_I420;
      } else {
        format_in = FMT_NV12;
      }
      break;

    case YOnly:
      format_in = FMT_Y;
      break;

    case RGB24:
      if(in_planar) {
        format_in = FMT_RGBP;
      } else {
        if(in_endian_mode) {
          format_in = FMT_BGR;
        } else {
          format_in = FMT_RGB;
        }
      }
      break;
    case ARGB32:
        if(in_endian_mode) {
          format_in = FMT_ABGR;
        } else {
          format_in = FMT_ARGB;
        }
      break;

  }

  const int  out_format        =  p_param.DST_CTRL & 0x000f;
  const bool out_endian_mode   = (p_param.DST_CTRL >> 5) & 0x0001;
  const bool out_planar        = (p_param.DST_CTRL >> 8) & 0x0001;

  const bool out_endian_mode_A = (p_param.DST_CTRL >> 4) & 0x0001;
  const bool A_swap_out        = (out_endian_mode_A)?true:false;

  switch(out_format) {
    case YUV420:
      if(out_planar) {
        format_out = FMT_I420;
      } else {
        printf("ERROR: Invalid output format\n");
        return 0;
      }
      break;

    case YOnly:
      format_out = FMT_Y;
      break;

    case RGB24:
      if(out_planar) {
        format_out = FMT_RGBP;
      } else {
        if(out_endian_mode) {
          format_out = FMT_BGR;
        } else {
          format_out = FMT_RGB;
        }
      }
      break;
    case ARGB32:
      if(out_endian_mode) {
         format_out = FMT_ABGR;
      } else {
         format_out = FMT_ARGB;
      }
      break;

    case YUV422:
      if(out_planar) {
        format_out = FMT_YUV422P;
      } else {
        printf("ERROR: Invalid output format\n");
        return 0;
      }
      break;
  }

  const int width = p_param.SRC_SIZE >> 16;
  const int height = p_param.SRC_SIZE & 0xffff;

    in_ch0  = (uint8 *)batch->cmd->src_addr0;
    in_ch1  = (uint8 *)batch->cmd->src_addr1;
    in_ch2  = (uint8 *)batch->cmd->src_addr2;

    out_ch0  = (uint8 *)batch->cmd->dst_addr0;
    out_ch1  = (uint8 *)batch->cmd->dst_addr1;
    out_ch2  = (uint8 *)batch->cmd->dst_addr2;

    video_in  = (int32 *)malloc(height*width*3 * sizeof(int32)); // process
    write_input_bin_file(format_in, video_in, A_swap_in, in_ch0, in_ch1, in_ch2, height, width, src_stride, srcUV);

  for(int i=0; i<crop_num; i++) {

    const int dst_crop_w = p_param.DST_CROP_SIZE[i] >> 16;
    const int dst_crop_h = p_param.DST_CROP_SIZE[i] & 0xffff;

    video_out  = (uint8 *)malloc(dst_crop_h*dst_crop_w * 3 * sizeof(uint8));

    vpp_top_1684(
      &p_param         ,
      i               ,
      format_in       ,
      format_out      ,
      video_in        ,
      video_out       ,
      height          ,
      width           ,
      batch->cmd->csc_type,
      csc_matrix_list);

    write_output_bin_file(format_out, A_swap_out, video_out, out_ch0, out_ch1, out_ch2, dst_crop_h, dst_crop_w, dst_stride, dstUV);

    free(video_out);

  } //for crop_num

    free(video_in);

    free(p_param.SRC_CROP_ST);
    free(p_param.SRC_CROP_SIZE);
    free(p_param.DST_CROP_ST);
    free(p_param.DST_CROP_SIZE);
    free(p_param.DST_RY_BASE);
    free(p_param.DST_GU_BASE);
    free(p_param.DST_BV_BASE);
    free(p_param.DST_STRIDE);
    free(p_param.DST_RY_BASE_EXT);
    free(p_param.DST_GU_BASE_EXT);
    free(p_param.DST_BV_BASE_EXT);
    free(p_param.SCL_CTRL);
    free(p_param.SCL_INIT);
    free(p_param.SCL_X);
    free(p_param.SCL_Y);

  return 1;
}

