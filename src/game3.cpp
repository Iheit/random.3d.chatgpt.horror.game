#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <vector>

enum class EnemyState { Patrol, Investigate, Chase, Search, Windup };
struct Wall { Vector3 p, s; };
struct Fuse { Vector3 p; bool taken=false; };
struct HideSpot { Vector3 p; float radius; };

static float FlatDist(Vector3 a, Vector3 b){ a.y=b.y=0; return Vector3Distance(a,b); }
static bool InWall(Vector3 p,const Wall&w,float r){return p.x>w.p.x-w.s.x*.5f-r&&p.x<w.p.x+w.s.x*.5f+r&&p.z>w.p.z-w.s.z*.5f-r&&p.z<w.p.z+w.s.z*.5f+r;}
static void MoveBody(Vector3& p,Vector3 delta,float radius,const std::vector<Wall>& walls){Vector3 q=p;q.x+=delta.x;for(const auto&w:walls)if(InWall(q,w,radius)){q.x=p.x;break;}p=q;q=p;q.z+=delta.z;for(const auto&w:walls)if(InWall(q,w,radius)){q.z=p.z;break;}p=q;}
static bool ClearSight(Vector3 a,Vector3 b,const std::vector<Wall>& walls){Vector3 d=Vector3Subtract(b,a);float len=Vector3Length(d);if(len<.1f)return true;d=Vector3Scale(d,1.f/len);int steps=(int)(len/.18f);for(int i=1;i<steps;i++){Vector3 p=Vector3Add(a,Vector3Scale(d,i*.18f));for(const auto&w:walls)if(InWall(p,w,.04f))return false;}return true;}
static Texture2D MakeTexture(Color base,Color accent,int seed){Image im=GenImageColor(96,96,base);Color*px=LoadImageColors(im);unsigned s=(unsigned)seed*747796405u+2891336453u;for(int y=0;y<96;y++)for(int x=0;x<96;x++){s^=s<<13;s^=s>>17;s^=s<<5;int n=s&255;int scratch=((x*9+y*5+seed*13)%71==0);if(n>230||scratch)px[y*96+x]=accent;else if(n<16)px[y*96+x]={(unsigned char)(base.r*.55f),(unsigned char)(base.g*.55f),(unsigned char)(base.b*.55f),255};}Texture2D t=LoadTextureFromImage(im);UnloadImageColors(px);UnloadImage(im);return t;}
static Model MakeBox(Texture2D tex){Model m=LoadModelFromMesh(GenMeshCube(1,1,1));m.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture=tex;return m;}
static void Box(Model&m,Vector3 p,Vector3 s,Color tint=WHITE){DrawModelEx(m,p,{0,1,0},0,s,tint);}
static Sound Tone(float sec,float hz,float vol){int n=std::max(1,(int)(sec*22050));short*d=(short*)MemAlloc((size_t)n*sizeof(short));for(int i=0;i<n;i++){float t=i/22050.f,e=1.f-i/(float)n;d[i]=(short)(std::sin(2*PI*hz*t)*e*vol*32767);}Wave w{};w.frameCount=n;w.sampleRate=22050;w.sampleSize=16;w.channels=1;w.data=d;Sound s=LoadSoundFromWave(w);UnloadWave(w);return s;}
static void Bed(Model&wood,Model&cloth,Vector3 p){Box(wood,{p.x,p.y+.32f,p.z},{2.6f,.6f,4.1f});Box(cloth,{p.x,p.y+.68f,p.z+.25f},{2.4f,.34f,3.1f});Box(cloth,{p.x,p.y+.90f,p.z-1.15f},{2.2f,.22f,.72f},{165,154,140,255});for(float x=-1.f;x<=1.f;x+=2.f)Box(wood,{p.x+x,p.y+.12f,p.z-1.7f},{.16f,.58f,.16f});}
static void Desk(Model&wood,Model&metal,Vector3 p){Box(wood,{p.x,p.y+.72f,p.z},{2.8f,.16f,1.05f});for(float x=-1.1f;x<=1.1f;x+=2.2f)Box(metal,{p.x+x,p.y+.36f,p.z},{.13f,.72f,.13f});Box(wood,{p.x,p.y+.34f,p.z+.18f},{1.5f,.10f,.78f});Box(metal,{p.x,p.y+1.05f,p.z-.28f},{.8f,.08f,.08f});}
static void Cabinet(Model&wood,Model&metal,Vector3 p){Box(wood,{p.x,p.y+1.0f,p.z},{1.45f,2.0f,.68f});for(float y=.48f;y<1.7f;y+=.42f)Box(metal,{p.x,p.y+y,p.z-.36f},{.12f,.06f,.04f},{175,145,90,255});}
static void Shelf(Model&wood,Vector3 p){Box(wood,{p.x,p.y+.45f,p.z},{2.4f,.12f,.38f});Box(wood,{p.x,p.y+1.1f,p.z},{2.4f,.12f,.38f});Box(wood,{p.x,p.y+1.75f,p.z},{2.4f,.12f,.38f});for(float x=-.85f;x<=.85f;x+=.42f){float h=.26f+((int)(x*10)&1)*.08f;Box(wood,{p.x+x,p.y+.75f,p.z},{.24f,h,.28f},{115,61,40,255});}}
static void Table(Model&wood,Model&metal,Vector3 p){Box(wood,{p.x,p.y+.82f,p.z},{2.5f,.16f,1.2f});for(float x=-.95f;x<=.95f;x+=1.9f)for(float z=-.35f;z<=.35f;z+=.7f)Box(metal,{p.x+x,p.y+.38f,p.z+z},{.12f,.75f,.12f});}
static void Lamp(Vector3 p,bool flicker){float f=flicker?(0.83f+0.17f*std::sin((float)GetTime()*9.f)):1.f;DrawCylinder(p,.28f,.38f,.12f,16,{42,39,35,255});DrawCylinder({p.x,p.y+.18f,p.z},.07f,.07f,.36f,12,{68,62,52,255});DrawSphere({p.x,p.y+.39f,p.z},.25f,{(unsigned char)(185*f),(unsigned char)(165*f),(unsigned char)(120*f),255});}
static void Enemy(Vector3 e,float phase){float bob=std::sin(phase*4.6f)*.025f;DrawCylinder({e.x,1.18f+bob,e.z},.53f,.63f,1.35f,16,{61,49,47,255});DrawCylinder({e.x-.29f,.70f+bob,e.z},.12f,.14f,.72f,12,{38,32,31,255});DrawCylinder({e.x+.29f,.70f+bob,e.z},.12f,.14f,.72f,12,{38,32,31,255});DrawSphere({e.x,2.18f+bob,e.z},.43f,{151,122,104,255});DrawSphere({e.x-.15f,2.25f+bob,e.z-.39f},.055f,{12,4,4,255});DrawSphere({e.x+.15f,2.25f+bob,e.z-.39f},.055f,{12,4,4,255});DrawCylinder({e.x,2.52f+bob,e.z},.47f,.32f,.18f,16,{35,27,26,255});}
static void Crosshair(){int x=GetScreenWidth()/2,y=GetScreenHeight()/2;DrawCircle(x,y,2.2f,{235,235,235,210});DrawLine(x-7,y,x-3,y,{180,180,180,130});DrawLine(x+3,y,x+7,y,{180,180,180,130});DrawLine(x,y-7,x,y-3,{180,180,180,130});DrawLine(x,y+3,x,y+7,{180,180,180,130});}

int main(){
    SetConfigFlags(FLAG_MSAA_4X_HINT);InitWindow(1280,720,"Night House");InitAudioDevice();SetTargetFPS(60);DisableCursor();
    Texture2D woodTex=MakeTexture({59,37,24,255},{104,66,39,255},7),metalTex=MakeTexture({50,52,50,255},{112,108,94,255},11),clothTex=MakeTexture({68,66,63,255},{112,104,91,255},19);
    Model wood=MakeBox(woodTex),metal=MakeBox(metalTex),cloth=MakeBox(clothTex);
    Sound pickup=Tone(.16f,720,.30f),doorS=Tone(.6f,70,.45f),heartbeat=Tone(.11f,53,.5f),attackS=Tone(.55f,38,.8f),creak=Tone(.8f,135,.18f),foot=Tone(.07f,92,.14f);
    std::vector<Wall>walls={{{-9.6f,1.5f,7},{.7f,3,18}},{{9.6f,1.5f,7},{.7f,3,18}},{{-5.4f,1.5f,-2},{8.2f,3,.7f}},{{5.4f,1.5f,-2},{8.2f,3,.7f}},{{0,1.5f,16},{20,3,.7f}},{{-5.8f,1.5f,6.2f},{.6f,3,4.0f}},{{5.8f,1.5f,9.6f},{.6f,3,4.7f}},{{-2.9f,1.5f,8.2f},{5.2f,3,.5f}},{{3.0f,1.5f,11.6f},{5.0f,3,.5f}}};
    std::vector<Fuse>fuses={{{-7.5f,.95f,3.0f}},{{7.1f,.95f,8.0f}},{{2.8f,.95f,14.0f}}};
    std::vector<HideSpot>hides={{{-8.0f,1,5.8f},1.2f},{{7.8f,1,12.8f},1.2f},{{-7.8f,1,13.4f},1.2f}};
    Camera3D cam{};cam.position={0,1.62f,13.4f};cam.up={0,1,0};cam.fovy=70;cam.projection=CAMERA_PERSPECTIVE;
    Vector3 enemy={0,1,3.5f},enemyFacing={0,0,1},target={-7,1,3},lastSeen=enemy,noisePos=enemy;EnemyState state=EnemyState::Patrol;float yaw=0,pitch=0,stamina=100,battery=100,stateTimer=0,attackTimer=0,footTimer=0,noiseTimer=0,msg=4;int got=0;bool hiding=false,dead=false,won=false,menu=true,paused=false,doorOpen=false,flashlight=true;
    auto reset=[&](){cam.position={0,1.62f,13.4f};yaw=0;pitch=0;enemy={0,1,3.5f};enemyFacing={0,0,1};target={-7,1,3};lastSeen=enemy;noisePos=enemy;state=EnemyState::Patrol;stateTimer=attackTimer=footTimer=noiseTimer=0;stamina=battery=100;got=0;hiding=dead=won=doorOpen=false;msg=4;for(auto&f:fuses)f.taken=false;};
    while(!WindowShouldClose()){
        float dt=std::min(GetFrameTime(),.05f); if(IsKeyPressed(KEY_F11))ToggleFullscreen();
        if(menu){BeginDrawing();ClearBackground({7,8,10,255});DrawText("NIGHT HOUSE",412,160,64,RAYWHITE);DrawText("A small original survival-horror game.",432,250,20,{170,170,170,255});DrawText("ENTER",560,350,28,RAYWHITE);DrawText("WASD move   SHIFT sprint   E interact/hide   F flashlight   ESC pause",285,420,17,{145,145,145,255});DrawText("F11 fullscreen",540,460,17,{120,120,120,255});EndDrawing();if(IsKeyPressed(KEY_ENTER)){menu=false;DisableCursor();}continue;}
        if(IsKeyPressed(KEY_ESCAPE)){paused=!paused;if(paused)EnableCursor();else DisableCursor();}
        if(paused){BeginDrawing();ClearBackground({8,9,11,235});DrawText("PAUSED",535,245,52,RAYWHITE);DrawText("ESC resume",548,330,20,{175,175,175,255});DrawText("F11 fullscreen",510,365,18,{145,145,145,255});EndDrawing();continue;}
        if(dead||won){BeginDrawing();ClearBackground(dead?Color{23,3,6,255}:Color{5,25,10,255});DrawText(dead?"SHE CAUGHT YOU":"YOU ESCAPED",dead?392:430,230,60,RAYWHITE);DrawText(dead?"The chase gave you time to react.":"You made it out of the house.",dead?412:452,315,20,{190,190,190,255});DrawText("ENTER restart",525,400,21,RAYWHITE);EndDrawing();if(IsKeyPressed(KEY_ENTER))reset();continue;}
        Vector2 md=GetMouseDelta();yaw+=md.x*.00235f;pitch=Clamp(pitch-md.y*.0020f,-1.18f,1.18f);
        Vector3 forward={std::sin(yaw),0,std::cos(yaw)},right={std::cos(yaw),0,-std::sin(yaw)};cam.target=Vector3Add(cam.position,{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)});
        bool nearHide=false;for(const auto&h:hides)if(FlatDist(cam.position,h.p)<h.radius)nearHide=true;if(IsKeyPressed(KEY_E)&&nearHide)hiding=!hiding;
        bool moving=IsKeyDown(KEY_W)||IsKeyDown(KEY_A)||IsKeyDown(KEY_S)||IsKeyDown(KEY_D);bool sprint=IsKeyDown(KEY_LEFT_SHIFT)&&moving&&stamina>1&&!hiding;float speed=sprint?3.8f:2.25f;if(sprint)stamina=std::max(0.f,stamina-dt*30.f);else stamina=std::min(100.f,stamina+dt*23.f);
        Vector3 mv{};if(IsKeyDown(KEY_W))mv=Vector3Add(mv,forward);if(IsKeyDown(KEY_S))mv=Vector3Subtract(mv,forward);if(IsKeyDown(KEY_D))mv=Vector3Add(mv,right);if(IsKeyDown(KEY_A))mv=Vector3Subtract(mv,right);if(Vector3Length(mv)>.01f&&!hiding)MoveBody(cam.position,Vector3Scale(Vector3Normalize(mv),speed*dt),.34f,walls);
        if(IsKeyPressed(KEY_F)&&battery>1)flashlight=!flashlight;if(flashlight)battery=std::max(0.f,battery-dt*0.55f);
        if(moving&&footTimer<=0){footTimer=sprint?.27f:.46f;SetSoundVolume(foot,sprint?.24f:.13f);PlaySound(foot);if(sprint){noisePos=cam.position;noiseTimer=1.0f;}}else footTimer-=dt;
        for(auto&f:fuses)if(!f.taken&&FlatDist(cam.position,f.p)<1.05f&&IsKeyPressed(KEY_E)){f.taken=true;got++;PlaySound(pickup);noisePos=cam.position;noiseTimer=2.0f;msg=2.2f;}
        if(got==3&&!doorOpen&&cam.position.z<0&&IsKeyPressed(KEY_E)){doorOpen=true;PlaySound(doorS);won=true;}
        noiseTimer=std::max(0.f,noiseTimer-dt);
        float ed=FlatDist(enemy,cam.position);Vector3 toPlayer=Vector3Subtract({cam.position.x,1.62f,cam.position.z},{enemy.x,1.7f,enemy.z});if(Vector3Length(toPlayer)>.01f)toPlayer=Vector3Normalize(toPlayer);float facing=Vector3DotProduct(toPlayer,enemyFacing);bool canSee=!hiding&&ed<11.f&&facing>.55f&&ClearSight({enemy.x,1.7f,enemy.z},{cam.position.x,1.62f,cam.position.z},walls);bool heard=noiseTimer>0&&FlatDist(enemy,noisePos)<(sprint?11.f:8.f);
        if(canSee){lastSeen=cam.position;state=EnemyState::Chase;stateTimer=0;}
        else if(heard&&state!=EnemyState::Windup){target=noisePos;state=EnemyState::Investigate;stateTimer=0;}
        if(state==EnemyState::Chase){target=cam.position;if(ed>13.f||(!canSee&&stateTimer>2.3f)){target=lastSeen;state=EnemyState::Search;stateTimer=0;}else if(ed<1.9f&&!hiding&&attackTimer<=0){state=EnemyState::Windup;attackTimer=.78f;stateTimer=0;}}
        if(state==EnemyState::Windup){attackTimer-=dt;if(attackTimer<=0){if(FlatDist(enemy,cam.position)<1.35f&&!hiding)dead=true;else{state=EnemyState::Search;target=lastSeen;stateTimer=0;}}}
        else {stateTimer+=dt;if(state==EnemyState::Investigate&&FlatDist(enemy,target)<.65f){state=EnemyState::Search;stateTimer=0;}if(state==EnemyState::Search){if(stateTimer>4.0f){state=EnemyState::Patrol;stateTimer=0;}else if(FlatDist(enemy,target)<.65f)target={(float)GetRandomValue(-7,7),1,(float)GetRandomValue(2,14)};}if(state==EnemyState::Patrol&&(FlatDist(enemy,target)<.7f||GetRandomValue(0,150)==0))target={(float)GetRandomValue(-7,7),1,(float)GetRandomValue(2,14)};Vector3 ev=Vector3Subtract(target,enemy);ev.y=0;if(Vector3Length(ev)>.08f){enemyFacing=Vector3Normalize(ev);float es=state==EnemyState::Investigate?1.0f:(state==EnemyState::Search?.78f:.58f);MoveBody(enemy,Vector3Scale(enemyFacing,es*dt),.44f,walls);}}
        if(ed<5.0f&&!hiding&&GetRandomValue(0,65)==0){SetSoundVolume(heartbeat,Clamp(1.f-ed/6.f,0.f,1.f)*.42f);PlaySound(heartbeat);}if(GetRandomValue(0,320)==0)PlaySound(creak);
        BeginDrawing();ClearBackground({4,5,7,255});BeginMode3D(cam);
        DrawPlane({0,0,7},{20,22},{39,36,32,255});DrawCube({0,3.02f,7},20,.14f,22,{24,24,23,255});
        for(const auto&w:walls){Box(wood,w.p,w.s,{175,165,150,255});DrawCubeWires(w.p,w.s.x,w.s.y,w.s.z,{25,22,20,255});}
        for(int z=-1;z<17;z++)Box(wood,{0,.05f,(float)z},{18,.04f,.025f},{75,50,34,255});
        for(float x=-8.7f;x<=8.7f;x+=2.2f){Box(wood,{x,.36f,-1.55f},{1.8f,.08f,.08f},{96,62,39,255});Box(wood,{x,.38f,15.55f},{1.8f,.08f,.08f},{96,62,39,255});}
        Bed(wood,cloth,{-7.0f,.0f,12.8f});Desk(wood,metal,{6.7f,.0f,3.0f});Cabinet(wood,metal,{-7.0f,.0f,2.5f});Cabinet(wood,metal,{7.0f,.0f,13.1f});Shelf(wood,{6.65f,.0f,8.0f});Table(wood,metal,{-.8f,.0f,2.6f});
        Box(wood,{-8.0f,1.15f,5.8f},{.62f,2.3f,.62f});Box(wood,{7.8f,1.15f,12.8f},{.62f,2.3f,.62f});Box(wood,{-7.8f,1.15f,13.4f},{.62f,2.3f,.62f});
        if(!doorOpen){Box(wood,{0,1.5f,-1.25f},{2.1f,3,.20f});Box(metal,{.55f,1.5f,-1.39f},{.08f,.08f,.08f},{190,160,85,255});}
        for(const auto&f:fuses)if(!f.taken){DrawCylinder(f.p,.16f,.16f,.42f,12,{214,206,174,255});DrawSphere({f.p.x,f.p.y+.22f,f.p.z},.13f,{235,220,110,255});}
        Lamp({-6.8f,2.82f,2.0f},true);Lamp({6.8f,2.82f,7.5f},false);Lamp({-3.2f,2.82f,13.8f},true);Enemy(enemy,GetTime());EndMode3D();
        Color overlay=flashlight?Color{0,0,0,28}:Color{0,0,0,105};DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),overlay);DrawRectangle(0,0,GetScreenWidth(),86,{0,0,0,185});
        DrawText(TextFormat("FUSES  %d / 3",got),30,20,25,RAYWHITE);DrawText(hiding?"HIDDEN":"",580,22,21,{185,185,185,255});DrawText("WASD move  SHIFT run  E interact/hide  F flashlight  ESC pause",30,55,16,{155,155,155,255});
        DrawRectangle(1035,22,190,11,{28,28,28,255});DrawRectangle(1035,22,(int)(190*stamina/100.f),11,{185,185,185,255});DrawText("STAMINA",1035,38,11,{150,150,150,255});DrawRectangle(1035,59,190,8,{28,28,28,255});DrawRectangle(1035,59,(int)(190*battery/100.f),8,{155,155,145,255});DrawText("BATTERY",1035,71,10,{140,140,140,255});
        if(nearHide)DrawText(hiding?"E  leave hiding":"E  hide",540,620,19,RAYWHITE);if(got<3)DrawText("Find the three fuses. Stay quiet when you can.",405,655,19,RAYWHITE);else DrawText("The front door is unlocked. Get outside.",435,655,19,RAYWHITE);if(msg>0){msg-=dt;}if(state==EnemyState::Chase)DrawText("SHE HEARD YOU",515,104,17,{205,160,160,255});Crosshair();EndDrawing();
    }
    UnloadSound(pickup);UnloadSound(doorS);UnloadSound(heartbeat);UnloadSound(attackS);UnloadSound(creak);UnloadSound(foot);UnloadModel(wood);UnloadModel(metal);UnloadModel(cloth);CloseAudioDevice();CloseWindow();return 0;
}
