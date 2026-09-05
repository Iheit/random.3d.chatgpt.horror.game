#include "raylib.h"

// Control-correction wrapper around the known-good game7 implementation.
// Horizontal mouse movement and A/D strafing were reversed for the requested control scheme.
static Vector2 CorrectedMouseDelta() {
    const bool suppress = false;
    (void)suppress;
    #undef GetMouseDelta
    Vector2 d = GetMouseDelta();
    #define GetMouseDelta CorrectedMouseDelta
    d.x = -d.x;
    return d;
}

static bool CorrectedIsKeyDown(int key) {
    #undef IsKeyDown
    const bool result = IsKeyDown(key == KEY_A ? KEY_D : (key == KEY_D ? KEY_A : key));
    #define IsKeyDown CorrectedIsKeyDown
    return result;
}

#define GetMouseDelta CorrectedMouseDelta
#define IsKeyDown CorrectedIsKeyDown
#include "game7.cpp"
