#include "lion-render.h"

#include <stdlib.h>
#include <stdio.h>

#define ELEMENTS(x) (sizeof(x)/sizeof((x)[0]))

static void init(struct lion *l, int i, VGint hexColor, const VGfloat *coords, int elems)
{
   static VGubyte cmds[128];
   VGfloat color[4];
   VGint j;

   color[0] = ((hexColor >> 16) & 0xff) / 255.f;
   color[1] = ((hexColor >> 8) & 0xff) / 255.f;
   color[2] = ((hexColor >> 0) & 0xff) / 255.f;
   color[3] = 1.0;

   l->paths[i] = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f,
                              0, 0, (unsigned int)VG_PATH_CAPABILITY_ALL);
   l->fills[i] = vgCreatePaint();
   vgSetParameterfv(l->fills[i], VG_PAINT_COLOR, 4, color);

   cmds[0] = VG_MOVE_TO_ABS;
   for (j = 1; j < elems; ++j) {
      cmds[j] = VG_LINE_TO_ABS;
   }

   vgAppendPathData(l->paths[i], elems, cmds, coords);
}

static void poly0(struct lion *l)
{
   VGfloat color = 0xf2cc99;
   static const VGfloat coords[] = {69,18, 82,8, 99,3, 118,5, 135,12, 149,21, 156,13, 165,9, 177,13, 183,28,
                                    180,50, 164,91, 155,107, 154,114, 151,121, 141,127, 139,136, 155,206, 157,251, 126,342,
                                    133,357, 128,376, 83,376, 75,368, 67,350, 61,350, 53,369, 4,369, 2,361, 5,354,
                                    12,342, 16,321, 4,257, 4,244, 7,218, 9,179, 26,127, 43,93, 32,77, 30,70,
                                    24,67, 16,49, 17,35, 18,23, 30,12, 40,7, 53,7, 62,12
   };

   init(l, 0, color, coords, ELEMENTS(coords)/2);
}

static void poly1(struct lion *l)
{
   VGfloat color = 0xe5b27f;
   static const VGfloat coords[] = {142,79, 136,74, 138,82, 133,78, 133,84, 127,78, 128,85,
                                    124,80, 125,87, 119,82, 119,90, 125,99, 125,96, 128,100, 128,94,
                                    131,98, 132,93, 135,97, 136,93, 138,97, 139,94, 141,98, 143,94,
                                    144,85
   };

   init(l, 1, color, coords, ELEMENTS(coords)/2);
}

static void poly2(struct lion *l)
{
   VGfloat color = 0xeb8080;
   static const VGfloat coords[] = {127,101, 132,100, 137,99, 144,101, 143,105, 135,110
   };

   init(l, 2, color, coords, ELEMENTS(coords)/2);
}

static void poly3(struct lion *l)
{
   VGfloat color = 0xf2cc99;
   static const VGfloat coords[] = {178,229, 157,248, 139,296, 126,349, 137,356,
                                    158,357, 183,342, 212,332, 235,288, 235,261,
                                    228,252, 212,250, 188,251
   };

   init(l, 3, color, coords, ELEMENTS(coords)/2);
}

static void poly4(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {56,229, 48,241, 48,250, 57,281, 63,325, 71,338,
                                    81,315, 76,321, 79,311, 83,301, 75,308, 80,298,
                                    73,303, 76,296, 71,298, 74,292, 69,293, 74,284,
                                    78,278, 71,278, 74,274, 68,273, 70,268, 66,267,
                                    68,261, 60,266, 62,259, 65,253, 57,258, 59,251,
                                    55,254, 55,248, 60,237, 54,240, 58,234, 54,236
   };

   init(l, 4, color, coords, ELEMENTS(coords)/2);
}

static void poly5(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {74,363, 79,368, 81,368, 85,362, 89,363, 92,370, 96,373,
                                    101,372, 108,361, 110,371, 113,373, 116,371, 120,358, 122,363,
                                    123,371, 126,371, 129,367, 132,357, 135,361, 130,376, 127,377,
                                    94,378, 84,376, 76,371
   };

   init(l, 5, color, coords, ELEMENTS(coords)/2);
}

static void poly6(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {212,250, 219,251, 228,258, 236,270, 235,287, 225,304,
                                    205,332, 177,343, 171,352, 158,357, 166,352, 168,346,
                                    168,339, 165,333, 155,327, 155,323, 161,320, 165,316,
                                    169,316, 167,312, 171,313, 168,308, 173,309, 170,306,
                                    177,306, 175,308, 177,311, 174,311, 176,316, 171,315,
                                    174,319, 168,320, 168,323, 175,327, 179,332, 183,326,
                                    184,332, 189,323, 190,328, 194,320, 194,325, 199,316,
                                    201,320, 204,313, 206,316, 208,310, 211,305, 219,298,
                                    226,288, 229,279, 228,266, 224,259, 217,253
   };

   init(l, 6, color, coords, ELEMENTS(coords)/2);
}

static void poly7(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {151,205, 151,238, 149,252, 141,268, 128,282, 121,301,
                                    130,300, 126,313, 118,324, 116,337, 120,346, 133,352,
                                    133,340, 137,333, 145,329, 156,327, 153,319, 153,291,
                                    157,271, 170,259, 178,277, 193,250, 174,216
   };

   init(l, 7, color, coords, ELEMENTS(coords)/2);
}

static void poly8(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {78,127, 90,142, 95,155, 108,164, 125,167, 139,175,
                                    150,206, 152,191, 141,140, 121,148, 100,136
   };

   init(l, 8, color, coords, ELEMENTS(coords)/2);
}

static void poly9(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {21,58, 35,63, 38,68, 32,69, 42,74, 40,79, 47,80, 54,83,
                                    45,94, 34,81, 32,73, 24,66
   };

   init(l, 9, color, coords, ELEMENTS(coords)/2);
}

static void poly10(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {71,34, 67,34, 66,27, 59,24, 54,17, 48,17, 39,22,
                                    30,26, 28,31, 31,39, 38,46, 29,45, 36,54, 41,61,
                                    41,70, 50,69, 54,71, 55,58, 67,52, 76,43, 76,39,
                                    68,44
   };

   init(l, 10, color, coords, ELEMENTS(coords)/2);
}

static void poly11(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {139,74, 141,83, 143,89, 144,104, 148,104, 155,106,
                                    154,86, 157,77, 155,72, 150,77, 144,77
   };

   init(l, 11, color, coords, ELEMENTS(coords)/2);
}

static void poly12(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {105,44, 102,53, 108,58, 111,62, 112,55
   };

   init(l, 12, color, coords, ELEMENTS(coords)/2);
}

static void poly13(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {141,48, 141,54, 144,58, 139,62, 137,66, 136,59, 137,52
   };

   init(l, 13, color, coords, ELEMENTS(coords)/2);
}

static void poly14(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {98,135, 104,130, 105,134, 108,132, 108,135, 112,134,
                                    113,137, 116,136, 116,139, 119,139, 124,141, 128,140,
                                    133,138, 140,133, 139,140, 126,146, 104,144
   };

   init(l, 14, color, coords, ELEMENTS(coords)/2);
}

static void poly15(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {97,116, 103,119, 103,116, 111,118, 116,117, 122,114,
                                    127,107, 135,111, 142,107, 141,114, 145,118, 149,121,
                                    145,125, 140,124, 127,121, 113,125, 100,124
   };

   init(l, 15, color, coords, ELEMENTS(coords)/2);
}

static void poly16(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {147,33, 152,35, 157,34, 153,31, 160,31, 156,28, 161,28,
                                    159,24, 163,25, 163,21, 165,22, 170,23, 167,17, 172,21,
                                    174,18, 175,23, 176,22, 177,28, 177,33, 174,37, 176,39,
                                    174,44, 171,49, 168,53, 164,57, 159,68, 156,70, 154,60,
                                    150,51, 146,43, 144,35
   };

   init(l, 16, color, coords, ELEMENTS(coords)/2);
}

static void poly17(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {85,72, 89,74, 93,75, 100,76, 105,75, 102,79, 94,79, 88,76
   };

   init(l, 17, color, coords, ELEMENTS(coords)/2);
}

static void poly18(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {86,214, 79,221, 76,232, 82,225, 78,239, 82,234, 78,245,
                                    81,243, 79,255, 84,250, 84,267, 87,254, 90,271, 90,257,
                                    95,271, 93,256, 95,249, 92,252, 93,243, 89,253, 89,241,
                                    86,250, 87,236, 83,245, 87,231, 82,231, 90,219, 84,221
   };

   init(l, 18, color, coords, ELEMENTS(coords)/2);
}

static void poly19(struct lion *l)
{
   VGfloat color = 0xffcc7f;
   static const VGfloat coords[] = {93,68, 96,72, 100,73, 106,72, 108,66, 105,63, 100,62
   };

   init(l, 19, color, coords, ELEMENTS(coords)/2);
}

static void poly20(struct lion *l)
{
   VGfloat color = 0xffcc7f;
   static const VGfloat coords[] = {144,64, 142,68, 142,73, 146,74, 150,73, 154,64, 149,62
   };

   init(l, 20, color, coords, ELEMENTS(coords)/2);
}

static void poly21(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {57,91, 42,111, 52,105, 41,117, 53,112, 46,120, 53,116,
                                    50,124, 57,119, 55,127, 61,122, 60,130, 67,126, 66,134,
                                    71,129, 72,136, 77,130, 76,137, 80,133, 82,138, 86,135,
                                    96,135, 94,129, 86,124, 83,117, 77,123, 79,117, 73,120,
                                    75,112, 68,116, 71,111, 65,114, 69,107, 63,110, 68,102,
                                    61,107, 66,98, 61,103, 63,97, 57,99
   };

   init(l, 21, color, coords, ELEMENTS(coords)/2);
}

static void poly22(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {83,79, 76,79, 67,82, 75,83, 65,88, 76,87, 65,92, 76,91,
                                    68,96, 77,95, 70,99, 80,98, 72,104, 80,102, 76,108, 85,103,
                                    92,101, 87,98, 93,96, 86,94, 91,93, 85,91, 93,89, 99,89, 105,93,
                                    107,85, 102,82, 92,80
   };

   init(l, 22, color, coords, ELEMENTS(coords)/2);
}

static void poly23(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {109,77, 111,83, 109,89, 113,94, 117,90, 117,81, 114,78
   };

   init(l, 23, color, coords, ELEMENTS(coords)/2);
}

static void poly24(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {122,128, 127,126, 134,127, 136,129, 134,130, 130,128, 124,129
   };

   init(l, 24, color, coords, ELEMENTS(coords)/2);
}

static void poly25(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {78,27, 82,32, 80,33, 82,36, 78,37, 82,40, 78,42, 81,46, 76,47,
                                    78,49, 74,50, 82,52, 87,50, 83,48, 91,46, 86,45, 91,42, 88,40,
                                    92,37, 86,34, 90,31, 86,29, 89,26
   };

   init(l, 25, color, coords, ELEMENTS(coords)/2);
}

static void poly26(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {82,17, 92,20, 79,21, 90,25, 81,25, 94,28, 93,26, 101,30,
                                    101,26, 107,33, 108,28, 111,40, 113,34, 115,45, 117,39,
                                    119,54, 121,46, 124,58, 126,47, 129,59, 130,49, 134,58,
                                    133,44, 137,48, 133,37, 137,40, 133,32, 126,20, 135,26,
                                    132,19, 138,23, 135,17, 142,18, 132,11, 116,6, 94,6, 78,11,
                                    92,12, 80,14, 90,16
   };

   init(l, 26, color, coords, ELEMENTS(coords)/2);
}

static void poly27(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {142,234, 132,227, 124,223, 115,220, 110,225, 118,224, 127,229,
                                    135,236, 122,234, 115,237, 113,242, 121,238, 139,243, 121,245,
                                    111,254, 95,254, 102,244, 104,235, 110,229, 100,231, 104,224,
                                    113,216, 122,215, 132,217, 141,224, 145,230, 149,240
   };

   init(l, 27, color, coords, ELEMENTS(coords)/2);
}

static void poly28(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {115,252, 125,248, 137,249, 143,258, 134,255, 125,254
   };

   init(l, 28, color, coords, ELEMENTS(coords)/2);
}

static void poly29(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {114,212, 130,213, 140,219, 147,225, 144,214, 137,209, 128,207
   };

   init(l, 29, color, coords, ELEMENTS(coords)/2);
}

static void poly30(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {102,263, 108,258, 117,257, 131,258, 116,260, 109,265
   };

   init(l, 30, color, coords, ELEMENTS(coords)/2);
}

static void poly31(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {51,241, 35,224, 40,238, 23,224, 31,242, 19,239, 28,247, 17,246,
                                    25,250, 37,254, 39,263, 44,271, 47,294, 48,317, 51,328, 60,351,
                                    60,323, 53,262, 47,246
   };

   init(l, 31, color, coords, ELEMENTS(coords)/2);
}

static void poly32(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {2,364, 9,367, 14,366, 18,355, 20,364, 26,366, 31,357, 35,364,
                                    39,364, 42,357, 47,363, 53,360, 59,357, 54,369, 7,373
   };

   init(l, 32, color, coords, ELEMENTS(coords)/2);
}

static void poly33(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {7,349, 19,345, 25,339, 18,341, 23,333, 28,326, 23,326, 27,320,
                                    23,316, 25,311, 20,298, 15,277, 12,264, 9,249, 10,223, 3,248,
                                    5,261, 15,307, 17,326, 11,343
   };

   init(l, 33, color, coords, ELEMENTS(coords)/2);
}

static void poly34(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {11,226, 15,231, 25,236, 18,227
   };

   init(l, 34, color, coords, ELEMENTS(coords)/2);
}

static void poly35(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {13,214, 19,217, 32,227, 23,214, 16,208, 15,190, 24,148,
                                    31,121, 24,137, 14,170, 8,189
   };

   init(l, 35, color, coords, ELEMENTS(coords)/2);
}

static void poly36(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {202,254, 195,258, 199,260, 193,263, 197,263, 190,268,
                                    196,268, 191,273, 188,282, 200,272, 194,272, 201,266,
                                    197,265, 204,262, 200,258, 204,256
   };

   init(l, 36, color, coords, ELEMENTS(coords)/2);
}

static void poly37(struct lion *l)
{
   VGfloat color = 0x845433;
   static const VGfloat coords[] = {151,213, 165,212, 179,225, 189,246, 187,262, 179,275,
                                    176,263, 177,247, 171,233, 163,230, 165,251, 157,264,
                                    146,298, 145,321, 133,326, 143,285, 154,260, 153,240
   };

   init(l, 37, color, coords, ELEMENTS(coords)/2);
}

static void poly38(struct lion *l)
{
   VGfloat color = 0x845433;
   static const VGfloat coords[] = {91,132, 95,145, 97,154, 104,148, 107,155, 109,150, 111,158,
                                    115,152, 118,159, 120,153, 125,161, 126,155, 133,164, 132,154,
                                    137,163, 137,152, 142,163, 147,186, 152,192, 148,167, 141,143,
                                    124,145, 105,143
   };

   init(l, 38, color, coords, ELEMENTS(coords)/2);
}

static void poly39(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {31,57, 23,52, 26,51, 20,44, 23,42, 21,36, 22,29, 25,23,
                                    24,32, 30,43, 26,41, 30,50, 26,48
   };

   init(l, 39, color, coords, ELEMENTS(coords)/2);
}

static void poly40(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {147,21, 149,28, 155,21, 161,16, 167,14, 175,15, 173,11, 161,9
   };

   init(l, 40, color, coords, ELEMENTS(coords)/2);
}

static void poly41(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {181,39, 175,51, 169,57, 171,65, 165,68, 165,75, 160,76,
                                    162,91, 171,71, 180,51
   };

   init(l, 41, color, coords, ELEMENTS(coords)/2);
}

static void poly42(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {132,346, 139,348, 141,346, 142,341, 147,342, 143,355, 133,350
   };

   init(l, 42, color, coords, ELEMENTS(coords)/2);
}

static void poly43(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {146,355, 151,352, 155,348, 157,343, 160,349, 151,356, 147,357
   };

   init(l, 43, color, coords, ELEMENTS(coords)/2);
}

static void poly44(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {99,266, 100,281, 94,305, 86,322, 78,332, 72,346, 73,331, 91,291
   };

   init(l, 44, color, coords, ELEMENTS(coords)/2);
}

static void poly45(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {20,347, 32,342, 45,340, 54,345, 45,350, 42,353, 38,350,
                                    31,353, 29,356, 23,350, 19,353, 15,349
   };

   init(l, 45, color, coords, ELEMENTS(coords)/2);
}

static void poly46(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {78,344, 86,344, 92,349, 88,358, 84,352
   };

   init(l, 46, color, coords, ELEMENTS(coords)/2);
}

static void poly47(struct lion *l)
{
   VGfloat color = 0x9c826b;
   static const VGfloat coords[] = {93,347, 104,344, 117,345, 124,354, 121,357, 116,351,
                                    112,351, 108,355, 102,351
   };

   init(l, 47, color, coords, ELEMENTS(coords)/2);
}

static void poly48(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {105,12, 111,18, 113,24, 113,29, 119,34, 116,23, 112,16
   };

   init(l, 48, color, coords, ELEMENTS(coords)/2);
}

static void poly49(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {122,27, 125,34, 127,43, 128,34, 125,29
   };

   init(l, 49, color, coords, ELEMENTS(coords)/2);
}

static void poly50(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {115,13, 122,19, 122,15, 113,10
   };

   init(l, 50, color, coords, ELEMENTS(coords)/2);
}

static void poly51(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {116,172, 107,182, 98,193, 98,183, 90,199, 89,189, 84,207,
                                    88,206, 87,215, 95,206, 93,219, 91,230, 98,216, 97,226,
                                    104,214, 112,209, 104,208, 113,202, 126,200, 139,207, 132,198,
                                    142,203, 134,192, 142,195, 134,187, 140,185, 130,181, 136,177,
                                    126,177, 125,171, 116,180
   };

   init(l, 51, color, coords, ELEMENTS(coords)/2);
}

static void poly52(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {74,220, 67,230, 67,221, 59,235, 63,233, 60,248, 70,232, 65,249,
                                    71,243, 67,256, 73,250, 69,262, 73,259, 71,267, 76,262, 72,271,
                                    78,270, 76,275, 82,274, 78,290, 86,279, 86,289, 92,274, 88,275,
                                    87,264, 82,270, 82,258, 77,257, 78,247, 73,246, 77,233, 72,236
   };

   init(l, 52, color, coords, ELEMENTS(coords)/2);
}

static void poly53(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {133,230, 147,242, 148,250, 145,254, 138,247, 129,246, 142,245,
                                    138,241, 128,237, 137,238
   };

   init(l, 53, color, coords, ELEMENTS(coords)/2);
}

static void poly54(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {133,261, 125,261, 116,263, 111,267, 125,265
   };

   init(l, 54, color, coords, ELEMENTS(coords)/2);
}

static void poly55(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {121,271, 109,273, 103,279, 99,305, 92,316, 85,327, 83,335,
                                    89,340, 97,341, 94,336, 101,336, 96,331, 103,330, 97,327, 108,325,
                                    99,322, 109,321, 100,318, 110,317, 105,314, 110,312, 107,310, 113,308,
                                    105,306, 114,303, 105,301, 115,298, 107,295, 115,294, 108,293, 117,291,
                                    109,289, 117,286, 109,286, 118,283, 112,281, 118,279, 114,278,
                                    119,276, 115,274
   };

   init(l, 55, color, coords, ELEMENTS(coords)/2);
}

static void poly56(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {79,364, 74,359, 74,353, 76,347, 80,351, 83,356, 82,360
   };

   init(l, 56, color, coords, ELEMENTS(coords)/2);
}

static void poly57(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {91,363, 93,356, 97,353, 103,355, 105,360, 103,366, 99,371, 94,368
   };

   init(l, 57, color, coords, ELEMENTS(coords)/2);
}

static void poly58(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {110,355, 114,353, 118,357, 117,363, 113,369, 111,362
   };

   init(l, 58, color, coords, ELEMENTS(coords)/2);
}

static void poly59(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {126,354, 123,358, 124,367, 126,369, 129,361, 129,357
   };

   init(l, 59, color, coords, ELEMENTS(coords)/2);
}

static void poly60(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {30,154, 24,166, 20,182, 23,194, 29,208, 37,218, 41,210, 41,223,
                                    46,214, 46,227, 52,216, 52,227, 61,216, 59,225, 68,213, 73,219,
                                    70,207, 77,212, 69,200, 77,202, 70,194, 78,197, 68,187, 76,182,
                                    64,182, 58,175, 58,185, 53,177, 50,186, 46,171, 44,182, 39,167,
                                    36,172, 36,162, 30,166
   };

   init(l, 60, color, coords, ELEMENTS(coords)/2);
}

static void poly61(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {44,130, 41,137, 45,136, 43,150, 48,142, 48,157, 53,150,
                                    52,164, 60,156, 61,169, 64,165, 66,175, 70,167, 74,176,
                                    77,168, 80,183, 85,172, 90,182, 93,174, 98,181, 99,173,
                                    104,175, 105,169, 114,168, 102,163, 95,157, 94,166, 90,154,
                                    87,162, 82,149, 75,159, 72,148, 68,155, 67,143, 62,148, 62,138,
                                    58,145, 56,133, 52,142, 52,128, 49,134, 47,125
   };

   init(l, 61, color, coords, ELEMENTS(coords)/2);
}

static void poly62(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {13,216, 19,219, 36,231, 22,223, 16,222, 22,227, 12,224, 13,220, 16,220
   };

   init(l, 62, color, coords, ELEMENTS(coords)/2);
}

static void poly63(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {10,231, 14,236, 25,239, 27,237, 19,234
   };

   init(l, 63, color, coords, ELEMENTS(coords)/2);
}

static void poly64(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {9,245, 14,242, 25,245, 13,245
   };

   init(l, 64, color, coords, ELEMENTS(coords)/2);
}

static void poly65(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {33,255, 26,253, 18,254, 25,256, 18,258, 27,260, 18,263,
                                    27,265, 19,267, 29,270, 21,272, 29,276, 21,278, 30,281,
                                    22,283, 31,287, 24,288, 32,292, 23,293, 34,298, 26,299,
                                    37,303, 32,305, 39,309, 33,309, 39,314, 34,314, 40,318,
                                    34,317, 40,321, 34,321, 41,326, 33,326, 40,330, 33,332,
                                    39,333, 33,337, 42,337, 54,341, 49,337, 52,335, 47,330,
                                    50,330, 45,325, 49,325, 45,321, 48,321, 45,316, 46,306,
                                    45,286, 43,274, 36,261
   };

   init(l, 65, color, coords, ELEMENTS(coords)/2);
}

static void poly66(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {7,358, 9,351, 14,351, 17,359, 11,364
   };

   init(l, 66, color, coords, ELEMENTS(coords)/2);
}

static void poly67(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {44,354, 49,351, 52,355, 49,361
   };

   init(l, 67, color, coords, ELEMENTS(coords)/2);
}

static void poly68(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {32,357, 37,353, 40,358, 36,361
   };

   init(l, 68, color, coords, ELEMENTS(coords)/2);
}

static void poly69(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {139,334, 145,330, 154,330, 158,334, 154,341, 152,348,
                                    145,350, 149,340, 147,336, 141,339, 139,345, 136,342,
                                    136,339
   };

   init(l, 69, color, coords, ELEMENTS(coords)/2);
}

static void poly70(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {208,259, 215,259, 212,255, 220,259, 224,263, 225,274, 224,283,
                                    220,292, 208,300, 206,308, 203,304, 199,315, 197,309, 195,318,
                                    193,313, 190,322, 190,316, 185,325, 182,318, 180,325, 172,321,
                                    178,320, 176,313, 186,312, 180,307, 188,307, 184,303, 191,302,
                                    186,299, 195,294, 187,290, 197,288, 192,286, 201,283, 194,280,
                                    203,277, 198,275, 207,271, 200,269, 209,265, 204,265, 212,262
   };

   init(l, 70, color, coords, ELEMENTS(coords)/2);
}

static void poly71(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {106,126, 106,131, 109,132, 111,134, 115,132, 115,135, 119,133, 118,137,
                                    123,137, 128,137, 133,134, 136,130, 136,127, 132,124, 118,128, 112,128,
                                    106,126, 106,126, 106,126
   };

   init(l, 71, color, coords, ELEMENTS(coords)/2);
}

static void poly72(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {107,114, 101,110, 98,102, 105,97, 111,98, 119,102, 121,108, 118,112, 113,115
   };

   init(l, 72, color, coords, ELEMENTS(coords)/2);
}

static void poly73(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {148,106, 145,110, 146,116, 150,118, 152,111, 151,107
   };

   init(l, 73, color, coords, ELEMENTS(coords)/2);
}

static void poly74(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {80,55, 70,52, 75,58, 63,57, 72,61, 57,61, 67,66, 57,67, 62,69, 54,71,
                                    61,73, 54,77, 63,78, 53,85, 60,84, 56,90, 69,84, 63,82, 75,76, 70,75,
                                    77,72, 72,71, 78,69, 72,66, 81,67, 78,64, 82,63, 80,60, 86,62
   };

   init(l, 74, color, coords, ELEMENTS(coords)/2);
}

static void poly75(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {87,56, 91,52, 96,50, 102,56, 98,56, 92,60
   };

   init(l, 75, color, coords, ELEMENTS(coords)/2);
}

static void poly76(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {85,68, 89,73, 98,76, 106,74, 96,73, 91,70
   };

   init(l, 76, color, coords, ELEMENTS(coords)/2);
}

static void poly77(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {115,57, 114,64, 111,64, 115,75, 122,81, 122,74, 126,79,
                                    126,74, 131,78, 130,72, 133,77, 131,68, 126,61, 119,57
   };

   init(l, 77, color, coords, ELEMENTS(coords)/2);
}

static void poly78(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {145,48, 143,53, 147,59, 151,59, 150,55
   };

   init(l, 78, color, coords, ELEMENTS(coords)/2);
}

static void poly79(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {26,22, 34,15, 43,10, 52,10, 59,16, 47,15, 32,22
   };

   init(l, 79, color, coords, ELEMENTS(coords)/2);
}

static void poly80(struct lion *l)
{
   VGfloat color = 0xffe5b2;
   static const VGfloat coords[] = {160,19, 152,26, 149,34, 154,33, 152,30, 157,30, 155,26, 158,27,
                                    157,23, 161,23
   };

   init(l, 80, color, coords, ELEMENTS(coords)/2);
}

static void poly81(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {98,117, 105,122, 109,122, 105,117, 113,120, 121,120, 130,112, 128,108,
                                    123,103, 123,99, 128,101, 132,106, 135,109, 142,105, 142,101, 145,101,
                                    145,91, 148,101, 145,105, 136,112, 135,116, 143,124, 148,120, 150,122,
                                    142,128, 133,122, 121,125, 112,126, 103,125, 100,129, 96,124
   };

   init(l, 81, color, coords, ELEMENTS(coords)/2);
}

static void poly82(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {146,118, 152,118, 152,115, 149,115
   };

   init(l, 82, color, coords, ELEMENTS(coords)/2);
}

static void poly83(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {148,112, 154,111, 154,109, 149,109
   };

   init(l, 83, color, coords, ELEMENTS(coords)/2);
}

static void poly84(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {106,112, 108,115, 114,116, 118,114
   };

   init(l, 84, color, coords, ELEMENTS(coords)/2);
}

static void poly85(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {108,108, 111,110, 116,110, 119,108
   };

   init(l, 85, color, coords, ELEMENTS(coords)/2);
}

static void poly86(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {106,104, 109,105, 117,106, 115,104
   };

   init(l, 86, color, coords, ELEMENTS(coords)/2);
}

static void poly87(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {50,25, 41,26, 34,33, 39,43, 49,58, 36,51, 47,68, 55,69, 54,59,
                                    61,57, 74,46, 60,52, 67,42, 57,48, 61,40, 54,45, 60,36, 59,29,
                                    48,38, 52,30, 47,32
   };

   init(l, 87, color, coords, ELEMENTS(coords)/2);
}

static void poly88(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {147,34, 152,41, 155,49, 161,53, 157,47, 164,47, 158,43, 168,44,
                                    159,40, 164,37, 169,37, 164,33, 169,34, 165,28, 170,30, 170,25,
                                    173,29, 175,27, 176,32, 173,36, 175,39, 172,42, 172,46, 168,49,
                                    170,55, 162,57, 158,63, 155,58, 153,50, 149,46
   };

   init(l, 88, color, coords, ELEMENTS(coords)/2);
}

static void poly89(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {155,71, 159,80, 157,93, 157,102, 155,108, 150,101, 149,93,
                                    154,101, 152,91, 151,83, 155,79
   };

   init(l, 89, color, coords, ELEMENTS(coords)/2);
}

static void poly90(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {112,78, 115,81, 114,91, 112,87, 113,82
   };

   init(l, 90, color, coords, ELEMENTS(coords)/2);
}

static void poly91(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {78,28, 64,17, 58,11, 47,9, 36,10, 28,16, 21,26, 18,41,
                                    20,51, 23,61, 33,65, 28,68, 37,74, 36,81, 43,87, 48,90,
                                    43,100, 40,98, 39,90, 31,80, 30,72, 22,71, 17,61, 14,46,
                                    16,28, 23,17, 33,9, 45,6, 54,6, 65,12
   };

   init(l, 91, color, coords, ELEMENTS(coords)/2);
}

static void poly92(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {67,18, 76,9, 87,5, 101,2, 118,3, 135,8, 149,20, 149,26,
                                    144,19, 132,12, 121,9, 105,7, 89,8, 76,14, 70,20
   };

   init(l, 92, color, coords, ELEMENTS(coords)/2);
}

static void poly93(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {56,98, 48,106, 56,103, 47,112, 56,110, 52,115, 57,113, 52,121, 62,115,
                                    58,123, 65,119, 63,125, 69,121, 68,127, 74,125, 74,129, 79,128, 83,132,
                                    94,135, 93,129, 85,127, 81,122, 76,126, 75,121, 71,124, 71,117, 66,121,
                                    66,117, 62,117, 64,112, 60,113, 60,110, 57,111, 61,105, 57,107, 60,101,
                                    55,102
   };

   init(l, 93, color, coords, ELEMENTS(coords)/2);
}

static void poly94(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {101,132, 103,138, 106,134, 106,139, 112,136, 111,142, 115,139,
                                    114,143, 119,142, 125,145, 131,142, 135,138, 140,134, 140,129,
                                    143,135, 145,149, 150,171, 149,184, 145,165, 141,150, 136,147,
                                    132,151, 131,149, 126,152, 125,150, 121,152, 117,148, 111,152,
                                    110,148, 105,149, 104,145, 98,150, 96,138, 94,132, 94,130, 98,132
   };

   init(l, 94, color, coords, ELEMENTS(coords)/2);
}

static void poly95(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {41,94, 32,110, 23,132, 12,163, 6,190, 7,217, 5,236,
                                    3,247, 9,230, 12,211, 12,185, 18,160, 26,134, 35,110,
                                    43,99
   };

   init(l, 95, color, coords, ELEMENTS(coords)/2);
}

static void poly96(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {32,246, 41,250, 50,257, 52,267, 53,295, 53,323, 59,350,
                                    54,363, 51,365, 44,366, 42,360, 40,372, 54,372, 59,366,
                                    62,353, 71,352, 75,335, 73,330, 66,318, 68,302, 64,294,
                                    67,288, 63,286, 63,279, 59,275, 58,267, 56,262, 50,247,
                                    42,235, 44,246, 32,236, 35,244
   };

   init(l, 96, color, coords, ELEMENTS(coords)/2);
}

static void poly97(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {134,324, 146,320, 159,322, 173,327, 179,337, 179,349,
                                    172,355, 158,357, 170,350, 174,343, 170,333, 163,328, 152,326,
                                    134,329
   };

   init(l, 97, color, coords, ELEMENTS(coords)/2);
}

static void poly98(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {173,339, 183,334, 184,338, 191,329, 194,332, 199,323, 202,325,
                                    206,318, 209,320, 213,309, 221,303, 228,296, 232,289, 234,279,
                                    233,269, 230,262, 225,256, 219,253, 208,252, 198,252, 210,249,
                                    223,250, 232,257, 237,265, 238,277, 238,291, 232,305, 221,323,
                                    218,335, 212,342, 200,349, 178,348
   };

   init(l, 98, color, coords, ELEMENTS(coords)/2);
}

static void poly99(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {165,296, 158,301, 156,310, 156,323, 162,324, 159,318,
                                    162,308, 162,304
   };

   init(l, 99, color, coords, ELEMENTS(coords)/2);
}

static void poly100(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {99,252, 105,244, 107,234, 115,228, 121,228, 131,235,
                                    122,233, 113,235, 109,246, 121,239, 133,243, 121,243,
                                    110,251
   };

   init(l, 100, color, coords, ELEMENTS(coords)/2);
}

static void poly101(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {117,252, 124,247, 134,249, 136,253, 126,252
   };

   init(l, 101, color, coords, ELEMENTS(coords)/2);
}

static void poly102(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {117,218, 132,224, 144,233, 140,225, 132,219, 117,218,
                                    117,218, 117,218
   };

   init(l, 102, color, coords, ELEMENTS(coords)/2);
}

static void poly103(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {122,212, 134,214, 143,221, 141,213, 132,210
   };

   init(l, 103, color, coords, ELEMENTS(coords)/2);
}

static void poly104(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {69,352, 70,363, 76,373, 86,378, 97,379, 108,379, 120,377,
                                    128,378, 132,373, 135,361, 133,358, 132,366, 127,375, 121,374,
                                    121,362, 119,367, 117,374, 110,376, 110,362, 107,357, 106,371,
                                    104,375, 97,376, 90,375, 90,368, 86,362, 83,364, 86,369, 85,373,
                                    78,370, 73,362, 71,351
   };

   init(l, 104, color, coords, ELEMENTS(coords)/2);
}

static void poly105(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {100,360, 96,363, 99,369, 102,364
   };

   init(l, 105, color, coords, ELEMENTS(coords)/2);
}

static void poly106(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {115,360, 112,363, 114,369, 117,364
   };

   init(l, 106, color, coords, ELEMENTS(coords)/2);
}

static void poly107(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {127,362, 125,364, 126,369, 128,365
   };

   init(l, 107, color, coords, ELEMENTS(coords)/2);
}

static void poly108(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {5,255, 7,276, 11,304, 15,320, 13,334, 6,348, 2,353, 0,363,
                                    5,372, 12,374, 25,372, 38,372, 44,369, 42,367, 36,368, 31,369,
                                    30,360, 27,368, 20,370, 16,361, 15,368, 10,369, 3,366, 3,359, 6,352,
                                    11,348, 17,331, 19,316, 12,291, 9,274
   };

   init(l, 108, color, coords, ELEMENTS(coords)/2);
}

static void poly109(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {10,358, 7,362, 10,366, 11,362
   };

   init(l, 109, color, coords, ELEMENTS(coords)/2);
}

static void poly110(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {25,357, 22,360, 24,366, 27,360
   };

   init(l, 110, color, coords, ELEMENTS(coords)/2);
}

static void poly111(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {37,357, 34,361, 36,365, 38,361
   };

   init(l, 111, color, coords, ELEMENTS(coords)/2);
}

static void poly112(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {49,356, 46,359, 47,364, 50,360
   };

   init(l, 112, color, coords, ELEMENTS(coords)/2);
}

static void poly113(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {130,101, 132,102, 135,101, 139,102, 143,103,
                                    142,101, 137,100, 133,100
   };

   init(l, 113, color, coords, ELEMENTS(coords)/2);
}

static void poly114(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {106,48, 105,52, 108,56, 109,52
   };

   init(l, 114, color, coords, ELEMENTS(coords)/2);
}

static void poly115(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {139,52, 139,56, 140,60, 142,58, 141,56
   };

   init(l, 115, color, coords, ELEMENTS(coords)/2);
}

static void poly116(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {25,349, 29,351, 30,355, 33,350, 37,348, 42,351, 45,347,
                                    49,345, 44,343, 36,345
   };

   init(l, 116, color, coords, ELEMENTS(coords)/2);
}

static void poly117(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {98,347, 105,351, 107,354, 109,349, 115,349, 120,353, 118,349,
                                    113,346, 104,346
   };

   init(l, 117, color, coords, ELEMENTS(coords)/2);
}

static void poly118(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {83,348, 87,352, 87,357, 89,351, 87,348
   };

   init(l, 118, color, coords, ELEMENTS(coords)/2);
}

static void poly119(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {155,107, 163,107, 170,107, 186,108, 175,109, 155,109
   };

   init(l, 119, color, coords, ELEMENTS(coords)/2);
}

static void poly120(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {153,114, 162,113, 175,112, 192,114, 173,114, 154,115
   };

   init(l, 120, color, coords, ELEMENTS(coords)/2);
}

static void poly121(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {152,118, 164,120, 180,123, 197,129, 169,123, 151,120
   };

   init(l, 121, color, coords, ELEMENTS(coords)/2);
}

static void poly122(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {68,109, 87,106, 107,106, 106,108, 88,108
   };

   init(l, 122, color, coords, ELEMENTS(coords)/2);
}

static void poly123(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {105,111, 95,112, 79,114, 71,116, 85,115, 102,113
   };

   init(l, 123, color, coords, ELEMENTS(coords)/2);
}

static void poly124(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {108,101, 98,99, 87,99, 78,99, 93,100, 105,102
   };

   init(l, 124, color, coords, ELEMENTS(coords)/2);
}

static void poly125(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {85,63, 91,63, 97,60, 104,60, 108,62, 111,69, 112,75,
                                    110,74, 108,71, 103,73, 106,69, 105,65, 103,64, 103,67,
                                    102,70, 99,70, 97,66, 94,67, 97,72, 88,67, 84,66
   };

   init(l, 125, color, coords, ELEMENTS(coords)/2);
}

static void poly126(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {140,74, 141,66, 144,61, 150,61, 156,62, 153,70, 150,73,
                                    152,65, 150,65, 151,68, 149,71, 146,71, 144,66, 143,70,
                                    143,74
   };

   init(l, 126, color, coords, ELEMENTS(coords)/2);
}

static void poly127(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {146,20, 156,11, 163,9, 172,9, 178,14, 182,18, 184,32, 182,42,
                                    182,52, 177,58, 176,67, 171,76, 165,90, 157,105, 160,92, 164,85,
                                    168,78, 167,73, 173,66, 172,62, 175,59, 174,55, 177,53, 180,46,
                                    181,29, 179,21, 173,13, 166,11, 159,13, 153,18, 148,23
   };

   init(l, 127, color, coords, ELEMENTS(coords)/2);
}

static void poly128(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {150,187, 148,211, 150,233, 153,247, 148,267, 135,283, 125,299,
                                    136,292, 131,313, 122,328, 122,345, 129,352, 133,359, 133,367,
                                    137,359, 148,356, 140,350, 131,347, 129,340, 132,332, 140,328,
                                    137,322, 140,304, 154,265, 157,244, 155,223, 161,220, 175,229,
                                    186,247, 185,260, 176,275, 178,287, 185,277, 188,261, 196,253,
                                    189,236, 174,213
   };

   init(l, 128, color, coords, ELEMENTS(coords)/2);
}

static void poly129(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {147,338, 142,341, 143,345, 141,354, 147,343
   };

   init(l, 129, color, coords, ELEMENTS(coords)/2);
}

static void poly130(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {157,342, 156,349, 150,356, 157,353, 163,346, 162,342
   };

   init(l, 130, color, coords, ELEMENTS(coords)/2);
}

static void poly131(struct lion *l)
{
   VGfloat color = 0x000000;
   static const VGfloat coords[] = {99,265, 96,284, 92,299, 73,339, 73,333, 87,300
   };

   init(l, 131, color, coords, ELEMENTS(coords)/2);
}


struct lion * lion_create(void)
{
   struct lion *l = calloc(1, sizeof(struct lion));

   poly0(l);
   poly1(l);
   poly2(l);
   poly3(l);
   poly4(l);
   poly5(l);
   poly6(l);
   poly7(l);
   poly8(l);
   poly9(l);

   poly10(l);
   poly11(l);
   poly12(l);
   poly13(l);
   poly14(l);
   poly15(l);
   poly16(l);
   poly17(l);
   poly18(l);
   poly19(l);

   poly20(l);
   poly21(l);
   poly22(l);
   poly23(l);
   poly24(l);
   poly25(l);
   poly26(l);
   poly27(l);
   poly28(l);
   poly29(l);

   poly30(l);
   poly31(l);
   poly32(l);
   poly33(l);
   poly34(l);
   poly35(l);
   poly36(l);
   poly37(l);
   poly38(l);
   poly39(l);

   poly40(l);
   poly41(l);
   poly42(l);
   poly43(l);
   poly44(l);
   poly45(l);
   poly46(l);
   poly47(l);
   poly48(l);
   poly49(l);

   poly50(l);
   poly51(l);
   poly52(l);
   poly53(l);
   poly54(l);
   poly55(l);
   poly56(l);
   poly57(l);
   poly58(l);
   poly59(l);

   poly60(l);
   poly61(l);
   poly62(l);
   poly63(l);
   poly64(l);
   poly65(l);
   poly66(l);
   poly67(l);
   poly68(l);
   poly69(l);

   poly70(l);
   poly71(l);
   poly72(l);
   poly73(l);
   poly74(l);
   poly75(l);
   poly76(l);
   poly77(l);
   poly78(l);
   poly79(l);

   poly80(l);
   poly81(l);
   poly82(l);
   poly83(l);
   poly84(l);
   poly85(l);
   poly86(l);
   poly87(l);
   poly88(l);
   poly89(l);

   poly90(l);
   poly91(l);
   poly92(l);
   poly93(l);
   poly94(l);
   poly95(l);
   poly96(l);
   poly97(l);
   poly98(l);
   poly99(l);

   poly100(l);
   poly101(l);
   poly102(l);
   poly103(l);
   poly104(l);
   poly105(l);
   poly106(l);
   poly107(l);
   poly108(l);
   poly109(l);

   poly110(l);
   poly111(l);
   poly112(l);
   poly113(l);
   poly114(l);
   poly115(l);
   poly116(l);
   poly117(l);
   poly118(l);
   poly119(l);

   poly120(l);
   poly121(l);
   poly122(l);
   poly123(l);
   poly124(l);
   poly125(l);
   poly126(l);
   poly127(l);
   poly128(l);
   poly129(l);

   poly130(l);
   poly131(l);

   return l;
}

void lion_render(struct lion *l)
{
   VGint i;

   for (i = 0; i < LION_SIZE; ++i) {
      vgSetPaint(l->fills[i], VG_FILL_PATH);
      vgDrawPath(l->paths[i], VG_FILL_PATH);
   }
}

void lion_destroy(struct lion *l)
{
   VGint i;
   for (i = 0; i < LION_SIZE; ++i) {
      vgDestroyPaint(l->fills[i]);
      vgDestroyPath(l->paths[i]);
   }
   free(l);
}
