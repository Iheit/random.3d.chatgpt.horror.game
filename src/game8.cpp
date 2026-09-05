#include "raylib.h"

static void DrawCabinet(Model& w, Model& m, Vector3 p);
static void DrawTable(Model& w, Model& m, Vector3 p);
static void DrawShelf(Model& w, Vector3 p);

static Vector2 CorrectedMouseDelta() {
    #undef GetMouseDelta
    Vector2 d = GetMouseDelta();
    #define GetMouseDelta CorrectedMouseDelta
    d.x = -d.x;
    return d;
}

static bool CorrectedIsKeyDown(int key) {
    #undef IsKeyDown
    const int mapped = (key == KEY_A) ? KEY_D : ((key == KEY_D) ? KEY_A : key);
    const bool result = IsKeyDown(mapped);
    #define IsKeyDown CorrectedIsKeyDown
    return result;
}

#define GetMouseDelta CorrectedMouseDelta
#define IsKeyDown CorrectedIsKeyDown
#include "game7.cpp"

static void DrawCabinet(Model& w, Model& m, Vector3 p) {
    DrawModelEx(w, {p.x,p.y+1.0f,p.z}, {0,1,0}, 0.0f, {1.45f,2.0f,.70f}, WHITE);
    for (float y=.48f; y<1.75f; y+=.42f)
        DrawModelEx(m, {p.x,p.y+y,p.z-.36f}, {0,1,0}, 0.0f, {.11f,.06f,.04f}, {175,145,90,255});
}

static void DrawTable(Model& w, Model& m, Vector3 p) {
    DrawModelEx(w, {p.x,p.y+.70f,p.z}, {0,1,0}, 0.0f, {2.45f,.16f,1.05f}, WHITE);
    for (float x=-.95f; x<=.95f; x+=1.90f)
        DrawModelEx(m, {p.x+x,p.y+.35f,p.z}, {0,1,0}, 0.0f, {.13f,.70f,.13f}, WHITE);
    DrawModelEx(w, {p.x,p.y+.34f,p.z+.18f}, {0,1,0}, 0.0f, {1.35f,.10f,.72f}, WHITE);
}

static void DrawShelf(Model& w, Vector3 p) {
    for (float y=.45f; y<=1.90f; y+=.55f)
        DrawModelEx(w, {p.x,p.y+y,p.z}, {0,1,0}, 0.0f, {2.30f,.12f,.36f}, WHITE);
    for (float x=-.8f; x<=.8f; x+=.4f)
        DrawModelEx(w, {p.x+x,p.y+.82f,p.z}, {0,1,0}, 0.0f, {.22f,.34f,.28f}, {112,61,39,255});
}
