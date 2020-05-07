#include "colors.h"

Vector3 colors::hsv_to_rgb(float h, float s, float v) {
    Vector3 out;
    if(s <= 0.0f) {
        out.x = v;
        out.y = v;
        out.z = v;
        return out;
    }
    float hh = h;
    if(hh >= 360.0f) hh = 0.0f;
    hh /= 60.0f;
    int i = (int)hh;
    float ff = hh - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - (s * ff));
    float t = v * (1.0f - (s * (1.0f - ff)));

    switch(i) {
    case 0:
        out.x = v;
        out.y = t;
        out.z = p;
        break;
    case 1:
        out.x = q;
        out.y = v;
        out.z = p;
        break;
    case 2:
        out.x = p;
        out.y = v;
        out.z = t;
        break;

    case 3:
        out.x = p;
        out.y = q;
        out.z = v;
        break;
    case 4:
        out.x = t;
        out.y = p;
        out.z = v;
        break;
    case 5:
    default:
        out.x = v;
        out.y = p;
        out.z = q;
        break;
    }
    return out;     
}
