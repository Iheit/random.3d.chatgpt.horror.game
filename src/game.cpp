#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <vector>

struct Wall { Vector3 p, s; };
struct Fuse { Vector3 p; bool got = false; };

static float Dist(Vector3 a, Vector3 b) { a.y = b.y = 0; return Vector3Distance(a,b); }
static bool Blocked(Vector3 p, const Wall& w, float r) {
    return p.x > w.p.x-w.s.x*.5f-r && p.x < w.p.x+w.s.x*.5f+r && p.z > w.p.z-w.s.z*.5f-r && p.z < w.p.z+w.s.z*.5f+r;
}
static void Move(Vector3& p, Vector3 v, float r, const std::vector<Wall>& ws) {
    Vector3 q=p; q.x+=v.x; for(auto&w:ws) if(Blocked(q,w,r)){q.x=p.x;break;} p=q;
    q=p; q.z+=v.z; for(auto&w:ws) if(Blocked(q,w,r)){q.z=p.z;break;} p=q;
}
static Texture2D Tex(Color a, Color b, int seed) {
    Image im=GenImageColor(96,96,a); Color* px=LoadImageColors(im); unsigned s=seed*747796405u+2891336453u;
    for(int y=0;y<96;y++) for(int x=0;x<96;x++){s^=s<<13;s^=s>>17;s^=s<<5; int n=(int)(s&255); if(n>218 || ((x*13+y*7+seed*11)%97)<3) px[y*96+x]=b;}
    UnloadImage(im); Image out=GenImageColor(96,96,a); UnloadImageColors(px); Color* op=LoadImageColors(out);
    s=seed*747796405u+2891336453u; for(int y=0;y<96;y++) for(int x=0;x<96;x++){s^=s<<13;s^=s>>17;s^=s<<5; int n=s&255; op[y*96+x]=(n>230)?b:a;}
    Texture2D t=LoadTextureFromImage(out); UnloadImageColors(op); UnloadImage(out); return t;
}
static Sound Tone(float sec,float hz,float vol) { int n=std::max(1,(int)(sec*22050)); short*d=(short*)MemAlloc(n*sizeof(short)); for(int i=0;i<n;i++){float t=i/22050.f,e=1.f-i/(float)n; d[i]=(short)(sin(2*PI*hz*t)*e*vol*32767);} Wave w{};w.frameCount=n;w.sampleRate=22050;w.sampleSize=16;w.channels=1;w.data=d;Sound s=LoadSoundFromWave(w);UnloadWave(w);return s; }
static void Box(Texture2D t,Vector3 p,Vector3 s){Mesh m=GenMeshCube(1,1,1);Model md=LoadModelFromMesh(m);md.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture=t;DrawModelEx(md,p,{0,1,0},0,s,WHITE);UnloadModel(md);}
static void Furniture(Texture2D w,Texture2D c,Texture2D m){
    Box(w,{-7,0.65f,1.2f},{3.2f,1.3f,1.0f}); Box(w,{-7,1.45f,1.2f},{2.8f,.18f,.9f});
    for(float x=-8.1f;x<=-5.9f;x+=2.2f) Box(m,{x,.35f,1.2f},{.12f,.7f,.12f});
    Box(c,{6.6f,.8f,2.2f},{2.6f,1.6f,.9f}); Box(w,{6.6f,1.7f,2.2f},{2.8f,.2f,1.0f});
    Box(w,{-6.8f,1.0f,12.5f},{2.8f,2.0f,.8f}); Box(c,{-6.8f,2.15f,12.5f},{2.5f,.35f,.72f});
    Box(m,{6.7f,.75f,12.6f},{1.4f,1.5f,1.2f}); Box(m,{6.7f,1.6f,12.0f},{1.0f,.06f,.06f});
}
static void Lamp(Vector3 p){ DrawCylinder(p,.32f,.42f,.12f,16,{45,42,38,255}); DrawCylinder({p.x,p.y+.15f,p.z},.08f,.08f,.35f,12,{70,65,55,255}); DrawSphere({p.x,p.y+.37f,p.z},.28f,{170,155,120,255}); }

int main(){
    SetConfigFlags(FLAG_MSAA_4X_HINT); InitWindow(1280,720,"Night House"); InitAudioDevice(); SetTargetFPS(60); DisableCursor();
    Texture2D wall=Tex({70,66,60,255},{103,89,70,255},3), wood=Tex({52,32,22,255},{91,58,35,255},8), metal=Tex({48,50,49,255},{105,107,98,255},13), cloth=Tex({60,59,58,255},{102,94,83,255},21);
    Sound pickup=Tone(.16f,740,.3f), door=Tone(.5f,75,.45f), heartbeat=Tone(.12f,55,.5f), attack=Tone(.55f,38,.8f), creak=Tone(.75f,145,.22f), step=Tone(.08f,95,.18f);
    std::vector<Wall> walls={{{-9.6f,1.5f,7},{.7f,3,18}},{{9.6f,1.5f,7},{.7f,3,18}},{{0,1.5f,-2},{20,3,.7f}},{{0,1.5f,16},{20,3,.7f}},{{-6,1.5f,5},{7,3,.7f}},{{5.5f,1.5f,5},{8,3,.7f}},{{-7,1.5f,10},{5,3,.7f}},{{5.5f,1.5f,11},{8,3,.7f}},{{-4,1.5f,1.8f},{.7,3,4}},{{4,1.5f,1.8f},{.7,3,4}},{{0,1.5f,8},{.7,3,6}}};
    std::vector<Fuse> fuses={{{-7.5f,.9f,2.8f}},{{6.8f,.9f,7.8f}},{{2.8f,.9f,14}}};
    Camera3D cam{}; cam.position={0,1.65f,13.5f}; cam.up={0,1,0}; cam.fovy=72; cam.projection=CAMERA_PERSPECTIVE;
    float yaw=0,pitch=0,stamina=1,enemyPause=0; Vector3 enemy={0,1,10},target=enemy,lastSeen={0,0,0}; int got=0; bool hiding=false,dead=false,won=false; float msg=5;
    auto reset=[&](){cam.position={0,1.65f,13.5f};yaw=0;pitch=0;stamina=1;enemy={0,1,10};target=enemy;got=0;hiding=dead=won=false;msg=5;for(auto&f:fuses)f.got=false;};
    while(!WindowShouldClose()){
        float dt=std::min(GetFrameTime(),.05f);
        if(dead||won){BeginDrawing();ClearBackground(dead?Color{17,3,5,255}:Color{5,22,10,255});DrawText(dead?"SHE FOUND YOU":"YOU ESCAPED",dead?400:430,255,58,RAYWHITE);DrawText(dead?"You should have heard her coming.":"The front door opens into the night.",dead?420:410,330,20,{185,185,185,255});DrawText("ENTER  -  restart",500,405,20,RAYWHITE);EndDrawing();if(IsKeyPressed(KEY_ENTER))reset();continue;}
        Vector2 md=GetMouseDelta(); yaw-=md.x*.0024f; pitch=Clamp(pitch-md.y*.0020f,-1.2f,1.2f);
        Vector3 forward={sin(yaw),0,cos(yaw)},right={-cos(yaw),0,sin(yaw)};
        cam.target=Vector3Add(cam.position,{sin(yaw)*cos(pitch),sin(pitch),cos(yaw)*cos(pitch)});
        bool sprint=IsKeyDown(KEY_LEFT_SHIFT)&&stamina>.04f&&!hiding; float speed=sprint?4.0f:2.35f; if(sprint)stamina=std::max(0.f,stamina-dt*.3f);else stamina=std::min(1.f,stamina+dt*.22f);
        Vector3 mv{};if(IsKeyDown(KEY_W))mv=Vector3Add(mv,forward);if(IsKeyDown(KEY_S))mv=Vector3Subtract(mv,forward);if(IsKeyDown(KEY_D))mv=Vector3Add(mv,right);if(IsKeyDown(KEY_A))mv=Vector3Subtract(mv,right);if(Vector3Length(mv)>.01f)Move(cam.position,Vector3Scale(Vector3Normalize(mv),speed*dt),.34f,walls);
        if(IsKeyPressed(KEY_E))hiding=!hiding;
        for(auto&f:fuses)if(!f.got&&Dist(cam.position,f.p)<1.15f){f.got=true;got++;msg=2.5f;PlaySound(pickup);}
        if(got==3&&cam.position.z<0&&IsKeyPressed(KEY_E))won=true;
        float ed=Dist(enemy,cam.position); bool visible=ed<7.0f; bool alert=visible|| (sprint&&ed<11.0f); if(hiding)alert=false;
        if(alert){target=cam.position;lastSeen=cam.position;enemyPause=1.5f;}else if(enemyPause>0)enemyPause-=dt;else if(Dist(enemy,target)<.8f||GetRandomValue(0,150)==0)target={(float)GetRandomValue(-7,7),1,(float)GetRandomValue(1,14)};
        Vector3 ev=Vector3Subtract(target,enemy);ev.y=0;if(Vector3Length(ev)>.1f){float es=alert?2.15f:0.72f;Move(enemy,Vector3Scale(Vector3Normalize(ev),es*dt),.43f,walls);}
        if(ed<1.05f&&!hiding){dead=true;PlaySound(attack);}
        if(ed<5.5f&&!hiding&&GetRandomValue(0,55)==0)PlaySound(heartbeat);
        if(GetRandomValue(0,300)==0)PlaySound(creak);
        BeginDrawing(); ClearBackground({5,6,8,255}); BeginMode3D(cam);
        DrawPlane({0,0,7},{20,22},{43,40,35,255});
        for(const auto&w:walls){DrawCube(w.p,w.s.x,w.s.y,w.s.z,{55,51,45,255});DrawCubeWires(w.p,w.s.x,w.s.y,w.s.z,{24,22,20,255});}
        for(int z=0;z<18;z+=2)DrawCube({0,.025f,(float)z},{18,0.04f,.025f}.x, .04f,.025f,{28,24,20,255});
        Furniture(wood,cloth,metal); Lamp({-6,2.8f,1.5f}); Lamp({6,2.8f,7}); Lamp({-2,2.8f,14});
        if(got<3){DrawCube({0,1.5f,-1.25f},2.2f,3,.25f,{65,39,26,255});DrawSphere({.55f,1.5f,-1.05f},.09f,{190,155,65,255});}
        for(const auto&f:fuses)if(!f.got){DrawCylinder(f.p,.16f,.16f,.42f,12,{215,208,175,255});DrawSphere({f.p.x,f.p.y+.23f,f.p.z},.13f,{235,220,120,255});}
        DrawCylinder({enemy.x,1.7f,enemy.z},.48f,.58f,1.05f,16,{67,57,55,255});DrawSphere({enemy.x,2.45f,enemy.z},.43f,{145,120,103,255});DrawSphere({enemy.x-.15f,2.51f,enemy.z-.37f},.065f,{15,5,5,255});DrawSphere({enemy.x+.15f,2.51f,enemy.z-.37f},.065f,{15,5,5,255});
        EndMode3D();
        DrawRectangle(0,0,1280,82,{0,0,0,165}); DrawText(TextFormat("FUSES  %d / 3",got),35,25,25,RAYWHITE);DrawText("WASD  move   SHIFT  run   E  hide / escape",35,53,17,{170,170,170,255});
        DrawRectangle(1030,28,190,13,{30,30,30,255});DrawRectangle(1030,28,(int)(190*stamina),13,{185,185,185,255});DrawText("STAMINA",1030,48,13,{155,155,155,255});
        if(msg>0){msg-=dt;DrawText(got<3?"Find the three fuses.":"All fuses found. Reach the front door and press E.",360,650,20,RAYWHITE);} if(hiding)DrawText("HIDING",570,115,20,{190,190,190,255});
        EndDrawing();
    }
    UnloadTexture(wall);UnloadTexture(wood);UnloadTexture(metal);UnloadTexture(cloth);CloseAudioDevice();CloseWindow();return 0;
}