#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <vector>

struct Wall { Vector3 p, s; };
struct Fuse { Vector3 p; bool got = false; };

static float FlatDistance(Vector3 a, Vector3 b) {
    a.y = b.y = 0;
    return Vector3Distance(a, b);
}

static bool HitsWall(Vector3 p, const Wall& w, float r) {
    return p.x > w.p.x - w.s.x * 0.5f - r && p.x < w.p.x + w.s.x * 0.5f + r &&
           p.z > w.p.z - w.s.z * 0.5f - r && p.z < w.p.z + w.s.z * 0.5f + r;
}

static void MoveWithCollision(Vector3& p, Vector3 v, float radius, const std::vector<Wall>& walls) {
    Vector3 q = p;
    q.x += v.x;
    for (const auto& w : walls) if (HitsWall(q, w, radius)) { q.x = p.x; break; }
    p = q;
    q = p;
    q.z += v.z;
    for (const auto& w : walls) if (HitsWall(q, w, radius)) { q.z = p.z; break; }
    p = q;
}

static Texture2D MakeTexture(Color base, Color detail, int seed) {
    Image image = GenImageColor(64, 64, base);
    Color* pixels = LoadImageColors(image);
    unsigned int state = (unsigned int)seed * 2654435761u + 17u;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            if (((x / 7 + y / 9 + seed) % 3) == 0 || (state & 255u) > 225u)
                pixels[y * 64 + x] = detail;
        }
    }
    UnloadImage(image);
    Image finalImage = GenImageColor(64, 64, base);
    UnloadImageColors(pixels);
    Color* finalPixels = LoadImageColors(finalImage);
    for (int i = 0; i < 4096; ++i) finalPixels[i] = (i % 11 == 0) ? detail : base;
    UnloadImage(finalImage);
    Image result = GenImageColor(64, 64, base);
    UnloadImageColors(finalPixels);
    Texture2D texture = LoadTextureFromImage(result);
    UnloadImage(result);
    return texture;
}

static Sound MakeTone(float seconds, float hz, float volume) {
    int samples = std::max(1, (int)(seconds * 22050.0f));
    short* data = (short*)MemAlloc((size_t)samples * sizeof(short));
    for (int i = 0; i < samples; ++i) {
        float t = (float)i / 22050.0f;
        float envelope = 1.0f - (float)i / (float)samples;
        data[i] = (short)(std::sin(2.0f * PI * hz * t) * envelope * volume * 32767.0f);
    }
    Wave wave{};
    wave.frameCount = (unsigned int)samples;
    wave.sampleRate = 22050;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = data;
    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return sound;
}

static void SpatialSound(Sound sound, Vector3 source, Vector3 listener, Vector3 right, float maxDistance = 14.0f) {
    float distance = FlatDistance(source, listener);
    float volume = Clamp(1.0f - distance / maxDistance, 0.0f, 1.0f);
    Vector3 direction = { source.x - listener.x, 0, source.z - listener.z };
    float pan = Vector3Length(direction) > 0.001f ? Vector3DotProduct(Vector3Normalize(direction), right) : 0.0f;
    SetSoundVolume(sound, volume * 0.45f);
    SetSoundPan(sound, Clamp(0.5f + pan * 0.35f, 0.0f, 1.0f));
}

static void DrawTexturedBox(Texture2D texture, Vector3 position, Vector3 size) {
    Mesh mesh = GenMeshCube(1.0f, 1.0f, 1.0f);
    Model model = LoadModelFromMesh(mesh);
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
    DrawModelEx(model, position, {0, 1, 0}, 0, size, WHITE);
    UnloadModel(model);
}

static void DrawFurniture(Texture2D wood, Texture2D cloth, Texture2D metal) {
    DrawTexturedBox(wood, {-7.2f, 0.75f, 2.5f}, {3.0f, 0.9f, 1.2f});
    DrawTexturedBox(wood, {7.1f, 1.0f, 3.1f}, {2.0f, 2.0f, 0.7f});
    DrawTexturedBox(cloth, {-7.2f, 1.0f, 12.5f}, {2.2f, 2.0f, 0.7f});
    DrawTexturedBox(metal, {6.8f, 0.7f, 12.7f}, {1.3f, 1.4f, 1.3f});
}

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Night House | C++ / CMake");
    InitAudioDevice();
    SetTargetFPS(60);
    DisableCursor();

    Texture2D wall = MakeTexture({76,70,62,255}, {113,96,76,255}, 3);
    Texture2D wood = MakeTexture({54,34,24,255}, {91,59,38,255}, 8);
    Texture2D metal = MakeTexture({52,55,55,255}, {115,116,106,255}, 13);
    Texture2D cloth = MakeTexture({65,65,66,255}, {105,100,92,255}, 21);

    Sound pickup = MakeTone(0.18f, 740.0f, 0.35f);
    Sound door = MakeTone(0.45f, 90.0f, 0.45f);
    Sound heartbeat = MakeTone(0.16f, 55.0f, 0.5f);
    Sound attack = MakeTone(0.6f, 38.0f, 0.8f);
    Sound creak = MakeTone(0.8f, 145.0f, 0.25f);

    std::vector<Wall> walls = {
        {{-9.6f,1.5f,7},{0.7f,3,18}}, {{9.6f,1.5f,7},{0.7f,3,18}},
        {{0,1.5f,-2},{20,3,0.7f}}, {{0,1.5f,16},{20,3,0.7f}},
        {{-6,1.5f,5},{7,3,0.7f}}, {{5.5f,1.5f,5},{8,3,0.7f}},
        {{-7,1.5f,10},{5,3,0.7f}}, {{5.5f,1.5f,11},{8,3,0.7f}},
        {{-4,1.5f,1.8f},{0.7f,3,4}}, {{4,1.5f,1.8f},{0.7f,3,4}},
        {{0,1.5f,8},{0.7f,3,6}}
    };

    std::vector<Fuse> fuses = {{{-7.5f,0.9f,2.8f}}, {{6.8f,0.9f,7.8f}}, {{2.8f,0.9f,14.0f}}};
    Camera3D camera{};
    camera.position = {0,1.65f,13.5f};
    camera.up = {0,1,0}; camera.fovy = 70; camera.projection = CAMERA_PERSPECTIVE;
    float yaw = PI, pitch = 0, stamina = 1;
    Vector3 granny = {0,1,12}, target = granny;
    int collected = 0;
    bool hiding = false, dead = false, won = false, doorOpen = false;
    float messageTime = 4;

    auto reset = [&]() {
        camera.position = {0,1.65f,13.5f}; yaw = PI; pitch = 0;
        granny = {0,1,12}; target = granny; collected = 0; stamina = 1;
        hiding = false; dead = false; won = false; doorOpen = false; messageTime = 4;
        for (auto& fuse : fuses) fuse.got = false;
    };

    while (!WindowShouldClose()) {
        float dt = std::min(GetFrameTime(), 0.05f);
        Vector3 forward = {std::sin(yaw),0,std::cos(yaw)};
        Vector3 right = {std::cos(yaw),0,-std::sin(yaw)};

        if (dead || won) {
            BeginDrawing();
            ClearBackground(dead ? Color{20,3,5,255} : Color{7,30,12,255});
            DrawText(dead ? "SHE FOUND YOU" : "YOU ESCAPED", 390, 260, 60, RAYWHITE);
            DrawText(dead ? "The house was not empty." : "The front door is finally open.", 400, 335, 22, Color{190,190,190,255});
            DrawText("ENTER: restart", 520, 410, 20, RAYWHITE);
            EndDrawing();
            if (IsKeyPressed(KEY_ENTER)) reset();
            continue;
        }

        Vector2 mouse = GetMouseDelta();
        yaw += mouse.x * 0.0025f;
        pitch = Clamp(pitch - mouse.y * 0.0022f, -1.35f, 1.35f);
        forward = {std::sin(yaw),0,std::cos(yaw)};
        right = {std::cos(yaw),0,-std::sin(yaw)};
        camera.target = Vector3Add(camera.position, {std::sin(yaw)*std::cos(pitch), std::sin(pitch), std::cos(yaw)*std::cos(pitch)});

        bool sprinting = IsKeyDown(KEY_LEFT_SHIFT) && stamina > 0.03f && !hiding;
        float speed = sprinting ? 4.7f : 2.6f;
        if (sprinting) stamina = std::max(0.0f, stamina - dt * 0.32f);
        else stamina = std::min(1.0f, stamina + dt * 0.2f);

        Vector3 movement{};
        if (IsKeyDown(KEY_W)) movement = Vector3Add(movement, forward);
        if (IsKeyDown(KEY_S)) movement = Vector3Subtract(movement, forward);
        if (IsKeyDown(KEY_D)) movement = Vector3Add(movement, right);
        if (IsKeyDown(KEY_A)) movement = Vector3Subtract(movement, right);
        if (Vector3Length(movement) > 0.001f) movement = Vector3Scale(Vector3Normalize(movement), speed * dt);
        if (IsKeyPressed(KEY_E)) hiding = !hiding;
        if (!hiding) MoveWithCollision(camera.position, movement, 0.34f, walls);

        for (auto& fuse : fuses) {
            if (!fuse.got && FlatDistance(camera.position, fuse.p) < 1.15f) {
                fuse.got = true; collected++; messageTime = 2.5f; PlaySound(pickup);
            }
        }
        if (collected == 3 && FlatDistance(camera.position, {0,1,-1.3f}) < 1.7f && IsKeyPressed(KEY_E)) {
            doorOpen = true; PlaySound(door); won = true;
        }

        float grannyDistance = FlatDistance(granny, camera.position);
        bool alert = grannyDistance < 9.0f || (sprinting && grannyDistance < 15.0f);
        if (alert && !hiding) target = camera.position;
        else if (FlatDistance(granny, target) < 0.8f || GetRandomValue(0,120) == 0)
            target = {(float)GetRandomValue(-7,7), 1, (float)GetRandomValue(2,14)};

        Vector3 enemyMove = Vector3Subtract(target, granny); enemyMove.y = 0;
        if (Vector3Length(enemyMove) > 0.1f) {
            enemyMove = Vector3Scale(Vector3Normalize(enemyMove), (alert ? 2.65f : 1.0f) * dt);
            MoveWithCollision(granny, enemyMove, 0.45f, walls);
        }

        if (grannyDistance < 1.0f && !hiding) {
            dead = true; SpatialSound(attack, granny, camera.position, right); PlaySound(attack);
        }
        if (grannyDistance < 6.0f && !hiding && GetRandomValue(0,45) == 0) {
            SpatialSound(heartbeat, granny, camera.position, right); PlaySound(heartbeat);
        }
        if (GetRandomValue(0,240) == 0) {
            Vector3 source = {(float)GetRandomValue(-8,8),1,(float)GetRandomValue(1,15)};
            SpatialSound(creak, source, camera.position, right); PlaySound(creak);
        }

        BeginDrawing();
        ClearBackground({7,7,9,255});
        BeginMode3D(camera);
        DrawPlane({0,0,7}, {20,22}, {47,43,38,255});
        for (const auto& wall : walls) {
            DrawCube(wall.p, wall.s.x, wall.s.y, wall.s.z, {48,43,38,255});
            DrawCubeWires(wall.p, wall.s.x, wall.s.y, wall.s.z, {20,18,17,255});
        }
        DrawFurniture(wood, cloth, metal);
        if (!doorOpen) {
            DrawCube({0,1.5f,-1.25f}, 2.2f, 3, 0.25f, {70,42,28,255});
            DrawSphere({0.55f,1.5f,-1.05f}, 0.09f, {180,150,70,255});
        }
        for (const auto& fuse : fuses) if (!fuse.got) {
            float pulse = 0.11f + 0.025f * std::sin((float)GetTime() * 5.0f);
            DrawCylinder(fuse.p, 0.16f, 0.16f, 0.42f, 12, {210,205,175,255});
            DrawSphere({fuse.p.x,fuse.p.y+0.23f,fuse.p.z}, pulse, {240,230,150,255});
        }
        DrawCylinder({granny.x,1.78f,granny.z}, 0.43f, 0.43f, 0.95f, 12, {70,63,60,255});
        DrawSphere({granny.x,2.48f,granny.z}, 0.4f, {155,132,112,255});
        DrawSphere({granny.x-0.14f,2.53f,granny.z-0.34f}, 0.055f, {18,8,7,255});
        DrawSphere({granny.x+0.14f,2.53f,granny.z-0.34f}, 0.055f, {18,8,7,255});
        EndMode3D();

        DrawRectangle(0,0,1280,78,{0,0,0,150});
        DrawText("NIGHT HOUSE",25,16,26,RAYWHITE);
        DrawText(TextFormat("FUSES %d / 3", collected),28,48,17,{200,195,180,255});
        DrawText("WASD move   SHIFT sprint   E hide/interact",370,23,17,{175,175,175,255});
        DrawRectangle(1010,23,210,12,{30,30,30,230});
        DrawRectangle(1010,23,(int)(210*stamina),12,RAYWHITE);
        if (collected == 3) DrawText("The front door can be opened",430,650,22,{235,220,170,255});
        if (hiding) DrawText("HIDING",580,690,18,{190,190,190,255});
        if (messageTime > 0) {
            DrawText("Find the three fuses. Do not let her hear you.",355,105,19,{210,200,180,255});
            messageTime -= dt;
        }
        EndDrawing();
    }

    UnloadTexture(wall); UnloadTexture(wood); UnloadTexture(metal); UnloadTexture(cloth);
    UnloadSound(pickup); UnloadSound(door); UnloadSound(heartbeat); UnloadSound(attack); UnloadSound(creak);
    CloseAudioDevice(); CloseWindow();
    return 0;
}
