#include "raylib.h"
#include "raymath.h"
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>

// Night House: a small, original, Granny-style first-person horror prototype.
// All meshes, textures and short sound effects are generated at runtime, so the
// repository stays source-only and builds without external art assets.

struct Box { Vector3 min, max; };
struct Pickup { Vector3 pos; bool taken = false; };

static float ClampF(float v, float a, float b) { return std::max(a, std::min(b, v)); }
static float DistXZ(Vector3 a, Vector3 b) {
    float x=a.x-b.x, z=a.z-b.z; return std::sqrt(x*x+z*z);
}
static bool InsideXZ(Vector3 p, const Box& b, float r=0.0f) {
    return p.x > b.min.x-r && p.x < b.max.x+r && p.z > b.min.z-r && p.z < b.max.z+r;
}

static Texture2D MakeNoiseTexture(int w, int h, int seed) {
    Image im = GenImageColor(w,h,BLANK);
    Color *px = LoadImageColors(im);
    unsigned int s = (unsigned int)seed * 747796405u + 2891336453u;
    for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
        s ^= s >> 16; s *= 2246822519u; s ^= s >> 13;
        unsigned char n = (unsigned char)(s & 255u);
        unsigned char base = (unsigned char)(48 + n/3);
        px[y*w+x] = Color{(unsigned char)(base+8),(unsigned char)(base+4),(unsigned char)(base),255};
    }
    UpdateTextureFromImage(im, px);
    UnloadImageColors(px); UnloadImage(im);
    return LoadTextureFromImage(im); // unreachable after unload, retained only as documentation
}

static Texture2D MakeTexture(int w, int h, Color a, Color b, int seed) {
    Image im = GenImageColor(w,h,a);
    Color *px = LoadImageColors(im);
    unsigned int s = (unsigned int)seed * 747796405u + 2891336453u;
    for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
        s ^= s >> 16; s *= 2246822519u; s ^= s >> 13;
        int n=(int)(s&255u);
        bool stripe = ((x/8 + y/8 + seed) % 2)==0;
        if (n > 205 || stripe) px[y*w+x]=b;
    }
    UpdateTextureFromImage(im, px);
    Texture2D t=LoadTextureFromImage(im);
    UnloadImageColors(px); UnloadImage(im);
    return t;
}

static Sound MakeTone(float seconds, float hz, float volume, int sampleRate=22050) {
    int count=(int)(seconds*sampleRate);
    short* data=(short*)MemAlloc(sizeof(short)*count);
    for(int i=0;i<count;i++) {
        float t=(float)i/sampleRate;
        float env=1.0f-(float)i/count;
        float v=std::sin(2*PI*hz*t)*env*volume;
        data[i]=(short)(v*32767.0f);
    }
    Wave w{(unsigned int)count,(unsigned int)sampleRate,16,1,data};
    Sound s=LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

static void DrawWoodPanel(Vector3 pos, Vector3 size, Color c, Texture2D tex) {
    DrawCubeTexture(tex,pos,size.x,size.y,size.z,c);
    DrawCubeWires(pos,size.x,size.y,size.z,Color{25,22,20,255});
}

static bool MoveWithCollision(Vector3& p, Vector3 delta, float radius, const std::vector<Box>& walls) {
    Vector3 next=p; next.x += delta.x;
    for(const auto& b:walls) if(InsideXZ(next,b,radius)) { next.x=p.x; break; }
    p=next; next=p; next.z += delta.z;
    for(const auto& b:walls) if(InsideXZ(next,b,radius)) { next.z=p.z; break; }
    p=next; return true;
}

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1280,720,"NIGHT HOUSE | C++ / CMake");
    InitAudioDevice();
    SetTargetFPS(60);
    DisableCursor();

    // Procedural art: deliberately modest geometry, but with enough surface detail
    // to avoid the classic "debug cubes pretending to be a game" aesthetic.
    Texture2D wallTex = MakeTexture(64,64,Color{91,82,72,255},Color{122,105,84,255},7);
    Texture2D woodTex = MakeTexture(64,64,Color{66,43,31,255},Color{101,67,46,255},13);
    Texture2D floorTex= MakeTexture(64,64,Color{52,48,43,255},Color{78,72,62,255},31);
    Texture2D metalTex= MakeTexture(64,64,Color{55,58,58,255},Color{110,111,103,255},51);

    Sound pickupS=MakeTone(.13f,880,.20f);
    Sound doorS=MakeTone(.35f,110,.25f);
    Sound heartbeat=MakeTone(.18f,58,.13f);
    Sound hitS=MakeTone(.50f,42,.45f);

    Camera3D cam{};
    cam.position={0,1.65f,12}; cam.target={0,1.65f,11}; cam.up={0,1,0};
    cam.fovy=72; cam.projection=CAMERA_PERSPECTIVE;

    std::vector<Box> walls={
        {{-10,-1,-1},{-9,4,15}}, {{9,-1,-1},{10,4,15}},
        {{-10,-1,-2},{10,4,-1}}, {{-10,-1,15},{10,4,16}},
        {{-10,-1,5},{-2,4,6}}, {{2,-1,5},{10,4,6}},
        {{-10,-1,0},{-4,4,1}}, {{4,-1,0},{10,4,1}},
        {{-10,-1,9},{-6,4,10}}, {{-2,-1,9},{10,4,10}},
        {{-5,-1,-1},{-4,4,5}}, {{5,-1,-1},{6,4,5}},
        {{-1,-1,5},{0,4,9}},
    };

    std::vector<Pickup> fuses={{{-7,1,3}},{{6,1,7}},{{3,1,13}}};
    Vector3 exitPos={0,1.5f,-1.25f};
    Vector3 granny={0,1.0f,11.5f};
    Vector3 grannyTarget=granny;
    float grannyYaw=PI;
    float bob=0, stamina=1.0f, scare=0, flash=0, messageTimer=4;
    int collected=0; bool doorOpen=false, dead=false, won=false;
    bool hiding=false;

    float yaw=PI, pitch=0;
    SetMousePosition(GetScreenWidth()/2,GetScreenHeight()/2);

    while(!WindowShouldClose()) {
        float dt=GetFrameTime();
        if(dt>0.05f) dt=0.05f;

        if(dead || won) {
            BeginDrawing();
            ClearBackground(dead?Color{18,2,2,255}:Color{8,30,14,255});
            DrawText(dead?"SHE FOUND YOU":"YOU ESCAPED",420,270,54,dead?RAYWHITE:Color{170,255,190,255});
            DrawText(dead?"The house was never as empty as it looked.":"The front door finally gave way.",420,340,20,Color{190,190,190,255});
            DrawText("Press ENTER to play again",470,405,20,RAYWHITE);
            EndDrawing();
            if(IsKeyPressed(KEY_ENTER)) {
                dead=won=false; collected=0; stamina=1; granny={0,1,11.5f};
                cam.position={0,1.65f,12}; cam.target={0,1.65f,11};
                for(auto& f:fuses) f.taken=false;
            }
            continue;
        }

        Vector2 mouse=GetMouseDelta();
        yaw += mouse.x*0.0025f;
        pitch=ClampF(pitch-mouse.y*0.0022f,-1.35f,1.35f);
        Vector3 forward={std::sin(yaw),0,std::cos(yaw)};
        Vector3 right={std::cos(yaw),0,-std::sin(yaw)};
        cam.target=Vector3Add(cam.position,Vector3{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)});

        bool sprint=IsKeyDown(KEY_LEFT_SHIFT)&&stamina>0.03f&&!hiding;
        float speed=sprint?4.6f:2.65f;
        if(sprint) stamina=std::max(0.0f,stamina-dt*0.30f); else stamina=std::min(1.0f,stamina+dt*0.20f);
        Vector3 move{};
        if(IsKeyDown(KEY_W)) move=Vector3Add(move,forward);
        if(IsKeyDown(KEY_S)) move=Vector3Subtract(move,forward);
        if(IsKeyDown(KEY_D)) move=Vector3Add(move,right);
        if(IsKeyDown(KEY_A)) move=Vector3Subtract(move,right);
        if(Vector3Length(move)>0) move=Vector3Scale(Vector3Normalize(move),speed*dt);
        if(IsKeyPressed(KEY_E)) hiding=!hiding;
        if(!hiding) MoveWithCollision(cam.position,move,.34f,walls);

        // Pickups.
        for(auto& f:fuses) if(!f.taken && DistXZ(cam.position,f.pos)<1.15f) {
            f.taken=true; collected++; messageTimer=2.5f; PlaySound(pickupS);
        }
        if(collected==3 && DistXZ(cam.position,exitPos)<1.6f && IsKeyPressed(KEY_E)) {
            doorOpen=true; PlaySound(doorS); won=true;
        }

        // Enemy AI. It hears sprinting and gets more aggressive near the player.
        float d=DistXZ(granny,cam.position);
        bool alerted=d<9.5f || (sprint && d<16.0f);
        if(alerted && !hiding) grannyTarget=cam.position;
        else if(DistXZ(granny,grannyTarget)<1.0f || GetRandomValue(0,100)<1) {
            grannyTarget={ (float)GetRandomValue(-7,7),1,(float)GetRandomValue(2,13) };
        }
        Vector3 gd=Vector3Subtract(grannyTarget,granny); gd.y=0;
        if(Vector3Length(gd)>0.1f) {
            gd=Vector3Scale(Vector3Normalize(gd),(alerted?2.55f:1.05f)*dt);
            MoveWithCollision(granny,gd,.48f,walls);
            grannyYaw=std::atan2(gd.x,gd.z);
        }
        if(d<1.0f && !hiding) { dead=true; PlaySound(hitS); }
        if(d<7.0f && !hiding && GetRandomValue(0,50)==0) PlaySound(heartbeat);
        bob += dt*(Vector3Length(move)>0 ? 9.0f : 2.0f);

        BeginDrawing();
        ClearBackground(Color{8,8,10,255});
        BeginMode3D(cam);
        DrawPlane({0,0,7}, {20,22}, Color{54,50,44,255});
        DrawPlane({0,4,7}, {20,22}, Color{37,34,31,255});
        for(const auto& b:walls) {
            Vector3 c={(b.min.x+b.max.x)/2,1.5f,(b.min.z+b.max.z)/2};
            Vector3 s={b.max.x-b.min.x,3.0f,b.max.z-b.min.z};
            DrawWoodPanel(c,s,WHITE,wallTex);
        }

        // Furniture and readable environmental shapes.
        DrawWoodPanel({-7.5f,.75f,2.5f},{3,.9f,1.2f},WHITE,woodTex);
        DrawWoodPanel({7.5f,1.0f,3.0f},{2,2,.7f},WHITE,woodTex);
        DrawWoodPanel({-7.3f,1.0f,12.4f},{2.2f,2,.7f},WHITE,woodTex);
        DrawCubeTexture(metalTex,{7.2f,.65f,11.8f},1.4f,1.3f,1.4f,WHITE);
        DrawCubeWires({7.2f,.65f,11.8f},1.4f,1.3f,1.4f,Color{20,20,20,255});

        // Exit door.
        if(!doorOpen) {
            DrawCubeTexture(woodTex,exitPos,2.2f,3.0f,.28f,WHITE);
            DrawCube({0,1.5f,-1.05f},.12f,.12f,.12f,Color{180,150,70,255});
        }

        // Fuses: small emissive-looking glowing objects built from primitives.
        for(const auto& f:fuses) if(!f.taken) {
            float pulse=.95f+.12f*std::sin(GetTime()*4+f.pos.x);
            DrawCylinder(f.pos,.16f,.42f,12,Color{220,220,180,255});
            DrawSphere({f.pos.x,f.pos.y+.23f,f.pos.z},.12f* pulse,Color{245,235,160,255});
        }

        // Granny: deliberately original silhouette, not a copy of any specific game asset.
        float gy=granny.y;
        DrawCylinder({granny.x,gy+.85f,granny.z},.43f,.95f,12,Color{77,68,61,255});
        DrawSphere({granny.x,gy+1.55f,granny.z},.40f,Color{160,137,116,255});
        DrawSphere({granny.x-.14f,gy+1.60f,granny.z-.34f},.055f,Color{25,10,8,255});
        DrawSphere({granny.x+.14f,gy+1.60f,granny.z-.34f},.055f,Color{25,10,8,255});
        DrawCylinder({granny.x,gy+.20f,granny.z},.52f,.18f,12,Color{48,40,38,255});
        Vector3 arm={std::sin(grannyYaw)*.48f,gy+.85f,granny.z+std::cos(grannyYaw)*.48f};
        DrawCylinder(arm,.11f,.70f,10,Color{144,121,104,255});
        EndMode3D();

        // Dark vignette and HUD.
        DrawRectangle(0,0,GetScreenWidth(),80,Color{0,0,0,120});
        DrawText("NIGHT HOUSE",28,18,25,RAYWHITE);
        DrawText(TextFormat("FUSES %d / 3",collected),30,50,17,Color{200,195,180,255});
        DrawText("WASD move   SHIFT sprint   E interact / hide",360,24,17,Color{175,175,175,255});
        DrawRectangle(980,25,220,12,Color{35,35,35,220});
        DrawRectangle(980,25,(int)(220*stamina),12,RAYWHITE);
        DrawText("STAMINA",980,43,12,Color{170,170,170,255});
        if(collected==3) DrawText("The front door can be opened",390,650,22,Color{235,220,170,255});
        if(messageTimer>0) { DrawText("Something is wrong with this house.",390,610,22,Color{220,205,190,255}); messageTimer-=dt; }
        if(d<8 && !hiding) {
            float danger=ClampF((8-d)/8,0,1);
            DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Color{100,0,0,(unsigned char)(danger*45)});
            DrawText("RUN",600,92,24,Color{235,90,80,255});
        }
        if(hiding) DrawText("HIDDEN",585,575,18,Color{190,200,205,255});
        DrawCircle(GetScreenWidth()/2,GetScreenHeight()/2,2,RAYWHITE);
        EndDrawing();
    }

    UnloadTexture(wallTex); UnloadTexture(woodTex); UnloadTexture(floorTex); UnloadTexture(metalTex);
    UnloadSound(pickupS); UnloadSound(doorS); UnloadSound(heartbeat); UnloadSound(hitS);
    CloseAudioDevice(); CloseWindow();
    return 0;
}
