#pragma once
#include "graphics.h"
#include "maths.h"

namespace colors {
    Vector3 hsv_to_rgb(float h, float s, float v);

    Texture2D get_palette_magma();
}