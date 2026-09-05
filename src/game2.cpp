#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>

enum class EnemyState { Patrol, Investigate, Chase, Search, Windup };
struct Wall { Vector3 p, s; };
struct Fuse { Vector3 p; bool taken = false; };
struct HideSpot { Vector3 p; float radius; };

static float FlatDist(Vector3 a, Vector3 b) { a.y = b.y = 0; return Vector3Distance(a, b); }
static bool Inside(Vector3 p, const Wall& w, float r) {
    return p.x > w.p.x - w.s.x*0.5f - r && p.x < w.p.x + w.s.x*0.5f + r &&
           p.z > w.p.z - w.s.z*0.5f - r && p.z < w.p.z + w.s.z*0.5f + r;
}
static void MoveBody(Vector3& p, Vector3 delta, float radius, const std::vector<Wall>& walls) {
    Vector3 q = p; q.x += delta.x;
    for (const auto& w : walls) if (Inside(q, w, radius)) { q.x = p.x; break; }
    p = q; q = p; q.z += delta.z;
    for (const auto& w : walls) if (Inside(q, w, radius)) { q.z = p.z; break; }
    p = q;
}
static bool HasLOS(Vector3 from, Vector3 to, const std::vector<Wall>& walls) {
    Vector3 d = Vector3Subtract(to, from); float len = Vector3Length(d);
    if (len < 0.01f) return true; d = Vector3Scale(d, 1.0f/len);
    int steps = (int)(len/0.22f);
    for (int i=1;i<steps;i++) { Vector3 p=Vector3Add(from,Vector3Scale(d,i*0.22f)); for (const auto&w:walls) if (Inside(p,w,0.05f)) return false; }
    return true;
}
static Texture2D MakeMat(Color base, Color accent, int seed) {
    Image im = GenImageColor(96,96,base); Color* px = LoadImageColors(im); unsigned s = (unsigned)seed*747796405u+2891336453u;
    for (int y=0;y<96;y++) for(int x=0;x<96;x++) { s^=s<<13; s^=s>>17; s^=s<<5; int n=s&255; if(n>228 || (x*11+y*7+seed)%53==0) px[y*96+x]=accent; else if(n<18) px[y*96+x]={(unsigned char)(base.r*.55f),(unsigned char)(base.g*.55f),(unsigned char)(base.b*.55f),255}; }
    Image out = GenImageColor(96,96,base); Color* op=LoadImageColors(out); for(int i=0;i<9216;i++) op[i]=px[i]; Texture2D t=LoadTextureFromImage(out); UnloadImageColors(px); UnloadImageColors(op); UnloadImage(im); UnloadImage(out); return t;
}
static Sound MakeTone(float seconds, float hz, float vol) {
    int n=std::max(1,(int)(seconds*22050)); short* data=(short*)MemAlloc(n*sizeof(short));
    for(int i=0;i<n;i++){float t=i/22050.f,e=1.f-i/(float)n; data[i]=(short)(std::sin(2*PI*hz*t)*e*vol*32767);}
    Wave w{}; w.frameCount=n; w.sampleRate=22050; w.sampleSize=16; w.channels=1; w.data=data; Sound snd=LoadSoundFromWave(w); UnloadWave(w); return snd;
}
static Model MakeBox(Texture2D tex) { Model m=LoadModelFromMesh(GenMeshCube(1,1,1)); m.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture=tex; return m; }
static void DrawBox(Model& m, Vector3 p, Vector3 s, Color tint=WHITE) { DrawModelEx(m,p,{0,1,0},0,s,tint); }
static void WallDetail(Model& wood, Vector3 p, float width) {
    DrawBox(wood,{p.x,p.y,p.z},{width,.16f,.12f},{105,70,43,255});
}
static void Bed(Model& wood, Model& cloth, Vector3 p) {
    DrawBox(wood,{p.x,p.y+.32f,p.z},{2.5f,.55f,4.1f}); DrawBox(cloth,{p.x,p.y+.68f,p.z+.35f},{2.35f,.38f,3.2f});
    DrawBox(cloth,{p.x,p.y+.9f,p.z-1.1f},{2.25f,.22f,.75f},{155,145,132,255});
    for(float x=-1.f;x<=1.f;x+=2.f) DrawBox(wood,{p.x+x,p.y+.12f,p.z-1.65f},{.16f,.55f,.16f});
}
static void Desk(Model& wood, Model& metal, Vector3 p) {
    DrawBox(wood,{p.x,p.y+.72f,p.z},{2.8f,.16f,1.1f});
    for(float x=-1.15f;x<=1.15f;x+=2.3f) DrawBox(metal,{p.x+x,p.y+.36f,p.z},{.14f,.7f,.14f});
    DrawBox(wood,{p.x,p.y+.35f,p.z+.12f},{1.5f,.1f,.8f}); DrawBox(metal,{p.x,p.y+1.05f,p.z-.3f},{.9f,.08f,.08f});
}
static void Cabinet(Model& wood, Model& metal, Vector3 p) {
    DrawBox(wood,{p.x,p.y+1.05f,p.z},{1.5f,2.1f,.65f});
    for(float y=.5f;y<=1.6f;y+=.55f){DrawBox(metal,{p.x,p.y+y,p.z-.34f},{.12f,.07f,.04f},{145,126,84,255});}
}
static void Shelf(Model& wood, Vector3 p) {
    DrawBox(wood,{p.x,p.y+1.5f,p.z},{2.5f,.12f,.42f}); DrawBox(wood,{p.x,p.y+.7f,p.z},{2.5f,.12f,.42f});
    for(float y=.65f;y<2.2f;y+=.45f) DrawBox(wood,{p.x,p.y+y,p.z},{2.35f,.28f,.3f},{120,68,42,255});
}
static void Door(Model& wood, Model& metal, Vector3 p, bool open) {
    if(!open){DrawBox(wood,p,{2.1f,3.f,.18f});DrawBox(metal,{p.x+.55f,p.y,p.z-.12f},{.08f,.08f,.08f},{190,160,90,255});}
    else { DrawBox(wood,{p.x-1.0f,p.y,p.z},{.18f,3.f,2.1f}); }
}
static void EnemyDraw(Vector3 e, float phase) {
    float bob=std::sin(phase*5)*.025f;
    DrawCylinder({e.x,1.35f+ bob,e.z},.5f,.62f,1.45f,16,{58,48,47,255});
    DrawCylinder({e.x-.31f,1.0f+ bob,e.z},.11f,.13f,.75f,12,{42,37,36,255});
    DrawCylinder({e.x+.31f,1.0f+ bob,e.z},.11f,.13f,.75f,12,{42,37,36,255});
    DrawSphere({e.x,2.45f+bob,e.z},.45f,{150,122,104,255});
    DrawSphere({e.x-.16f,2.52f+bob,e.z-.40f},.06f,{15,5,5,255}); DrawSphere({e.x+.16f,2.52f+bob,e.z-.40f},.06f,{15,5,5,255});
    DrawCylinder({e.x,2.82f+bob,e.z},.48f,.34f,.20f,16,{38,29,27,255});
}
static void Crosshair() { DrawCircle(GetScreenWidth()/2,GetScreenHeight()/2,2.5f,{220,220,220,190}); }

int main(){
    SetConfigFlags(FLAG_MSAA_4X_HINT); InitWindow(1280,720,"Night House"); InitAudioDevice(); SetTargetFPS(60); DisableCursor();
    Texture2D woodTex=MakeMat({58,37,25,255},{99,63,37,255},7), wallTex=MakeMat({72,68,61,255},{112,96,78,255},3), metalTex=MakeMat({49,51,50,255},{112,106,92,255},11), clothTex=MakeMat({66,65,64,255},{112,104,91,255},19);
    Model wood=MakeBox(woodTex), metal=MakeBox(metalTex), cloth=MakeBox(clothTex);
    Sound pickup=MakeTone(.18f,720,.30f), doorSound=MakeTone(.55f,72,.45f), heartbeat=MakeTone(.12f,52,.48f), hit=MakeTone(.55f,38,.8f), creak=MakeTone(.9f,135,.20f), foot=MakeTone(.07f,92,.15f);
    std::vector<Wall> walls={{{-9.6f,1.5f,7},{.7f,3,18}},{{9.6f,1.5f,7},{.7f,3,18}},{{0,1.5f,-2},{20,3,.7f}},{{0,1.5f,16},{20,3,.7f}},{{-5.8f,1.5f,5},{.7f,3,6}},{{5.7f,1.5f,5},{.7f,3,6}},{{-5.8f,1.5f,11.2f},{.7f,3,5}},{{5.7f,1.5f,11.2f},{.7f,3,5}},{{0,1.5f,7.7f},{.55f,3,5.6f}},{{-3.0f,1.5f,4.2f},{5,3,.55f}},{{3.0f,1.5f,11.2f},{5,3,.55f}}};
    std::vector<Fuse> fuses={{{-7.5f,.95f,2.8f}},{{7.1f,.95f,8.1f}},{{2.8f,.95f,14.05f}}};
    std::vector<HideSpot> hides={{{-8.0f,1,5.8f},1.3f},{{7.7f,1,12.9f},1.3f},{{-7.7f,1,14.0f},1.3f}};
    Camera3D cam{}; cam.position={0,1.62f,13.4f}; cam.up={0,1,0}; cam.fovy=70; cam.projection=CAMERA_PERSPECTIVE;
    Vector3 enemy={0,1,3.0f}, target=enemy, lastSeen={0,1,3}; float yaw=0,pitch=0,stamina=1,battery=100,attackTimer=0,stateTimer=0,footTimer=0,msgTimer=0; EnemyState state=EnemyState::Patrol; int got=0; bool hiding=false,dead=false,won=false,paused=false,menu=true,doorOpen=false,flashlight=true;
    auto reset=[&](){cam.position={0,1.62f,13.4f};yaw=0;pitch=0;enemy={0,1,3};target=enemy;lastSeen=enemy;stamina=1;battery=100;got=0;hiding=false;dead=false;won=false;paused=false;doorOpen=false;state=EnemyState::Patrol;stateTimer=0;attackTimer=0;for(auto&f:fuses)f.taken=false;};
    while(!WindowShouldClose()){
        float dt=std::min(GetFrameTime(),.05f);
        if(IsKeyPressed(KEY_F11)) ToggleFullscreen();
        if(menu){ BeginDrawing(); ClearBackground({8,8,10,255}); DrawText("NIGHT HOUSE",410,170,64,RAYWHITE); DrawText("A small horror game about making too much noise in an empty house.",270,255,19,{170,170,170,255}); DrawText("ENTER  play",510,370,24,RAYWHITE); DrawText("WASD move   SHIFT sprint   E hide/interact   F flashlight   ESC pause",275,430,18,{145,145,145,255}); DrawText("F11 fullscreen",540,475,17,{120,120,120,255}); EndDrawing(); if(IsKeyPressed(KEY_ENTER)){menu=false;DisableCursor();} continue; }
        if(IsKeyPressed(KEY_ESCAPE)){paused=!paused;if(paused)EnableCursor();else DisableCursor();}
        if(paused){BeginDrawing();ClearBackground({10,10,12,220});DrawText("PAUSED",540,250,52,RAYWHITE);DrawText("ESC resume",548,330,20,{175,175,175,255});DrawText("F11 fullscreen",510,370,18,{145,145,145,255});EndDrawing();continue;}
        if(dead||won){BeginDrawing();ClearBackground(dead?Color{22,3,6,255}:Color{5,26,11,255});DrawText(dead?"SHE CAUGHT YOU":"YOU ESCAPED",dead?400:430,235,60,RAYWHITE);DrawText(dead?"You were given enough warning.":"The front door finally opens.",dead?425:430,315,20,{190,190,190,255});DrawText("ENTER restart",525,400,21,RAYWHITE);EndDrawing();if(IsKeyPressed(KEY_ENTER))reset();continue;}
        Vector2 md=GetMouseDelta(); yaw+=md.x*.00235f; pitch=Clamp(pitch-md.y*.0020f,-1.18f,1.18f);
        Vector3 forward={std::sin(yaw),0,std::cos(yaw)}, right={std::cos(yaw),0,-std::sin(yaw)};
        cam.target=Vector3Add(cam.position,{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)});
        bool nearHide=false; for(const auto&h:hides)if(FlatDist(cam.position,h.p)<h.radius)nearHide=true; if(IsKeyPressed(KEY_E)&&nearHide)hiding=!hiding;
        bool moving=IsKeyDown(KEY_W)||IsKeyDown(KEY_A)||IsKeyDown(KEY_S)||IsKeyDown(KEY_D); bool sprint=IsKeyDown(KEY_LEFT_SHIFT)&&moving&&stamina>.05f&&!hiding; float speed=sprint?4.1f:2.35f; if(sprint)stamina=std::max(0.f,stamina-dt*.34f);else stamina=std::min(1.f,stamina+dt*.25f);
        Vector3 mv{}; if(IsKeyDown(KEY_W))mv=Vector3Add(mv,forward); if(IsKeyDown(KEY_S))mv=Vector3Subtract(mv,forward); if(IsKeyDown(KEY_D))mv=Vector3Add(mv,right); if(IsKeyDown(KEY_A))mv=Vector3Subtract(mv,right); if(Vector3Length(mv)>.01f&&!hiding)MoveBody(cam.position,Vector3Scale(Vector3Normalize(mv),speed*dt),.34f,walls);
        if(IsKeyPressed(KEY_F)&&battery>1)flashlight=!flashlight; if(flashlight)battery=std::max(0.f,battery-dt*.9f);
        for(auto&f:fuses) if(!f.taken&&FlatDist(cam.position,f.p)<1.1f&&IsKeyPressed(KEY_E)){f.taken=true;got++;msgTimer=2.5f;PlaySound(pickup);}
        if(got==3&&!doorOpen&&cam.position.z<.1f&&IsKeyPressed(KEY_E)){doorOpen=true;PlaySound(doorSound);won=true;}
        float ed=FlatDist(enemy,cam.position); Vector3 toPlayer=Vector3Subtract({cam.position.x,1.65f,cam.position.z},{enemy.x,1.7f,enemy.z}); float len=std::max(.001f,Vector3Length(toPlayer)); float facing=Vector3DotProduct(Vector3Normalize(toPlayer),{std::sin(yaw),0,std::cos(yaw)});
        Vector3 enemyForward={std::sin(stateTimer),0,std::cos(stateTimer)}; bool canSee=!hiding&&ed<11.0f&&HasLOS({enemy.x,1.7f,enemy.z},{cam.position.x,1.65f,cam.position.z},walls); bool inFront=Vector3DotProduct(Vector3Normalize(toPlayer),enemyForward)>.15f;
        if(canSee&&inFront){ lastSeen=cam.position; if(ed<8.0f || sprint){state=EnemyState::Chase;target=cam.position;stateTimer=0;} else if(state!=EnemyState::Windup) {state=EnemyState::Investigate;target=cam.position;stateTimer=0;} }
        else if(sprint&&!hiding&&ed<13.0f){state=EnemyState::Investigate;target=cam.position;stateTimer=0;}
        if(state==EnemyState::Chase){target=cam.position;if(ed<1.8f&&!hiding&&attackTimer<=0){state=EnemyState::Windup;attackTimer=.72f;stateTimer=0;}}
        if(state==EnemyState::Windup){attackTimer-=dt;stateTimer+=dt;if(attackTimer<=0){if(FlatDist(enemy,cam.position)<1.55f&&!hiding)dead=true;else{state=EnemyState::Search;target=lastSeen;stateTimer=0;}}}
        else {if(state==EnemyState::Investigate&&FlatDist(enemy,target)<.65f){state=EnemyState::Search;stateTimer=0;} if(state==EnemyState::Search){stateTimer+=dt;if(stateTimer>4.0f){state=EnemyState::Patrol;stateTimer=0;}} if(state==EnemyState::Patrol&&(FlatDist(enemy,target)<.6f||GetRandomValue(0,120)==0))target={(float)GetRandomValue(-7,7),1,(float)GetRandomValue(2,14)}; Vector3 ev=Vector3Subtract(target,enemy);ev.y=0; if(Vector3Length(ev)>.08f){float es=state==EnemyState::Chase?1.85f:(state==EnemyState::Investigate?1.1f:(state==EnemyState::Search?.82f:.62f));MoveBody(enemy,Vector3Scale(Vector3Normalize(ev),es*dt),.43f,walls);}}
        if(ed<5.0f&&!hiding&&GetRandomValue(0,65)==0)PlaySound(heartbeat); if(moving&&footTimer<=0){if(sprint)footTimer=.28f;else footTimer=.48f;SetSoundVolume(foot,sprint?.22f:.12f);PlaySound(foot);} if(moving)footTimer-=dt; else footTimer=0; if(GetRandomValue(0,330)==0)PlaySound(creak);
        BeginDrawing(); ClearBackground({4,5,7,255}); BeginMode3D(cam);
        DrawPlane({0,0,7},{20,22},{42,39,35,255}); DrawCube({0,3.05f,7},20,.15f,22,{28,27,25,255});
        for(const auto&w:walls){DrawBox(wood,w.p,w.s,{.72f,.68f,.62f,1});DrawCubeWires(w.p,w.s.x,w.s.y,w.s.z,{27,24,22,255});}
        for(int z=-1;z<=17;z+=1)DrawBox(wood,{0,.05f,(float)z},{18,.05f,.025f},{.46f,.34f,.23f,1});
        for(float x=-9;x<=9;x+=3){WallDetail(wood,{-9.2f, .95f, x},.8f);WallDetail(wood,{9.2f,.95f,x},.8f);}
        Bed(wood,cloth,{-7.0f,.0f,12.9f}); Desk(wood,metal,{6.8f,.0f,3.0f}); Cabinet(wood,metal,{-7.1f,.0f,2.7f}); Cabinet(wood,metal,{7.0f,.0f,13.0f}); Shelf(wood,{6.7f,.0f,8.0f});
        DrawBox(wood,{-1.8f,.55f,4.0f},{2.2f,1.1f,1.0f}); DrawBox(wood,{-1.8f,1.25f,4.0f},{1.8f,.12f,.8f});
        Door(wood,metal,{0,1.5f,-1.25f},doorOpen); for(const auto&h:hides)DrawBox(wood,h.p,{.5f,2.3f,.5f},{.28f,.22f,.18f,1});
        for(const auto&f:fuses)if(!f.taken){DrawCylinder(f.p,.16f,.16f,.42f,12,{220,212,180,255});DrawSphere({f.p.x,f.p.y+.23f,f.p.z},.14f,{235,218,105,255});}
        for(Vector3 lp: {Vector3{-6.7f,2.85f,2.0f},Vector3{6.7f,2.85f,7.5f},Vector3{-3.0f,2.85f,13.8f}}){DrawCylinder(lp,.30f,.42f,.12f,16,{45,41,36,255});DrawSphere({lp.x,lp.y+.28f,lp.z},.25f,{178,160,122,255});}
        EnemyDraw(enemy,GetTime()); EndMode3D();
        DrawRectangle(0,0,1280,86,{0,0,0,180}); DrawText(TextFormat("FUSES  %d / 3",got),32,22,25,RAYWHITE); DrawText(hiding?"HIDDEN":"",570,25,22,{175,175,175,255});
        DrawText("WASD move   SHIFT run   E interact/hide   F flashlight   ESC pause",32,57,16,{155,155,155,255});
        DrawRectangle(1040,25,185,12,{28,28,28,255}); DrawRectangle(1040,25,(int)(185*stamina),12,{190,190,190,255}); DrawText("STAMINA",1040,42,12,{150,150,150,255});
        DrawRectangle(1040,64,185,9,{28,28,28,255}); DrawRectangle(1040,64,(int)(185*(battery/100.f)),9,{160,160,145,255}); DrawText("BATTERY",1040,76,11,{140,140,140,255});
        if(nearHide)DrawText(hiding?"E  leave hiding":"E  hide",540,625,19,RAYWHITE); if(got<3)DrawText("Find the three fuses and get out.",430,655,19,RAYWHITE); else DrawText("The door is unlocked. Get outside.",445,655,19,RAYWHITE); if(state==EnemyState::Chase)DrawText("SHE HEARD YOU",505,105,17,{190,150,150,255}); Crosshair(); EndDrawing();
    }
    UnloadSound(pickup);UnloadSound(doorSound);UnloadSound(heartbeat);UnloadSound(hit);UnloadSound(creak);UnloadSound(foot);UnloadModel(wood);UnloadModel(metal);UnloadModel(cloth);UnloadTexture(woodTex);UnloadTexture(wallTex);UnloadTexture(metalTex);UnloadTexture(clothTex);CloseAudioDevice();CloseWindow(); return 0;
}
