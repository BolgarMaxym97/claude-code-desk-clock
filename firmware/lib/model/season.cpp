// Season background for the clock stage (board "Pack3" 82-85): calm when the mascot rests, a little motion
// only while it plays an action. Colours: fixed slots + S_F (green, reserved in every palette).
#include <math.h>
#include "scene.h"
// Coordinates are screen px; the clock shows x 43..195 of the stage (it is drawn 1.2x larger and cropped).

void seasonLayer(Canvas &c, int month, uint32_t t, bool moving) {
    if (month >= 9 && month <= 11) {  // autumn: leaf pile, a few falling leaves
        const uint8_t L[3] = {S_BODY, S_HATR, S_LID};
        for (int i = 0; i < 10; i++) R(c, 47 + i * 15, 144 + (i % 2), 8, 4, L[i % 3], 2);
        if (moving)
            for (int i = 0; i < 3; i++)
                R(c, 52 + (i * 61) % 130 + 6 * sinf((t + i * 5) / 4.0f), 80 + (t * (1 + i % 2) + i * 23) % 60, 5, 4, L[i], 2);
    } else if (month >= 3 && month <= 5) {  // spring: flowers along the bottom, a drifting petal
        for (int i = 0; i < 8; i++) {
            R(c, 52 + i * 20, 138, 2, 9, S_F);
            R(c, 50 + i * 20, 134, 6, 4, i % 2 ? S_HATR : S_HATW, 2);
        }
        if (moving) R(c, 190 - (t * 2) % 144, 92 + 8 * sinf(t / 5.0f), 3, 2, S_HATW);
    } else if (month >= 6 && month <= 8) {  // summer: grass tufts, small sun, a butterfly
        for (int i = 0; i < 12; i++) R(c, 48 + i * 13, 142 + (i % 3), 3, 7, S_F);
        R(c, 180, 84, 10, 10, S_INK, 5);
        if (moving) {
            float bx = 110 + 55 * sinf(t / 9.0f), by = 98 + 6 * sinf(t / 4.0f);
            int w = (t % 2) ? 5 : 2;
            R(c, bx - w, by, w, 5, S_HATW, 2);
            R(c, bx + 2, by, w, 5, S_HATW, 2);
        }
    } else {  // winter (outside the New Year season, which has its own snow): frost in the corners
        const int C[4][2] = {{46, 82}, {180, 82}, {46, 140}, {180, 140}};
        for (auto &k : C)
            for (int j = 0; j < 6; j++) R(c, k[0] + (j * 7) % 14, k[1] + (j * 5) % 8, 2, 2, S_HATW);
        if (moving)
            for (int i = 0; i < 4; i++) R(c, 50 + (i * 47) % 140, 80 + (t + i * 17) % 60, 2, 2, S_INK);
    }
}
