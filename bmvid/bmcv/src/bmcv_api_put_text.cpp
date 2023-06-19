#include <memory>
#include <vector>
#include <iostream>
#ifdef __linux__
  #include <sys/time.h>
#else
  #include <time.h>
#endif
#include <float.h>
#include <math.h>
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "device_mem_allocator.h"
#include "hershey_fronts.h"

using namespace std;

#define SATURATE(a, s, e) ((a) > (e) ? (e) : ((a) < (s) ? (s) : (a)))
#define IS_CS_YUV(a) (a == FORMAT_NV12 || a == FORMAT_NV21 || a == FORMAT_NV16 \
                   || a == FORMAT_NV61 || a == FORMAT_NV24 || a == FORMAT_YUV420P \
                   || a == FORMAT_YUV422P || a == FORMAT_YUV444P)
typedef long long int64;
typedef unsigned char uchar;

typedef struct {
    int64 x;
    int64 y;
} bmPoint2l;

enum {XY_SHIFT = 16, XY_ONE = 1 << XY_SHIFT};

static const int ITUR_BT_601_SHIFT = 20;
static const int ITUR_BT_601_CRY =  269484;
static const int ITUR_BT_601_CGY =  528482;
static const int ITUR_BT_601_CBY =  102760;
static const int ITUR_BT_601_CRU = -155188;
static const int ITUR_BT_601_CGU = -305135;
static const int ITUR_BT_601_CBU =  460324;
static const int ITUR_BT_601_CGV = -385875;
static const int ITUR_BT_601_CBV = -74448;

enum HersheyFonts {
    FONT_HERSHEY_SIMPLEX        = 0, //!< normal size sans-serif font
    FONT_HERSHEY_PLAIN          = 1, //!< small size sans-serif font
    FONT_HERSHEY_DUPLEX         = 2, //!< normal size sans-serif font (more complex than FONT_HERSHEY_SIMPLEX)
    FONT_HERSHEY_COMPLEX        = 3, //!< normal size serif font
    FONT_HERSHEY_TRIPLEX        = 4, //!< normal size serif font (more complex than FONT_HERSHEY_COMPLEX)
    FONT_HERSHEY_COMPLEX_SMALL  = 5, //!< smaller version of FONT_HERSHEY_COMPLEX
    FONT_HERSHEY_SCRIPT_SIMPLEX = 6, //!< hand-writing style font
    FONT_HERSHEY_SCRIPT_COMPLEX = 7, //!< more complex variant of FONT_HERSHEY_SCRIPT_SIMPLEX
    FONT_ITALIC                 = 16 //!< flag for italic font
};

enum { FONT_SIZE_SHIFT=8, FONT_ITALIC_ALPHA=(1 << 8),
       FONT_ITALIC_DIGIT=(2 << 8), FONT_ITALIC_PUNCT=(4 << 8),
       FONT_ITALIC_BRACES=(8 << 8), FONT_HAVE_GREEK=(16 << 8),
       FONT_HAVE_CYRILLIC=(32 << 8) };

static const int HersheyPlain[] = {
(5 + 4*16) + FONT_HAVE_GREEK,
199, 214, 217, 233, 219, 197, 234, 216, 221, 222, 228, 225, 211, 224, 210, 220,
200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 212, 213, 191, 226, 192,
215, 190, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 193, 84,
194, 85, 86, 87, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126,
195, 223, 196, 88 };

static const int HersheyPlainItalic[] = {
(5 + 4*16) + FONT_ITALIC_ALPHA + FONT_HAVE_GREEK,
199, 214, 217, 233, 219, 197, 234, 216, 221, 222, 228, 225, 211, 224, 210, 220,
200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 212, 213, 191, 226, 192,
215, 190, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 193, 84,
194, 85, 86, 87, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161,
162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176,
195, 223, 196, 88 };

static const int HersheyComplexSmall[] = {
(6 + 7*16) + FONT_HAVE_GREEK,
1199, 1214, 1217, 1275, 1274, 1271, 1272, 1216, 1221, 1222, 1219, 1232, 1211, 1231, 1210, 1220,
1200, 1201, 1202, 1203, 1204, 1205, 1206, 1207, 1208, 1209, 1212, 2213, 1241, 1238, 1242,
1215, 1273, 1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008, 1009, 1010, 1011, 1012, 1013,
1014, 1015, 1016, 1017, 1018, 1019, 1020, 1021, 1022, 1023, 1024, 1025, 1026, 1223, 1084,
1224, 1247, 586, 1249, 1101, 1102, 1103, 1104, 1105, 1106, 1107, 1108, 1109, 1110, 1111,
1112, 1113, 1114, 1115, 1116, 1117, 1118, 1119, 1120, 1121, 1122, 1123, 1124, 1125, 1126,
1225, 1229, 1226, 1246 };

static const int HersheyComplexSmallItalic[] = {
(6 + 7*16) + FONT_ITALIC_ALPHA + FONT_HAVE_GREEK,
1199, 1214, 1217, 1275, 1274, 1271, 1272, 1216, 1221, 1222, 1219, 1232, 1211, 1231, 1210, 1220,
1200, 1201, 1202, 1203, 1204, 1205, 1206, 1207, 1208, 1209, 1212, 1213, 1241, 1238, 1242,
1215, 1273, 1051, 1052, 1053, 1054, 1055, 1056, 1057, 1058, 1059, 1060, 1061, 1062, 1063,
1064, 1065, 1066, 1067, 1068, 1069, 1070, 1071, 1072, 1073, 1074, 1075, 1076, 1223, 1084,
1224, 1247, 586, 1249, 1151, 1152, 1153, 1154, 1155, 1156, 1157, 1158, 1159, 1160, 1161,
1162, 1163, 1164, 1165, 1166, 1167, 1168, 1169, 1170, 1171, 1172, 1173, 1174, 1175, 1176,
1225, 1229, 1226, 1246 };

static const int HersheySimplex[] = {
(9 + 12*16) + FONT_HAVE_GREEK,
2199, 714, 717, 733, 719, 697, 734, 716, 721, 722, 728, 725, 711, 724, 710, 720,
700, 701, 702, 703, 704, 705, 706, 707, 708, 709, 712, 713, 691, 726, 692,
715, 690, 501, 502, 503, 504, 505, 506, 507, 508, 509, 510, 511, 512, 513,
514, 515, 516, 517, 518, 519, 520, 521, 522, 523, 524, 525, 526, 693, 584,
694, 2247, 586, 2249, 601, 602, 603, 604, 605, 606, 607, 608, 609, 610, 611,
612, 613, 614, 615, 616, 617, 618, 619, 620, 621, 622, 623, 624, 625, 626,
695, 723, 696, 2246 };

static const int HersheyDuplex[] = {
(9 + 12*16) + FONT_HAVE_GREEK,
2199, 2714, 2728, 2732, 2719, 2733, 2718, 2727, 2721, 2722, 2723, 2725, 2711, 2724, 2710, 2720,
2700, 2701, 2702, 2703, 2704, 2705, 2706, 2707, 2708, 2709, 2712, 2713, 2730, 2726, 2731,
2715, 2734, 2501, 2502, 2503, 2504, 2505, 2506, 2507, 2508, 2509, 2510, 2511, 2512, 2513,
2514, 2515, 2516, 2517, 2518, 2519, 2520, 2521, 2522, 2523, 2524, 2525, 2526, 2223, 2084,
2224, 2247, 587, 2249, 2601, 2602, 2603, 2604, 2605, 2606, 2607, 2608, 2609, 2610, 2611,
2612, 2613, 2614, 2615, 2616, 2617, 2618, 2619, 2620, 2621, 2622, 2623, 2624, 2625, 2626,
2225, 2229, 2226, 2246 };

static const int HersheyComplex[] = {
(9 + 12*16) + FONT_HAVE_GREEK + FONT_HAVE_CYRILLIC,
2199, 2214, 2217, 2275, 2274, 2271, 2272, 2216, 2221, 2222, 2219, 2232, 2211, 2231, 2210, 2220,
2200, 2201, 2202, 2203, 2204, 2205, 2206, 2207, 2208, 2209, 2212, 2213, 2241, 2238, 2242,
2215, 2273, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013,
2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021, 2022, 2023, 2024, 2025, 2026, 2223, 2084,
2224, 2247, 587, 2249, 2101, 2102, 2103, 2104, 2105, 2106, 2107, 2108, 2109, 2110, 2111,
2112, 2113, 2114, 2115, 2116, 2117, 2118, 2119, 2120, 2121, 2122, 2123, 2124, 2125, 2126,
2225, 2229, 2226, 2246, 2801, 2802, 2803, 2804, 2805, 2806, 2807, 2808, 2809, 2810, 2811,
2812, 2813, 2814, 2815, 2816, 2817, 2818, 2819, 2820, 2821, 2822, 2823, 2824, 2825, 2826,
2827, 2828, 2829, 2830, 2831, 2832, 2901, 2902, 2903, 2904, 2905, 2906, 2907, 2908, 2909,
2910, 2911, 2912, 2913, 2914, 2915, 2916, 2917, 2918, 2919, 2920, 2921, 2922, 2923, 2924,
2925, 2926, 2927, 2928, 2929, 2930, 2931, 2932};

static const int HersheyComplexItalic[] = {
(9 + 12*16) + FONT_ITALIC_ALPHA + FONT_ITALIC_DIGIT + FONT_ITALIC_PUNCT +
FONT_HAVE_GREEK + FONT_HAVE_CYRILLIC,
2199, 2764, 2778, 2782, 2769, 2783, 2768, 2777, 2771, 2772, 2219, 2232, 2211, 2231, 2210, 2220,
2750, 2751, 2752, 2753, 2754, 2755, 2756, 2757, 2758, 2759, 2212, 2213, 2241, 2238, 2242,
2765, 2273, 2051, 2052, 2053, 2054, 2055, 2056, 2057, 2058, 2059, 2060, 2061, 2062, 2063,
2064, 2065, 2066, 2067, 2068, 2069, 2070, 2071, 2072, 2073, 2074, 2075, 2076, 2223, 2084,
2224, 2247, 587, 2249, 2151, 2152, 2153, 2154, 2155, 2156, 2157, 2158, 2159, 2160, 2161,
2162, 2163, 2164, 2165, 2166, 2167, 2168, 2169, 2170, 2171, 2172, 2173, 2174, 2175, 2176,
2225, 2229, 2226, 2246 };

static const int HersheyTriplex[] = {
(9 + 12*16) + FONT_HAVE_GREEK,
2199, 3214, 3228, 3232, 3219, 3233, 3218, 3227, 3221, 3222, 3223, 3225, 3211, 3224, 3210, 3220,
3200, 3201, 3202, 3203, 3204, 3205, 3206, 3207, 3208, 3209, 3212, 3213, 3230, 3226, 3231,
3215, 3234, 3001, 3002, 3003, 3004, 3005, 3006, 3007, 3008, 3009, 3010, 3011, 3012, 3013,
2014, 3015, 3016, 3017, 3018, 3019, 3020, 3021, 3022, 3023, 3024, 3025, 3026, 2223, 2084,
2224, 2247, 587, 2249, 3101, 3102, 3103, 3104, 3105, 3106, 3107, 3108, 3109, 3110, 3111,
3112, 3113, 3114, 3115, 3116, 3117, 3118, 3119, 3120, 3121, 3122, 3123, 3124, 3125, 3126,
2225, 2229, 2226, 2246 };

static const int HersheyTriplexItalic[] = {
(9 + 12*16) + FONT_ITALIC_ALPHA + FONT_ITALIC_DIGIT +
FONT_ITALIC_PUNCT + FONT_HAVE_GREEK,
2199, 3264, 3278, 3282, 3269, 3233, 3268, 3277, 3271, 3272, 3223, 3225, 3261, 3224, 3260, 3270,
3250, 3251, 3252, 3253, 3254, 3255, 3256, 3257, 3258, 3259, 3262, 3263, 3230, 3226, 3231,
3265, 3234, 3051, 3052, 3053, 3054, 3055, 3056, 3057, 3058, 3059, 3060, 3061, 3062, 3063,
2064, 3065, 3066, 3067, 3068, 3069, 3070, 3071, 3072, 3073, 3074, 3075, 3076, 2223, 2084,
2224, 2247, 587, 2249, 3151, 3152, 3153, 3154, 3155, 3156, 3157, 3158, 3159, 3160, 3161,
3162, 3163, 3164, 3165, 3166, 3167, 3168, 3169, 3170, 3171, 3172, 3173, 3174, 3175, 3176,
2225, 2229, 2226, 2246 };

static const int HersheyScriptSimplex[] = {
(9 + 12*16) + FONT_ITALIC_ALPHA + FONT_HAVE_GREEK,
2199, 714, 717, 733, 719, 697, 734, 716, 721, 722, 728, 725, 711, 724, 710, 720,
700, 701, 702, 703, 704, 705, 706, 707, 708, 709, 712, 713, 691, 726, 692,
715, 690, 551, 552, 553, 554, 555, 556, 557, 558, 559, 560, 561, 562, 563,
564, 565, 566, 567, 568, 569, 570, 571, 572, 573, 574, 575, 576, 693, 584,
694, 2247, 586, 2249, 651, 652, 653, 654, 655, 656, 657, 658, 659, 660, 661,
662, 663, 664, 665, 666, 667, 668, 669, 670, 671, 672, 673, 674, 675, 676,
695, 723, 696, 2246 };

static const int HersheyScriptComplex[] = {
(9 + 12*16) + FONT_ITALIC_ALPHA + FONT_ITALIC_DIGIT + FONT_ITALIC_PUNCT + FONT_HAVE_GREEK,
2199, 2764, 2778, 2782, 2769, 2783, 2768, 2777, 2771, 2772, 2219, 2232, 2211, 2231, 2210, 2220,
2750, 2751, 2752, 2753, 2754, 2755, 2756, 2757, 2758, 2759, 2212, 2213, 2241, 2238, 2242,
2215, 2273, 2551, 2552, 2553, 2554, 2555, 2556, 2557, 2558, 2559, 2560, 2561, 2562, 2563,
2564, 2565, 2566, 2567, 2568, 2569, 2570, 2571, 2572, 2573, 2574, 2575, 2576, 2223, 2084,
2224, 2247, 586, 2249, 2651, 2652, 2653, 2654, 2655, 2656, 2657, 2658, 2659, 2660, 2661,
2662, 2663, 2664, 2665, 2666, 2667, 2668, 2669, 2670, 2671, 2672, 2673, 2674, 2675, 2676,
2225, 2229, 2226, 2246 };

static inline uchar rgbToY42x(uchar r, uchar g, uchar b)
{
    const int shifted16 = (16 << ITUR_BT_601_SHIFT);
    const int halfShift = (1 << (ITUR_BT_601_SHIFT - 1));
    int yy = ITUR_BT_601_CRY * r + ITUR_BT_601_CGY * g + ITUR_BT_601_CBY * b + halfShift + shifted16;

    return SATURATE(yy >> ITUR_BT_601_SHIFT, 0, 255);
}

static inline void rgbToUV42x(uchar r, uchar g, uchar b, uchar& u, uchar& v)
{
    const int halfShift = (1 << (ITUR_BT_601_SHIFT - 1));
    const int shifted128 = (128 << ITUR_BT_601_SHIFT);
    int uu = ITUR_BT_601_CRU * r + ITUR_BT_601_CGU * g + ITUR_BT_601_CBU * b + halfShift + shifted128;
    int vv = ITUR_BT_601_CBU * r + ITUR_BT_601_CGV * g + ITUR_BT_601_CBV * b + halfShift + shifted128;

    u = SATURATE(uu >> ITUR_BT_601_SHIFT, 0, 255);
    v = SATURATE(vv >> ITUR_BT_601_SHIFT, 0, 255);
}

void inline fill16bit(
        uchar* buffer,
        uchar u,
        uchar v,
        int size) {
   for(int i = 0; i < size; i++)
   {
       *(buffer+i*2)=u;
       *(buffer+i*2+1)=v;
   }
}

static void fillYuvRow(
        bmMat &img,
        uchar y,
        uchar u,
        uchar v,
        int starty,
        int startx,
        int filllen) {
    starty = starty < 0 ? 0 : starty;
    startx = startx < 0 ? 0 : startx;
    starty = starty >= img.height ? img.height - 1 : starty;
    startx = startx >= img.width ? img.width - 1 : startx;
    filllen = (startx + filllen > img.width - 1) ? (img.width - 1 - startx) : filllen;
    if (img.format == FORMAT_GRAY) {
        memset((uchar *)img.data[0] + starty * img.step[0] + startx, y, filllen);
    }
    else if (img.format == FORMAT_YUV444P) {
        memset((uchar *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        memset((uchar *)img.data[1] + starty * img.step[1] + startx, u, filllen);
        memset((uchar *)img.data[2] + starty * img.step[2] + startx, v, filllen);
    }
    else if (img.format == FORMAT_YUV422P) {
        memset((uchar *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        memset((uchar *)img.data[1] + starty * img.step[1] + startx / 2, u, filllen/2);
        memset((uchar *)img.data[2] + starty * img.step[2] + startx / 2, v, filllen/2);
    }
    else if (img.format == FORMAT_YUV420P) {
        memset((uchar *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        memset((uchar *)img.data[1] + starty / 2 * img.step[1] + startx / 2, u, filllen / 2);
        memset((uchar *)img.data[2] + starty / 2 * img.step[2] + startx / 2, v, filllen / 2);
    }
    else if (img.format == FORMAT_NV12) {
        memset((uchar *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        fill16bit((uchar *)img.data[1] + (starty / 2) * img.step[1] + (startx / 2) * 2, u, v, filllen / 2);
    }
    else if (img.format == FORMAT_NV21) {
        memset((uchar *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        fill16bit((uchar *)img.data[1] + (starty / 2) * img.step[1] + (startx / 2) * 2, v, u, filllen / 2);
    }
    else if (img.format == FORMAT_NV16) {
        memset((uchar *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        fill16bit((uchar *)img.data[1] + starty * img.step[1] + (startx / 2) * 2, u, v, filllen / 2);
    }
    else if (img.format == FORMAT_NV61) {
        memset((uchar *)img.data[0] + starty * img.step[0] + startx, y, filllen);
        fill16bit((uchar *)img.data[1] + starty * img.step[1] + (startx / 2) * 2, v, u, filllen / 2);
    }
    else {
        printf("rectangle function can't support  format=%d\n", img.format);
        return;
    }
    return;
}

static void put_line(
        bmMat &inout,
        bmcv_point_t start,
        bmcv_point_t end,
        bmcv_color_t color,
        int thickness) {
    // clip point
    bmcv_point_t sp;
    bmcv_point_t ep;
    sp.x = SATURATE(start.x, 0, (inout.width - 1) << 16);
    sp.y = SATURATE(start.y, 0, (inout.height - 1) << 16);
    ep.x = SATURATE(end.x, 0, (inout.width - 1) << 16);
    ep.y = SATURATE(end.y, 0, (inout.height - 1) << 16);
    // because only support start_y < end_y
    if (sp.y > ep.y) {
        swap(sp.x, ep.x);
        swap(sp.y, ep.y);
    }
    uchar rc = color.r;
    uchar gc = color.g;
    uchar bc = color.b;
    uchar yc = rgbToY42x(rc, gc, bc);
    uchar uc=128, vc=128;
    rgbToUV42x(rc, gc, bc, uc, vc);

    static const double INV_XY_ONE = 1. / XY_ONE;
    bmPoint2l p0, p1;
    p0.x = sp.x;// << XY_SHIFT;
    p0.y = sp.y;// << XY_SHIFT;
    p1.x = ep.x;// << XY_SHIFT;
    p1.y = ep.y;// << XY_SHIFT;
    double dx = (p0.x - p1.x) * INV_XY_ONE;
    double dy = (p1.y - p0.y) * INV_XY_ONE;
    double r = dx * dx + dy * dy;
    if (dx == 0) {
        int ybegin = (int)(p0.y >> XY_SHIFT);
        int xbegin = (int)(p0.x >> XY_SHIFT) - thickness / 2;
        for(int i=0; i< (int)dy ; i++)
            fillYuvRow(inout, yc, uc, vc, ybegin + i, xbegin, thickness);
        return;
    }
    if (dy == 0) {
        int xbegin = dx < 0 ? (int)(p0.x >> XY_SHIFT) : (int)(p1.x >> XY_SHIFT);
        int ybegin = (int)(p0.y >> XY_SHIFT) - thickness / 2;
        for (int i=0; i < thickness; i++)
            fillYuvRow(inout, yc, uc, vc, ybegin + i, xbegin, fabs(dx));
        return;
    }
    if (thickness <= 1) {
        int area1 = (int)(( p1.y - p0.y) >> XY_SHIFT);
        int p0y = (int)(p0.y >> XY_SHIFT);
        for (int i = 0; i < area1; i++)
           fillYuvRow(inout, yc, uc, vc, p0y + i, (int64)(p0.x - (i << XY_SHIFT) * dx / dy) >> XY_SHIFT, 1);
    } else {
        if (fabs(r) > DBL_EPSILON) {
            bmPoint2l pt[4];
            bmPoint2l dp;
            thickness <<= XY_SHIFT - 1;
            r = (thickness + XY_ONE * 0.5) / sqrt(r);
            dp.x = round(dy * r);
            dp.y = round(dx * r);
            pt[0].x = p0.x + dp.x;
            pt[0].y = p0.y + dp.y;
            pt[1].x = p0.x - dp.x;
            pt[1].y = p0.y - dp.y;
            pt[2].x = p1.x - dp.x;
            pt[2].y = p1.y - dp.y;
            pt[3].x = p1.x + dp.x;
            pt[3].y = p1.y + dp.y;

            int pt1y = (int)pt[1].y >> XY_SHIFT;
            int pt0y = (int)pt[0].y >> XY_SHIFT;
            int pt2y = (int)pt[2].y >> XY_SHIFT;
            int pt3y = (int)pt[3].y >> XY_SHIFT;
            int area1= abs(pt1y - pt0y);
            bmPoint2l toppt = pt0y < pt1y ? pt[0] : pt[1];
            int topy = pt0y < pt1y ? pt0y : pt1y;
            for (int i= 0; i <= area1; i++) {  // begin with pt[1].y
                int xbegin = (int)((int64)(toppt.x - (i << XY_SHIFT) * dx / dy) >> XY_SHIFT);
                int xend = (int)((int64)(toppt.x + (i << XY_SHIFT) * dy / dx) >> XY_SHIFT);
                int fillbegin = xbegin;
                int filllen = xend - xbegin;
                if(xbegin > xend)
                {
                    fillbegin = xend;
                    filllen = xbegin-xend;
                }
                //memory copy y  uv buffer
                fillYuvRow(inout, yc, uc, vc, topy + i, fillbegin, filllen);
            }

            int area2= pt0y > pt1y ? pt2y - pt0y : pt3y - pt1y;
            bmPoint2l sectoppt = pt0y > pt1y ? pt[0] : pt[1];
            int secondy = topy + area1;
            for (int i = 0; i < area2; i++ ) {  //begin with p[0].y
                int xbegin = (int)((int64)(toppt.x - ((i + area1) << XY_SHIFT) * dx / dy) >> XY_SHIFT);
                int xend = (int)((int64)(sectoppt.x - (i << XY_SHIFT) * dx / dy) >> XY_SHIFT);
                int fillbegin = xbegin;
                int filllen = xend - xbegin;
                if(xbegin > xend)
                {
                    fillbegin = xend;
                    filllen = xbegin - xend;
                }
                fillYuvRow(inout, yc, uc, vc, secondy + i, fillbegin, filllen);
                //memory copy y  uv buffer
            }
            int area3 = area1;
            bmPoint2l bottomopt = pt2y > pt3y ? pt[2] : pt[3];
            int thridy = topy + area1 + area2;
            for (int i = 0; i < area3; i++) {  //begin with p[2].y
                int xbegin = (int)((int64)(bottomopt.x - ((area3 - i) << XY_SHIFT) * dy / dx) >> XY_SHIFT);
                int xend = (int)((int64)(bottomopt.x + ((area3 - i) << XY_SHIFT) * dx / dy) >> XY_SHIFT);
                int fillbegin = xbegin;
                int filllen = xend - xbegin;
                if(xbegin > xend) {
                    fillbegin = xend;
                    filllen = xbegin-xend;
                }
                fillYuvRow(inout, yc, uc, vc, thridy + i, fillbegin, filllen);
                //memory copy y  uv buffer
            }
        }
    }
}

static const int* get_font_data(int fontFace) {
    bool isItalic = (fontFace & FONT_ITALIC) != 0;
    const int* ascii = 0;
    switch (fontFace & 15) {
        case FONT_HERSHEY_SIMPLEX:
            ascii = HersheySimplex;
            break;
        case FONT_HERSHEY_PLAIN:
            ascii = !isItalic ? HersheyPlain : HersheyPlainItalic;
            break;
        case FONT_HERSHEY_DUPLEX:
            ascii = HersheyDuplex;
            break;
        case FONT_HERSHEY_COMPLEX:
            ascii = !isItalic ? HersheyComplex : HersheyComplexItalic;
            break;
        case FONT_HERSHEY_TRIPLEX:
            ascii = !isItalic ? HersheyTriplex : HersheyTriplexItalic;
            break;
        case FONT_HERSHEY_COMPLEX_SMALL:
            ascii = !isItalic ? HersheyComplexSmall : HersheyComplexSmallItalic;
            break;
        case FONT_HERSHEY_SCRIPT_SIMPLEX:
            ascii = HersheyScriptSimplex;
            break;
        case FONT_HERSHEY_SCRIPT_COMPLEX:
            ascii = HersheyScriptComplex;
            break;
        default:
            cout << "Unknown font type" << endl;
    }
    return ascii;
}

inline void read_check(
        int &c,
        int &i,
        const char* text,
        int fontFace) {
    int leftBoundary = ' ', rightBoundary = 127;

    if (c >= 0x80 && fontFace == FONT_HERSHEY_COMPLEX) {
        if(c == 0xD0 && (unsigned char)text[i + 1] >= 0x90 && (unsigned char)text[i + 1] <= 0xBF) {
            c = (unsigned char)text[++i] - 17;
            leftBoundary = 127;
            rightBoundary = 175;
        } else if (c == 0xD1 && (unsigned char)text[i + 1] >= 0x80 && (unsigned char)text[i + 1] <= 0x8F) {
            c = (unsigned char)text[++i] + 47;
            leftBoundary = 175;
            rightBoundary = 191;
        } else {
            if(c >= 0xC0 && text[i+1] != 0) //2 bytes utf
                i++;
            if(c >= 0xE0 && text[i+1] != 0) //3 bytes utf
                i++;
            if(c >= 0xF0 && text[i+1] != 0) //4 bytes utf
                i++;
            if(c >= 0xF8 && text[i+1] != 0) //5 bytes utf
                i++;
            if(c >= 0xFC && text[i+1] != 0) //6 bytes utf
                i++;
            c = '?';
        }
    }
    if(c >= rightBoundary || c < leftBoundary)
        c = '?';
}

void poly_line(
        bmMat mat,
        bmcv_point_t* v,
        int count,
        bool is_closed,
        bmcv_color_t color,
        int thickness
        ) {
    if (!v || count <= 0)
        return;

    int i = is_closed ? count - 1 : 0;
    // int flags = 2 + !is_closed;
    bmcv_point_t p0;

    p0 = v[i];
    for (i = !is_closed; i < count; i++){
        bmcv_point_t p = v[i];
        put_line(mat, p0, p, color, thickness);
        p0 = p;
        // flags = 2;
    }
}

void put_text(
        bmMat mat,
        const char* text,
        bmcv_point_t org,
        int fontFace,
        float fontScale,
        bmcv_color_t color,
        int thickness) {
    if (text == NULL) {
        return;
    }
    const int* ascii = get_font_data(fontFace);

    int base_line = -(ascii[0] & 15);
    int hscale = round(fontScale * XY_ONE), vscale = hscale;

    int view_x = org.x << XY_SHIFT;
    int view_y = (org.y << XY_SHIFT) + base_line * vscale;
    std::vector<bmcv_point_t> pts;
    pts.reserve(1 << 10);
    const char **faces = g_HersheyGlyphs;

    for(int i = 0; i < (int)strlen(text); i++) {
        int c = (unsigned char)text[i];
        bmcv_point_t p;
        read_check(c, i, text, fontFace);

        const char* ptr = faces[ascii[(c-' ')+1]];
        p.x = (unsigned char)ptr[0] - 'R';
        p.y = (unsigned char)ptr[1] - 'R';
        int dx = p.y * hscale;
        view_x -= p.x * hscale;
        pts.resize(0);

        for (ptr += 2;; ) {
            if (*ptr == ' ' || !*ptr) {
                if (pts.size() > 1 ) {
                    poly_line(mat, &pts[0], (int)pts.size(), false, color, thickness);
                }
                if (!*ptr++)
                    break;
                pts.resize(0);
            } else {
                p.x = (unsigned char)ptr[0] - 'R';
                p.y = (unsigned char)ptr[1] - 'R';
                ptr += 2;
                bmcv_point_t point = {p.x * hscale + view_x, p.y * vscale + view_y};
                pts.push_back(point);
            }
        }
        view_x += dx;
    }
    return;
}
static bm_status_t bmcv_put_text_check(
        bm_handle_t handle,
        bm_image image,
        int thickness) {
    if (handle == NULL) {
        bmlib_log("DRAW_LINE", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }
    if (thickness <= 0) {
        bmlib_log("DRAW_LINE", BMLIB_LOG_ERROR, "thickness should greater than 0!\r\n");
        return BM_ERR_PARAM;
    }
    if (!IS_CS_YUV(image.image_format) && image.image_format != FORMAT_GRAY) {
        bmlib_log("DRAW_LINE", BMLIB_LOG_ERROR, "image format not supported %d !\r\n", image.image_format);
        return BM_ERR_PARAM;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_put_text(
        bm_handle_t handle,
        bm_image image,
        const char* text,
        bmcv_point_t org,
        bmcv_color_t color,
        float fontScale,
        int thickness) {
    if (BM_SUCCESS != bmcv_put_text_check(handle, image, thickness)) {
        return BM_ERR_FAILURE;
    }
    int w = image.width;
    int h = image.height;
    unsigned char* host_buf = new unsigned char [w * h * 3];
    unsigned char* in_ptr[3] = {host_buf, host_buf + w * h, host_buf + w * h * 2};
    bm_image_copy_device_to_host(image, (void **)in_ptr);
    int str[3];
    bm_image_get_stride(image, str);
    bmMat mat;
    mat.width = image.width;
    mat.height = image.height;
    mat.format = image.image_format;
    mat.step = &str[0];
    mat.data = (void**)in_ptr;
    put_text(mat, text, org, FONT_HERSHEY_SIMPLEX, fontScale, color, thickness);
    bm_image_copy_host_to_device(image, (void **)in_ptr);
    delete [] host_buf;
    return BM_SUCCESS;
}

