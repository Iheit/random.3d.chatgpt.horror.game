#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <vector>

enum class EnemyState { Patrol, Investigate, Chase, Search, Windup };
struct Wall { Vector3 p, s; };
struct Fuse { Vector3 p; bool taken = false; };
struct HideSpot { Vector3 p; float radius; };

static float Dist(Vector3 a, Vector3 b) { a.y = b.y = 0; return Vector3Distance(a,b); }
static bool Blocked(Vector3 p, const Wall& w, float r) {
    return p.x > w.p.x-w.s.x*.5f-r && p.x < w.p.x+w.s.x*.5f+r &&
           p.z > w.p.z-w.s.z*.5f-r && p.z < w.p.z+w.s.z*.5f+r;
}
static void MoveBody(Vector3& p, Vector3 d, float r, const std::vector<Wall>& walls) {
    Vector3 q=p; q.x+=d.x;
    for(const auto&w:walls) if(Blocked(q,w,r)){q.x=p.x;break;} p=q;
    q=p; q.z+=d.z;
    for(const auto&w:walls) if(Blocked(q,w,r)){q.z=p.z;break;} p=q;
}
static bool LOS(Vector3 a, Vector3 b, const std::vector<Wall>& walls) {
    Vector3 d=Vector3Subtract(b,a); float len=Vector3Length(d); if(len<.1f)return true;
    d=Vector3Scale(d,1.f/len);
    for(int i=1;i<(int)(len/.16f);++i){Vector3 p=Vector3Add(a,Vector3Scale(d,i*.16f));for(const auto&w:walls)if(Blocked(p,w,.03f))return false;}
    return true;
}
static Texture2D Tex(Color base,Color accent,int seed){
    Image im=GenImageColor(96,96,base); Color*px=(Color*)im.data; unsigned s=(unsigned)seed*747796405u+2891336453u;
    for(int y=0;y<96;y++)for(int x=0;x<96;x++){s^=s<<13;s^=s>>17;s^=s<<5;int n=s&255;if(n>232||((x*9+y*5+seed*13)%71==0))px[y*96+x]=accent;else if(n<18)px[y*96+x]={(unsigned char)(base.r*.55f),(unsigned char)(base.g*.55f),(unsigned char)(base.b*.55f),255};}
    Texture2D t=LoadTextureFromImage(im);UnloadImage(im);return t;
}
static Model BoxModel(Texture2D t){Model m=LoadModelFromMesh(GenMeshCube(1,1,1));m.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture=t;return m;}
static void Box(Model&m,Vector3 p,Vector3 s,Color c=WHITE){DrawModelEx(m,p,{0,1,0},0,s,c);}
static Sound Tone(float sec,float hz,float vol){int n=std::max(1,(int)(sec*22050));short*d=(short*)MemAlloc((size_t)n*sizeof(short));for(int i=0;i<n;i++){float t=i/22050.f,e=1.f-i/(float)n;d[i]=(short)(std::sin(2*PI*hz*t)*e*vol*32767);}Wave w{};w.frameCount=n;w.sampleRate=22050;w.sampleSize=16;w.channels=1;w.data=d;Sound s=LoadSoundFromWave(w);UnloadWave(w);return s;}
static void Bed(Model&w,Model&c,Vector3 p){Box(w,{p.x,p.y+.24f,p.z},{2.7f,.48f,4.0f});Box(c,{p.x,p.y+.54f,p.z+.15f},{2.45f,.22f,3.05f});Box(c,{p.x,p.y+.76f,p.z-1.12f},{2.2f,.20f,.70f},{162,150,137,255});for(float x=-1;x<=1;x+=2)Box(w,{p.x+x,p.y+.10f,p.z-1.7f},{.15f,.45f,.15f});}
static void Desk(Model&w,Model&m,Vector3 p){Box(w,{p.x,p.y+.72f,p.z},{2.8f,.16f,1.0f});for(float x=-1.1f;x<=1.1f;x+=2.2f)Box(m,{p.x+x,p.y+.36f,p.z},{.13f,.72f,.13f});Box(w,{p.x,p.y+.34f,p.z+.18f},{1.45f,.10f,.74f});}
static void Cabinet(Model&w,Model&m,Vector3 p){Box(w,{p.x,p.y+1,p.z},{1.45f,2,.70f});for(float y=.48f;y<1.75f;y+=.42f)Box(m,{p.x,p.y+y,p.z-.36f},{.11f,.06f,.04f},{175,145,90,255});}
static void Shelf(Model&w,Vector3 p){for(float y=.45f;y<=1.9f;y+=.55f)Box(w,{p.x,p.y+y,p.z},{2.3f,.12f,.36f});for(float x=-.8f;x<=.8f;x+=.4f)Box(w,{p.x+x,p.y+.82f,p.z},{.22f,.34f,.28f},{112,61,39,255});}
static void Closet(Model&w,Vector3 p){Box(w,{p.x-.34f,p.y+1.18f,p.z},{.18f,2.36f,.72f});Box(w,{p.x+.34f,p.y+1.18f,p.z},{.18f,2.36f,.72f});Box(w,{p.x,p.y+2.38f,p.z},{.86f,.16f,.76f});Box(w,{p.x,p.y+.05f,p.z},{.86f,.10f,.76f});}
static void Lamp(Model&m,Vector3 p,bool flicker){float f=flicker?.80f+.20f*std::sin((float)GetTime()*9.f):1.f;Box(m,{p.x,p.y+.1f,p.z},{.16f,.18f,.16f},{62,58,51,255});DrawSphere({p.x,p.y+.32f,p.z},.15f,{(unsigned char)(205*f),(unsigned char)(179*f),(unsigned char)(122*f),255});}
static void EnemyDraw(Vector3 e,float t){float b=std::sin(t*4.2f)*.015f;DrawCylinder({e.x,.12f+b,e.z},.43f,.48f,.98f,16,{61,49,46,255});DrawCylinder({e.x-.21f,.05f+b,e.z},.105f,.12f,.58f,12,{37,31,30,255});DrawCylinder({e.x+.21f,.05f+b,e.z},.105f,.12f,.58f,12,{37,31,30,255});DrawSphere({e.x,1.31f+b,e.z},.33f,{149,119,101,255});DrawSphere({e.x-.115f,1.35f+b,e.z-.30f},.045f,{10,3,3,255});DrawSphere({e.x+.115f,1.35f+b,e.z-.30f},.045f,{10,3,3,255});DrawCylinder({e.x,1.56f+b,e.z},.37f,.40f,.15f,16,{33,25,24,255});}
static void Crosshair(){int x=GetScreenWidth()/2,y=GetScreenHeight()/2;DrawCircle(x,y,2.2f,{235,235,235,210});DrawLine(x-7,y,x-3,y,{185,185,185,130});DrawLine(x+3,y,x+7,y,{185,185,185,130});DrawLine(x,y-7,x,y-3,{185,185,185,130});DrawLine(x,y+3,x,y+7,{185,185,185,130});}
static void Spatial(Sound s,Vector3 src,Vector3 listener,Vector3 right){float d=Dist(src,listener),v=Clamp(1.f-d/16.f,0.f,1.f);Vector3 dir={src.x-listener.x,0,src.z-listener.z};float pan=Vector3Length(dir)>.01f?Vector3DotProduct(Vector3Normalize(dir),right):0;SetSoundVolume(s,v*.38f);SetSoundPan(s,Clamp(.5f+pan*.4f,0.f,1.f));PlaySound(s);}

int main(){
    SetConfigFlags(FLAG_MSAA_4X_HINT); InitWindow(1280,720,"Night House"); InitAudioDevice(); SetTargetFPS(60); DisableCursor();
    Texture2D wallTex=Tex({77,72,64,255},{118,102,86,255},3),woodTex=Tex({59,37,24,255},{104,66,39,255},7),metalTex=Tex({49,51,50,255},{112,108,94,255},11),clothTex=Tex({67,65,62,255},{110,102,89,255},19);
    Model wall=BoxModel(wallTex),wood=BoxModel(woodTex),metal=BoxModel(metalTex),cloth=BoxModel(clothTex);
    Sound pickup=Tone(.16f,720,.28f),doorSound=Tone(.70f,68,.46f),heartbeat=Tone(.12f,53,.50f),attack=Tone(.60f,34,.80f),creak=Tone(.85f,132,.18f),foot=Tone(.07f,92,.14f);

    // 42 x 34 house. Central hall is 8 m wide; six side rooms branch from it.
    std::vector<Wall>walls={
        {{-21,1.6f,0},{.7f,3.2f,34}},{{21,1.6f,0},{.7f,3.2f,34}},{{0,1.6f,17},{42,3.2f,.7f}},
        {{-12,1.6f,-17},{18,3.2f,.7f}},{{12,1.6f,-17},{18,3.2f,.7f}},
        // left wing room separators
        {{-12,1.6f,5},{16,3.2f,.55f}},{{-12,1.6f,-6},{16,3.2f,.55f}},
        // right wing room separators
        {{12,1.6f,5},{16,3.2f,.55f}},{{12,1.6f,-6},{16,3.2f,.55f}},
        // left central-hall door jambs, top/middle/bottom rooms
        {{-4,1.6f,7},{.55f,3.2f,4}},{{-4,1.6f,14},{.55f,3.2f,6}},
        {{-4,1.6f,-3.0f},{.55f,3.2f,6}},{{-4,1.6f,3.8f},{.55f,3.2f,2.4f}},
        {{-4,1.6f,-14.0f},{.55f,3.2f,6}},{{-4,1.6f,-7.8f},{.55f,3.2f,3.6f}},
        // right central-hall door jambs
        {{4,1.6f,7},{.55f,3.2f,4}},{{4,1.6f,14},{.55f,3.2f,6}},
        {{4,1.6f,-3.0f},{.55f,3.2f,6}},{{4,1.6f,3.8f},{.55f,3.2f,2.4f}},
        {{4,1.6f,-14.0f},{.55f,3.2f,6}},{{4,1.6f,-7.8f},{.55f,3.2f,3.6f}}
    };

    std::vector<Fuse>fuses={{{-14.8f,.95f,11.7f}},{{14.8f,.95f,11.5f}},{{-14.5f,.95f,-11.0f}}};
    std::vector<HideSpot>hides={{{-18.0f,1,12.9f},1.3f},{{18.0f,1,12.8f},1.3f},{{-17.8f,1,-11.0f},1.3f},{{17.8f,1,-11.0f},1.3f}};

    Camera3D cam{};cam.position={0,1.62f,14.4f};cam.up={0,1,0};cam.fovy=70;cam.projection=CAMERA_PERSPECTIVE;
    Vector3 enemy={0,1,3.5f},enemyFacing={0,0,1},target={0,1,10},lastSeen=enemy,noisePos=enemy;EnemyState state=EnemyState::Patrol;
    std::vector<Vector3>patrol={{0,1,11},{0,1,2},{0,1,-4},{0,1,-11},{0,1,-4},{0,1,6}};int patrolIndex=0;
    float yaw=0,pitch=0,stamina=100,battery=100,stateTimer=0,attackTimer=0,footTimer=0,noiseTimer=0,bob=0;int got=0;
    bool hiding=false,dead=false,won=false,menu=true,paused=false,doorOpen=false,flashlight=true;
    auto reset=[&](){cam.position={0,1.62f,14.4f};yaw=pitch=0;enemy={0,1,3.5f};enemyFacing={0,0,1};target={0,1,10};lastSeen=enemy;noisePos=enemy;state=EnemyState::Patrol;patrolIndex=0;stateTimer=attackTimer=footTimer=noiseTimer=bob=0;stamina=battery=100;got=0;hiding=dead=won=doorOpen=false;for(auto&f:fuses)f.taken=false;};

    while(!WindowShouldClose()){
        float dt=std::min(GetFrameTime(),.05f);if(IsKeyPressed(KEY_F11))ToggleFullscreen();
        if(menu){BeginDrawing();ClearBackground({7,8,10,255});DrawText("NIGHT HOUSE",412,132,64,RAYWHITE);DrawText("A larger six-room survival-horror house",430,220,20,{170,170,170,255});DrawText("ENTER  PLAY",532,320,27,RAYWHITE);DrawText("WASD move   SHIFT sprint   E interact / hide   F flashlight   ESC pause",266,395,17,{145,145,145,255});DrawText("F11 fullscreen",540,435,17,{120,120,120,255});EndDrawing();if(IsKeyPressed(KEY_ENTER)){menu=false;DisableCursor();}continue;}
        if(IsKeyPressed(KEY_ESCAPE)){paused=!paused;if(paused)EnableCursor();else DisableCursor();}
        if(paused){BeginDrawing();ClearBackground({8,9,11,235});DrawText("PAUSED",535,245,52,RAYWHITE);DrawText("ESC resume",548,330,20,{175,175,175,255});EndDrawing();continue;}
        if(dead||won){BeginDrawing();ClearBackground(dead?Color{23,3,6,255}:Color{5,25,10,255});DrawText(dead?"SHE CAUGHT YOU":"YOU ESCAPED",dead?390:430,230,60,RAYWHITE);DrawText(dead?"The house was bigger than expected.":"You found the front door.",dead?420:465,315,20,{190,190,190,255});DrawText("ENTER restart",525,400,21,RAYWHITE);EndDrawing();if(IsKeyPressed(KEY_ENTER))reset();continue;}

        Vector2 md=GetMouseDelta();yaw+=md.x*.00235f;pitch=Clamp(pitch-md.y*.0020f,-1.18f,1.18f);
        Vector3 forward={std::sin(yaw),0,std::cos(yaw)},right={std::cos(yaw),0,-std::sin(yaw)};
        cam.target=Vector3Add(cam.position,{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)});
        bool nearHide=false;for(const auto&h:hides)if(Dist(cam.position,h.p)<h.radius)nearHide=true;if(IsKeyPressed(KEY_E)&&nearHide)hiding=!hiding;
        bool moving=IsKeyDown(KEY_W)||IsKeyDown(KEY_A)||IsKeyDown(KEY_S)||IsKeyDown(KEY_D);bool sprint=IsKeyDown(KEY_LEFT_SHIFT)&&moving&&stamina>1&&!hiding;float speed=sprint?4.0f:2.35f;
        if(sprint)stamina=std::max(0.f,stamina-dt*30.f);else stamina=std::min(100.f,stamina+dt*23.f);
        Vector3 mv{};if(IsKeyDown(KEY_W))mv=Vector3Add(mv,forward);if(IsKeyDown(KEY_S))mv=Vector3Subtract(mv,forward);if(IsKeyDown(KEY_D))mv=Vector3Add(mv,right);if(IsKeyDown(KEY_A))mv=Vector3Subtract(mv,right);if(Vector3Length(mv)>.01f&&!hiding){MoveBody(cam.position,Vector3Scale(Vector3Normalize(mv),speed*dt),.34f,walls);bob+=dt*(sprint?13.f:8.f);}else bob=0;cam.position.y=1.62f+(moving&&!hiding?std::sin(bob)*.018f:0);
        if(IsKeyPressed(KEY_F)&&battery>1)flashlight=!flashlight;if(flashlight)battery=std::max(0.f,battery-dt*.42f);
        if(moving&&footTimer<=0&&!hiding){footTimer=sprint?.25f:.46f;SetSoundVolume(foot,sprint?.23f:.12f);PlaySound(foot);if(sprint){noisePos=cam.position;noiseTimer=1.05f;}}else footTimer-=dt;
        for(auto&f:fuses)if(!f.taken&&Dist(cam.position,f.p)<1.10f&&IsKeyPressed(KEY_E)){f.taken=true;++got;SetSoundVolume(pickup,.30f);PlaySound(pickup);noisePos=cam.position;noiseTimer=1.5f;}
        if(got==3&&!doorOpen&&cam.position.z<-15.2f&&std::abs(cam.position.x)<3.0f&&IsKeyPressed(KEY_E)){doorOpen=true;Spatial(doorSound,{0,1,-17},cam.position,right);won=true;}
        noiseTimer=std::max(0.f,noiseTimer-dt);

        float ed=Dist(enemy,cam.position);Vector3 to=Vector3Subtract({cam.position.x,1.62f,cam.position.z},{enemy.x,1.30f,enemy.z});if(Vector3Length(to)>.01f)to=Vector3Normalize(to);
        bool canSee=!hiding&&ed<13.5f&&Vector3DotProduct(to,enemyFacing)>.48f&&LOS({enemy.x,1.25f,enemy.z},{cam.position.x,1.62f,cam.position.z},walls);
        bool heard=noiseTimer>0&&Dist(enemy,noisePos)<(sprint?12.f:8.5f);
        if(canSee){lastSeen=cam.position;state=EnemyState::Chase;stateTimer=0;}else if(heard&&state!=EnemyState::Windup){target=noisePos;state=EnemyState::Investigate;stateTimer=0;}
        if(state==EnemyState::Chase){target=cam.position;if(ed>16.f||(!canSee&&stateTimer>2.5f)){target=lastSeen;state=EnemyState::Search;stateTimer=0;}else if(ed<1.85f&&attackTimer<=0&&!hiding){state=EnemyState::Windup;attackTimer=.80f;stateTimer=0;}}
        if(state==EnemyState::Windup){attackTimer-=dt;if(attackTimer<=0){if(Dist(enemy,cam.position)<1.35f&&!hiding){Spatial(attack,enemy,cam.position,right);dead=true;}else{state=EnemyState::Search;target=lastSeen;stateTimer=0;}}}
        else {stateTimer+=dt;if(state==EnemyState::Investigate&&Dist(enemy,target)<.7f){state=EnemyState::Search;stateTimer=0;}if(state==EnemyState::Search){if(stateTimer>4.2f){state=EnemyState::Patrol;stateTimer=0;}else if(Dist(enemy,target)<.75f)target=patrol[patrolIndex];}if(state==EnemyState::Patrol&&Dist(enemy,target)<.75f){patrolIndex=(patrolIndex+1)%patrol.size();target=patrol[patrolIndex];}Vector3 ev=Vector3Subtract(target,enemy);ev.y=0;if(Vector3Length(ev)>.08f){enemyFacing=Vector3Normalize(ev);float es=state==EnemyState::Investigate?1.18f:(state==EnemyState::Search?.90f:.62f);MoveBody(enemy,Vector3Scale(enemyFacing,es*dt),.42f,walls);}}
        if(ed<5.5f&&!hiding&&GetRandomValue(0,58)==0){SetSoundVolume(heartbeat,Clamp(1-ed/6.5f,0.f,1.f)*.42f);PlaySound(heartbeat);}if(GetRandomValue(0,330)==0)PlaySound(creak);

        BeginDrawing();ClearBackground({4,5,7,255});BeginMode3D(cam);
        DrawPlane({0,0,0},{42,34},{38,35,31,255});DrawCube({0,3.08f,0},42,.14f,34,{24,24,23,255});
        for(const auto&w:walls){Box(wall,w.p,w.s,std::abs(w.p.x)>20.5f||std::abs(w.p.z)>16.5f?Color{176,168,154,255}:Color{204,194,178,255});DrawCubeWires(w.p,w.s.x,w.s.y,w.s.z,{25,22,20,255});}
        for(int z=-16;z<=16;z++)Box(wood,{0,.035f,(float)z},{41.5f,.045f,.028f},{72,48,34,255});
        // Top-left bedroom, top-right bedroom
        Bed(wood,cloth,{-15.8f,0,12.8f});Bed(wood,cloth,{15.8f,0,12.8f});DrawCabinet(wood,metal,{-19,0,7.6f});DrawCabinet(wood,metal,{19,0,7.5f});
        Closet(wood,{-18,0,13.7f});Closet(wood,{18,0,13.6f});
        // Middle rooms
        Shelf(wood,{-15.8f,0,-1.0f});Shelf(wood,{15.8f,0,-1.0f});DrawTable(wood,metal,{-12.8f,0,2.0f});DrawTable(wood,metal,{12.8f,0,2.0f});
        // Bottom rooms
        Desk(wood,metal,{-15.8f,0,-12.2f});Desk(wood,metal,{15.8f,0,-12.2f});DrawCabinet(wood,metal,{-19,0,-8.7f});DrawCabinet(wood,metal,{19,0,-8.7f});
        // Hall dressing and lighting
        DrawShelf(wood,{-5.25f,0,4.0f});DrawShelf(wood,{5.25f,0,4.0f});Lamp(metal,{-6.2f,2.70f,10.2f},true);Lamp(metal,{6.2f,2.70f,10.2f},false);Lamp(metal,{0,2.72f,1.0f},true);Lamp(metal,{0,2.72f,-9.0f},true);
        if(!doorOpen){Box(wood,{-12,1.6f,-17},{18,3.2f,.7f});Box(wood,{12,1.6f,-17},{18,3.2f,.7f});Box(metal,{.65f,1.48f,-16.6f},{.09f,.09f,.09f},{190,160,85,255});}
        for(const auto&f:fuses)if(!f.taken){DrawCylinder(f.p,.16f,.16f,.42f,12,{214,206,174,255});DrawSphere({f.p.x,f.p.y+.22f,f.p.z},.13f,{235,220,110,255});}
        for(const auto&h:hides)Closet(wood,h.p);
        EnemyDraw(enemy,(float)GetTime());EndMode3D();
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),flashlight?Color{0,0,0,22}:Color{0,0,0,108});DrawRectangle(0,0,1280,86,{0,0,0,185});
        DrawText(TextFormat("FUSES   %d / 3",got),30,20,25,RAYWHITE);DrawText(hiding?"HIDDEN":"",585,22,21,{185,185,185,255});DrawText("WASD move   SHIFT run   E interact / hide   F flashlight   ESC pause",30,55,16,{155,155,155,255});
        DrawRectangle(1035,22,190,11,{28,28,28,255});DrawRectangle(1035,22,(int)(190*stamina/100.f),11,{185,185,185,255});DrawText("STAMINA",1035,38,11,{150,150,150,255});DrawRectangle(1035,59,190,8,{28,28,28,255});DrawRectangle(1035,59,(int)(190*battery/100.f),8,{155,155,145,255});DrawText("BATTERY",1035,71,10,{140,140,140,255});
        if(nearHide)DrawText(hiding?"E leave hiding":"E hide",535,620,19,RAYWHITE);DrawText(got<3?"Search the six rooms for the three fuses.":"All fuses found. Reach the front door.",390,655,19,{190,190,190,255});if(state==EnemyState::Windup)DrawText("TOO CLOSE",560,104,17,{205,160,160,255});if(state==EnemyState::Chase)DrawText("SHE SEES YOU",515,104,17,{205,160,160,255});Crosshair();EndDrawing();
    }
    UnloadSound(pickup);UnloadSound(doorSound);UnloadSound(heartbeat);UnloadSound(attack);UnloadSound(creak);UnloadSound(foot);UnloadModel(wall);UnloadModel(wood);UnloadModel(metal);UnloadModel(cloth);UnloadTexture(wallTex);UnloadTexture(woodTex);UnloadTexture(metalTex);UnloadTexture(clothTex);CloseAudioDevice();CloseWindow();return 0;
}
