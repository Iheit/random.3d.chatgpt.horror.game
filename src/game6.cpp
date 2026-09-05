#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <vector>

enum class EnemyState { Patrol, Investigate, Chase, Search, Windup };
struct Wall { Vector3 p, s; };
struct Fuse { Vector3 p; bool taken = false; };
struct HideSpot { Vector3 p; float radius; };

static float FlatDist(Vector3 a, Vector3 b) {
    a.y = b.y = 0.0f;
    return Vector3Distance(a, b);
}

static bool Blocked(Vector3 p, const Wall& w, float r) {
    return p.x > w.p.x - w.s.x * 0.5f - r && p.x < w.p.x + w.s.x * 0.5f + r &&
           p.z > w.p.z - w.s.z * 0.5f - r && p.z < w.p.z + w.s.z * 0.5f + r;
}

static void MoveBody(Vector3& p, Vector3 d, float r, const std::vector<Wall>& walls) {
    Vector3 q = p;
    q.x += d.x;
    for (const auto& w : walls) {
        if (Blocked(q, w, r)) { q.x = p.x; break; }
    }
    p = q;
    q = p;
    q.z += d.z;
    for (const auto& w : walls) {
        if (Blocked(q, w, r)) { q.z = p.z; break; }
    }
    p = q;
}

static bool LOS(Vector3 a, Vector3 b, const std::vector<Wall>& walls) {
    Vector3 d = Vector3Subtract(b, a);
    float len = Vector3Length(d);
    if (len < 0.1f) return true;
    d = Vector3Scale(d, 1.0f / len);
    for (int i = 1; i < (int)(len / 0.16f); ++i) {
        Vector3 p = Vector3Add(a, Vector3Scale(d, i * 0.16f));
        for (const auto& w : walls) {
            if (Blocked(p, w, 0.035f)) return false;
        }
    }
    return true;
}

static Texture2D MakeTexture(Color base, Color accent, int seed) {
    Image im = GenImageColor(96, 96, base);
    Color* px = (Color*)im.data;
    unsigned s = (unsigned)seed * 747796405u + 2891336453u;
    for (int y = 0; y < 96; ++y) {
        for (int x = 0; x < 96; ++x) {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            int n = s & 255;
            bool scratch = ((x * 9 + y * 5 + seed * 13) % 71 == 0);
            if (n > 232 || scratch) px[y * 96 + x] = accent;
            else if (n < 18) px[y * 96 + x] = {
                (unsigned char)(base.r * 0.55f),
                (unsigned char)(base.g * 0.55f),
                (unsigned char)(base.b * 0.55f), 255
            };
        }
    }
    Texture2D t = LoadTextureFromImage(im);
    UnloadImage(im);
    return t;
}

static Model MakeBox(Texture2D t) {
    Model m = LoadModelFromMesh(GenMeshCube(1, 1, 1));
    m.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = t;
    return m;
}

static void Box(Model& m, Vector3 p, Vector3 s, Color tint = WHITE) {
    DrawModelEx(m, p, {0, 1, 0}, 0, s, tint);
}

static Sound Tone(float sec, float hz, float vol) {
    int n = std::max(1, (int)(sec * 22050));
    short* data = (short*)MemAlloc((size_t)n * sizeof(short));
    for (int i = 0; i < n; ++i) {
        float t = i / 22050.0f;
        float e = 1.0f - i / (float)n;
        data[i] = (short)(std::sin(2 * PI * hz * t) * e * vol * 32767);
    }
    Wave w{};
    w.frameCount = n;
    w.sampleRate = 22050;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = data;
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

static void DrawBed(Model& wood, Model& cloth, Vector3 p) {
    Box(wood, {p.x, p.y + 0.25f, p.z}, {2.8f, 0.5f, 4.2f});
    Box(cloth, {p.x, p.y + 0.56f, p.z + 0.15f}, {2.48f, 0.22f, 3.2f});
    Box(cloth, {p.x, p.y + 0.78f, p.z - 1.25f}, {2.25f, 0.22f, 0.72f}, {166, 154, 140, 255});
    for (float x = -1.05f; x <= 1.05f; x += 2.1f) {
        Box(wood, {p.x + x, p.y + 0.10f, p.z - 1.75f}, {0.16f, 0.50f, 0.16f});
    }
}

static void DrawDesk(Model& wood, Model& metal, Vector3 p) {
    Box(wood, {p.x, p.y + 0.73f, p.z}, {2.9f, 0.16f, 1.10f});
    for (float x = -1.12f; x <= 1.12f; x += 2.24f) Box(metal, {p.x + x, p.y + 0.36f, p.z}, {0.13f, 0.75f, 0.13f});
    Box(wood, {p.x, p.y + 0.37f, p.z + 0.20f}, {1.55f, 0.10f, 0.80f});
    Box(metal, {p.x, p.y + 1.08f, p.z - 0.28f}, {0.82f, 0.08f, 0.08f});
}

static void DrawCabinet(Model& wood, Model& metal, Vector3 p) {
    Box(wood, {p.x, p.y + 1.0f, p.z}, {1.50f, 2.0f, 0.72f});
    for (float y = 0.45f; y < 1.72f; y += 0.42f) Box(metal, {p.x, p.y + y, p.z - 0.38f}, {0.12f, 0.06f, 0.04f}, {178, 149, 92, 255});
}

static void DrawShelf(Model& wood, Vector3 p) {
    for (float y = 0.45f; y <= 2.0f; y += 0.52f) Box(wood, {p.x, p.y + y, p.z}, {2.6f, 0.12f, 0.40f});
    for (float x = -0.92f; x <= 0.92f; x += 0.46f) Box(wood, {p.x + x, p.y + 0.86f, p.z}, {0.25f, 0.36f, 0.30f}, {113, 62, 40, 255});
}

static void DrawTable(Model& wood, Model& metal, Vector3 p) {
    Box(wood, {p.x, p.y + 0.80f, p.z}, {2.6f, 0.16f, 1.30f});
    for (float x = -1.0f; x <= 1.0f; x += 2.0f)
        for (float z = -0.38f; z <= 0.38f; z += 0.76f)
            Box(metal, {p.x + x, p.y + 0.38f, p.z + z}, {0.12f, 0.76f, 0.12f});
}

static void DrawCloset(Model& wood, Vector3 p) {
    Box(wood, {p.x - 0.36f, p.y + 1.22f, p.z}, {0.18f, 2.44f, 0.74f});
    Box(wood, {p.x + 0.36f, p.y + 1.22f, p.z}, {0.18f, 2.44f, 0.74f});
    Box(wood, {p.x, p.y + 2.42f, p.z}, {0.90f, 0.18f, 0.78f});
    Box(wood, {p.x, p.y + 0.06f, p.z}, {0.90f, 0.12f, 0.78f});
}

static void DrawLamp(Model& metal, Vector3 p, bool flicker) {
    float f = flicker ? (0.78f + 0.22f * std::sin((float)GetTime() * 9.0f)) : 1.0f;
    Box(metal, {p.x, p.y + 0.10f, p.z}, {0.16f, 0.20f, 0.16f}, {64, 60, 52, 255});
    DrawSphere({p.x, p.y + 0.34f, p.z}, 0.16f, {(unsigned char)(210 * f), (unsigned char)(184 * f), (unsigned char)(124 * f), 255});
}

static void DrawEnemy(Vector3 e, float t) {
    float bob = std::sin(t * 4.2f) * 0.018f;
    Color coat = {60, 49, 46, 255};
    Color pants = {36, 31, 29, 255};
    Color skin = {148, 118, 100, 255};
    DrawCylinder({e.x, 0.10f + bob, e.z}, 0.43f, 0.49f, 1.02f, 16, coat);
    DrawCylinder({e.x - 0.22f, 0.03f + bob, e.z}, 0.105f, 0.12f, 0.62f, 12, pants);
    DrawCylinder({e.x + 0.22f, 0.03f + bob, e.z}, 0.105f, 0.12f, 0.62f, 12, pants);
    DrawSphere({e.x, 1.37f + bob, e.z}, 0.34f, skin);
    DrawSphere({e.x - 0.12f, 1.42f + bob, e.z - 0.305f}, 0.045f, {10, 3, 3, 255});
    DrawSphere({e.x + 0.12f, 1.42f + bob, e.z - 0.305f}, 0.045f, {10, 3, 3, 255});
    DrawCylinder({e.x, 1.63f + bob, e.z}, 0.39f, 0.42f, 0.16f, 16, {33, 25, 24, 255});
}

static void Crosshair() {
    int x = GetScreenWidth() / 2, y = GetScreenHeight() / 2;
    DrawCircle(x, y, 2.2f, {235, 235, 235, 210});
    DrawLine(x - 7, y, x - 3, y, {185, 185, 185, 130});
    DrawLine(x + 3, y, x + 7, y, {185, 185, 185, 130});
    DrawLine(x, y - 7, x, y - 3, {185, 185, 185, 130});
    DrawLine(x, y + 3, x, y + 7, {185, 185, 185, 130});
}

static void Spatial(Sound s, Vector3 src, Vector3 listener, Vector3 right) {
    float d = FlatDist(src, listener);
    float volume = Clamp(1.0f - d / 15.0f, 0.0f, 1.0f);
    Vector3 dir = {src.x - listener.x, 0, src.z - listener.z};
    float pan = Vector3Length(dir) > 0.01f ? Vector3DotProduct(Vector3Normalize(dir), right) : 0.0f;
    SetSoundVolume(s, volume * 0.38f);
    SetSoundPan(s, Clamp(0.5f + pan * 0.40f, 0.0f, 1.0f));
    PlaySound(s);
}

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Night House");
    InitAudioDevice();
    SetTargetFPS(60);
    DisableCursor();

    Texture2D wallTex = MakeTexture({77, 72, 64, 255}, {118, 102, 86, 255}, 3);
    Texture2D woodTex = MakeTexture({59, 37, 24, 255}, {104, 66, 39, 255}, 7);
    Texture2D metalTex = MakeTexture({49, 51, 50, 255}, {112, 108, 94, 255}, 11);
    Texture2D clothTex = MakeTexture({67, 65, 62, 255}, {110, 102, 89, 255}, 19);
    Model wall = MakeBox(wallTex), wood = MakeBox(woodTex), metal = MakeBox(metalTex), cloth = MakeBox(clothTex);

    Sound pickup = Tone(0.16f, 720, 0.28f);
    Sound doorSound = Tone(0.70f, 68, 0.46f);
    Sound heartbeat = Tone(0.12f, 53, 0.50f);
    Sound attack = Tone(0.60f, 34, 0.80f);
    Sound creak = Tone(0.85f, 132, 0.18f);
    Sound foot = Tone(0.07f, 92, 0.14f);

    // Roughly 42 x 34 meters with a central hall and six side rooms.
    std::vector<Wall> walls = {
        {{-21, 1.6f, 0},   {0.7f, 3.2f, 34}},
        {{ 21, 1.6f, 0},   {0.7f, 3.2f, 34}},
        {{  0, 1.6f, 17},  {42, 3.2f, 0.7f}},
        {{-12, 1.6f,-17},  {18, 3.2f, 0.7f}},
        {{ 12, 1.6f,-17},  {18, 3.2f, 0.7f}},
        {{-11, 1.6f,  8.5f},{18, 3.2f, 0.55f}},
        {{-11, 1.6f, -5.0f},{18, 3.2f, 0.55f}},
        {{ 11, 1.6f,  8.5f},{18, 3.2f, 0.55f}},
        {{ 11, 1.6f, -5.0f},{18, 3.2f, 0.55f}},
        {{-11, 1.6f,  2.1f},{0.55f,3.2f,12.2f}},
        {{ 11, 1.6f,  2.1f},{0.55f,3.2f,12.2f}},
        {{-3.0f,1.6f, 10.8f},{0.55f,3.2f, 12.4f}},
        {{ 3.0f,1.6f, 10.8f},{0.55f,3.2f, 12.4f}},
        {{-3.0f,1.6f,-10.8f},{0.55f,3.2f,12.4f}},
        {{ 3.0f,1.6f,-10.8f},{0.55f,3.2f,12.4f}}
    };

    std::vector<Fuse> fuses = {
        {{-15.7f, 0.95f, 12.0f}},
        {{ 15.8f, 0.95f, 11.7f}},
        {{ -7.2f, 0.95f,-10.7f}}
    };
    std::vector<HideSpot> hides = {
        {{-18.0f, 1.0f, 13.5f}, 1.30f},
        {{ 18.0f, 1.0f, 13.1f}, 1.30f},
        {{-16.8f, 1.0f,-10.5f}, 1.30f},
        {{ 16.8f, 1.0f,-10.5f}, 1.30f}
    };

    Camera3D cam{};
    cam.position = {0, 1.62f, 14.4f};
    cam.up = {0, 1, 0};
    cam.fovy = 70;
    cam.projection = CAMERA_PERSPECTIVE;

    Vector3 enemy = {0, 1, 0};
    Vector3 enemyFacing = {0, 0, 1};
    Vector3 target = {0, 1, 8};
    Vector3 lastSeen = enemy;
    Vector3 noisePos = enemy;
    EnemyState state = EnemyState::Patrol;
    std::vector<Vector3> patrol = {
        {0,1,10}, {0,1,1}, {0,1,-8}, {-7,1,-8}, {7,1,-8}, {0,1,1}, {0,1,10}
    };
    int patrolIndex = 0;

    float yaw = 0, pitch = 0, stamina = 100, battery = 100;
    float stateTimer = 0, attackTimer = 0, footTimer = 0, noiseTimer = 0, bob = 0;
    int got = 0;
    bool hiding = false, dead = false, won = false, menu = true, paused = false;
    bool doorOpen = false, flashlight = true;

    auto reset = [&]() {
        cam.position = {0, 1.62f, 14.4f};
        yaw = 0; pitch = 0;
        enemy = {0,1,0}; enemyFacing = {0,0,1}; target = {0,1,8}; lastSeen = enemy; noisePos = enemy;
        state = EnemyState::Patrol; patrolIndex = 0;
        stateTimer = attackTimer = footTimer = noiseTimer = bob = 0;
        stamina = battery = 100; got = 0;
        hiding = dead = won = doorOpen = false;
        for (auto& f : fuses) f.taken = false;
    };

    while (!WindowShouldClose()) {
        float dt = std::min(GetFrameTime(), 0.05f);
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        if (menu) {
            BeginDrawing();
            ClearBackground({7,8,10,255});
            DrawText("NIGHT HOUSE", 408, 132, 64, RAYWHITE);
            DrawText("A larger original survival-horror house", 442, 220, 20, {170,170,170,255});
            DrawText("ENTER  PLAY", 532, 320, 27, RAYWHITE);
            DrawText("WASD move   SHIFT sprint   E interact / hide   F flashlight   ESC pause", 266, 395, 17, {145,145,145,255});
            DrawText("F11 fullscreen", 540, 435, 17, {120,120,120,255});
            EndDrawing();
            if (IsKeyPressed(KEY_ENTER)) { menu = false; DisableCursor(); }
            continue;
        }

        if (IsKeyPressed(KEY_ESCAPE)) {
            paused = !paused;
            if (paused) EnableCursor(); else DisableCursor();
        }
        if (paused) {
            BeginDrawing();
            ClearBackground({8,9,11,235});
            DrawText("PAUSED", 535, 245, 52, RAYWHITE);
            DrawText("ESC resume", 548, 330, 20, {175,175,175,255});
            EndDrawing();
            continue;
        }

        if (dead || won) {
            BeginDrawing();
            ClearBackground(dead ? Color{23,3,6,255} : Color{5,25,10,255});
            DrawText(dead ? "SHE CAUGHT YOU" : "YOU ESCAPED", dead ? 390 : 430, 230, 60, RAYWHITE);
            DrawText(dead ? "The house was bigger than expected." : "You found your way out.", dead ? 420 : 455, 315, 20, {190,190,190,255});
            DrawText("ENTER restart", 525, 400, 21, RAYWHITE);
            EndDrawing();
            if (IsKeyPressed(KEY_ENTER)) reset();
            continue;
        }

        Vector2 mouse = GetMouseDelta();
        yaw += mouse.x * 0.00235f;
        pitch = Clamp(pitch - mouse.y * 0.0020f, -1.18f, 1.18f);
        Vector3 forward = {std::sin(yaw), 0, std::cos(yaw)};
        Vector3 right = {std::cos(yaw), 0, -std::sin(yaw)};
        cam.target = Vector3Add(cam.position, {
            std::sin(yaw) * std::cos(pitch),
            std::sin(pitch),
            std::cos(yaw) * std::cos(pitch)
        });

        bool nearHide = false;
        for (const auto& h : hides) if (FlatDist(cam.position, h.p) < h.radius) nearHide = true;
        if (IsKeyPressed(KEY_E) && nearHide) hiding = !hiding;

        bool moving = IsKeyDown(KEY_W) || IsKeyDown(KEY_A) || IsKeyDown(KEY_S) || IsKeyDown(KEY_D);
        bool sprint = IsKeyDown(KEY_LEFT_SHIFT) && moving && stamina > 1.0f && !hiding;
        float speed = sprint ? 4.0f : 2.35f;
        if (sprint) stamina = std::max(0.0f, stamina - dt * 30.0f);
        else stamina = std::min(100.0f, stamina + dt * 23.0f);

        Vector3 mv{};
        if (IsKeyDown(KEY_W)) mv = Vector3Add(mv, forward);
        if (IsKeyDown(KEY_S)) mv = Vector3Subtract(mv, forward);
        if (IsKeyDown(KEY_D)) mv = Vector3Add(mv, right);
        if (IsKeyDown(KEY_A)) mv = Vector3Subtract(mv, right);
        if (Vector3Length(mv) > 0.01f && !hiding) {
            MoveBody(cam.position, Vector3Scale(Vector3Normalize(mv), speed * dt), 0.34f, walls);
            bob += dt * (sprint ? 13.0f : 8.0f);
        } else bob = 0;
        cam.position.y = 1.62f + (moving && !hiding ? std::sin(bob) * 0.018f : 0);

        if (IsKeyPressed(KEY_F) && battery > 1.0f) flashlight = !flashlight;
        if (flashlight) battery = std::max(0.0f, battery - dt * 0.42f);

        if (moving && footTimer <= 0 && !hiding) {
            footTimer = sprint ? 0.25f : 0.46f;
            SetSoundVolume(foot, sprint ? 0.23f : 0.12f);
            PlaySound(foot);
            if (sprint) { noisePos = cam.position; noiseTimer = 1.05f; }
        } else footTimer -= dt;

        for (auto& f : fuses) {
            if (!f.taken && FlatDist(cam.position, f.p) < 1.10f && IsKeyPressed(KEY_E)) {
                f.taken = true;
                ++got;
                SetSoundVolume(pickup, 0.30f);
                PlaySound(pickup);
                noisePos = cam.position;
                noiseTimer = 1.5f;
            }
        }

        if (got == 3 && !doorOpen && cam.position.z < -15.2f && std::abs(cam.position.x) < 3.0f && IsKeyPressed(KEY_E)) {
            doorOpen = true;
            Spatial(doorSound, {0,1,-17}, cam.position, right);
            won = true;
        }
        noiseTimer = std::max(0.0f, noiseTimer - dt);

        float enemyDist = FlatDist(enemy, cam.position);
        Vector3 toPlayer = Vector3Subtract({cam.position.x,1.62f,cam.position.z}, {enemy.x,1.25f,enemy.z});
        if (Vector3Length(toPlayer) > 0.01f) toPlayer = Vector3Normalize(toPlayer);
        bool canSee = !hiding && enemyDist < 13.5f && Vector3DotProduct(toPlayer, enemyFacing) > 0.48f && LOS({enemy.x,1.25f,enemy.z}, {cam.position.x,1.62f,cam.position.z}, walls);
        bool heard = noiseTimer > 0 && FlatDist(enemy, noisePos) < (sprint ? 12.0f : 8.5f);

        if (canSee) { lastSeen = cam.position; state = EnemyState::Chase; stateTimer = 0; }
        else if (heard && state != EnemyState::Windup) { target = noisePos; state = EnemyState::Investigate; stateTimer = 0; }

        if (state == EnemyState::Chase) {
            target = cam.position;
            if (enemyDist > 16.0f || (!canSee && stateTimer > 2.5f)) {
                target = lastSeen;
                state = EnemyState::Search;
                stateTimer = 0;
            } else if (enemyDist < 1.85f && attackTimer <= 0 && !hiding) {
                state = EnemyState::Windup;
                attackTimer = 0.80f;
                stateTimer = 0;
            }
        }

        if (state == EnemyState::Windup) {
            attackTimer -= dt;
            if (attackTimer <= 0) {
                if (FlatDist(enemy, cam.position) < 1.35f && !hiding) {
                    Spatial(attack, enemy, cam.position, right);
                    dead = true;
                } else {
                    state = EnemyState::Search;
                    target = lastSeen;
                    stateTimer = 0;
                }
            }
        } else {
            stateTimer += dt;
            if (state == EnemyState::Investigate && FlatDist(enemy, target) < 0.70f) {
                state = EnemyState::Search; stateTimer = 0;
            }
            if (state == EnemyState::Search) {
                if (stateTimer > 4.2f) { state = EnemyState::Patrol; stateTimer = 0; }
                else if (FlatDist(enemy, target) < 0.75f) target = patrol[patrolIndex];
            }
            if (state == EnemyState::Patrol && FlatDist(enemy, target) < 0.75f) {
                patrolIndex = (patrolIndex + 1) % (int)patrol.size();
                target = patrol[patrolIndex];
            }
            Vector3 ev = Vector3Subtract(target, enemy);
            ev.y = 0;
            if (Vector3Length(ev) > 0.08f) {
                enemyFacing = Vector3Normalize(ev);
                float enemySpeed = state == EnemyState::Investigate ? 1.18f : (state == EnemyState::Search ? 0.90f : 0.62f);
                MoveBody(enemy, Vector3Scale(enemyFacing, enemySpeed * dt), 0.42f, walls);
            }
        }

        if (enemyDist < 5.5f && !hiding && GetRandomValue(0, 58) == 0) {
            SetSoundVolume(heartbeat, Clamp(1.0f - enemyDist / 6.5f, 0.0f, 1.0f) * 0.42f);
            PlaySound(heartbeat);
        }
        if (GetRandomValue(0, 330) == 0) PlaySound(creak);

        BeginDrawing();
        ClearBackground({4,5,7,255});
        BeginMode3D(cam);

        DrawPlane({0,0,0}, {42,34}, {38,35,31,255});
        DrawCube({0,3.08f,0}, 42, 0.14f, 34, {24,24,23,255});

        for (const auto& w : walls) {
            bool dark = std::abs(w.p.x) > 20.5f || std::abs(w.p.z) > 16.5f;
            Box(wall, w.p, w.s, dark ? Color{176,168,154,255} : Color{204,194,178,255});
            DrawCubeWires(w.p, w.s.x, w.s.y, w.s.z, {25,22,20,255});
        }

        for (int z = -16; z <= 16; ++z) Box(wood, {0,0.035f,(float)z}, {41.5f,0.045f,0.028f}, {72,48,34,255});

        // Foyer and dining room.
        DrawTable(wood, metal, {0,0,5.0f});
        DrawLamp(metal, {-8.0f,2.68f,10.6f}, true);
        DrawLamp(metal, {8.0f,2.68f,10.6f}, false);
        DrawLamp(metal, {0.0f,2.72f,1.5f}, true);

        // Left rooms.
        DrawBed(wood, cloth, {-16.0f,0,12.9f});
        DrawCabinet(wood, metal, {-19.0f,0,7.0f});
        DrawCloset(wood, {-17.7f,0,13.9f});
        DrawDesk(wood, metal, {-16.1f,0,-11.2f});
        DrawShelf(wood, {-19.0f,0,-9.6f});
        DrawCabinet(wood, metal, {-13.3f,0,-13.5f});
        DrawLamp(metal, {-16.0f,2.72f,-10.0f}, true);

        // Right rooms.
        DrawBed(wood, cloth, {16.0f,0,12.8f});
        DrawCabinet(wood, metal, {19.0f,0,7.0f});
        DrawCloset(wood, {17.7f,0,13.8f});
        DrawDesk(wood, metal, {16.0f,0,-11.3f});
        DrawShelf(wood, {19.0f,0,-9.5f});
        DrawTable(wood, metal, {14.7f,0,-14.0f});
        DrawLamp(metal, {16.0f,2.72f,-10.0f}, false);

        // A few corridor props make the center feel inhabited.
        Box(wood, {-5.15f,0.9f,4.0f}, {0.42f,1.8f,0.42f});
        Box(wood, { 5.15f,0.9f,4.0f}, {0.42f,1.8f,0.42f});
        DrawShelf(wood, {6.5f,0,3.5f});
        DrawShelf(wood, {-6.5f,0,3.5f});

        if (!doorOpen) {
            Box(wood, {-12.0f,1.6f,-17.0f}, {18.0f,3.2f,0.70f});
            Box(wood, { 12.0f,1.6f,-17.0f}, {18.0f,3.2f,0.70f});
            Box(metal, {0.7f,1.50f,-16.60f}, {0.09f,0.09f,0.09f}, {190,160,85,255});
        }

        for (const auto& f : fuses) if (!f.taken) {
            DrawCylinder(f.p, 0.16f, 0.16f, 0.42f, 12, {214,206,174,255});
            DrawSphere({f.p.x, f.p.y + 0.22f, f.p.z}, 0.13f, {235,220,110,255});
        }

        DrawCloset(wood, {-18.0f,0,13.5f});
        DrawCloset(wood, {18.0f,0,13.3f});
        DrawCloset(wood, {-16.8f,0,-10.5f});
        DrawCloset(wood, {16.8f,0,-10.5f});

        DrawEnemy(enemy, (float)GetTime());
        EndMode3D();

        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(), flashlight ? Color{0,0,0,22} : Color{0,0,0,110});
        DrawRectangle(0,0,1280,86,{0,0,0,185});
        DrawText(TextFormat("FUSES   %d / 3", got), 30, 20, 25, RAYWHITE);
        DrawText(hiding ? "HIDDEN" : "", 585, 22, 21, {185,185,185,255});
        DrawText("WASD move   SHIFT run   E interact / hide   F flashlight   ESC pause", 30, 55, 16, {155,155,155,255});
        DrawRectangle(1035,22,190,11,{28,28,28,255});
        DrawRectangle(1035,22,(int)(190*stamina/100.0f),11,{185,185,185,255});
        DrawText("STAMINA",1035,38,11,{150,150,150,255});
        DrawRectangle(1035,59,190,8,{28,28,28,255});
        DrawRectangle(1035,59,(int)(190*battery/100.0f),8,{155,155,145,255});
        DrawText("BATTERY",1035,71,10,{140,140,140,255});
        if (nearHide) DrawText(hiding ? "E leave hiding" : "E hide", 535, 620, 19, RAYWHITE);
        DrawText(got < 3 ? "Search the rooms for the three fuses." : "All fuses found. Reach the front door.", 410, 655, 19, {190,190,190,255});
        if (state == EnemyState::Windup) DrawText("TOO CLOSE", 560, 104, 17, {205,160,160,255});
        if (state == EnemyState::Chase) DrawText("SHE SEES YOU", 515, 104, 17, {205,160,160,255});
        Crosshair();
        EndDrawing();
    }

    UnloadSound(pickup); UnloadSound(doorSound); UnloadSound(heartbeat); UnloadSound(attack); UnloadSound(creak); UnloadSound(foot);
    UnloadModel(wall); UnloadModel(wood); UnloadModel(metal); UnloadModel(cloth);
    UnloadTexture(wallTex); UnloadTexture(woodTex); UnloadTexture(metalTex); UnloadTexture(clothTex);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
