from pathlib import Path
import re

p = Path('src/game9.cpp')
s = p.read_text(encoding='utf-8')

# Fix the malformed patrol waypoint expression in the generated game9 source.
s = s.replace('target=Vector3{{0,1,11},{-15,1,11},{15,1,2},{0,1,-4},{0,1,-12},{0,1,5}}[pi];', 'static const Vector3 patrol[6]={{0,1,11},{-15,1,11},{15,1,2},{0,1,-4},{0,1,-12},{0,1,5}};target=patrol[pi];')

# Supply the missing crosshair helper.
marker = 'int main(){'
if 'static void Crosshair()' not in s:
    s = s.replace(marker, 'static void Crosshair(){int cx=GetScreenWidth()/2,cy=GetScreenHeight()/2;DrawLine(cx-7,cy,cx+7,cy,Color{220,220,220,190});DrawLine(cx,cy-7,cx,cy+7,Color{220,220,220,190});}\n'+marker, 1)

# Add lightweight grid A* navigation for the enemy. The existing collision walls remain
# authoritative, while the planner chooses a route around them on a 0.5m grid.
if 'static bool NavMove(' not in s:
    nav = r'''
static bool NavBlocked(Vector3 p, float r, const std::vector<Wall>& ws){
    if(p.x < -20.4f || p.x > 20.4f || p.z < -16.4f || p.z > 16.4f) return true;
    for(const auto& wall:ws) if(Hit(p,wall,r)) return true;
    return false;
}
static void NavMove(Vector3& p, Vector3 goal, float step, float radius, const std::vector<Wall>& ws){
    const float G=.5f; const int W=82,H=66; const int N=W*H;
    auto ix=[](int x,int z){return z*W+x;};
    auto gx=[](float x){return std::clamp((int)std::floor((x+20.5f)/G),0,W-1);};
    auto gz=[](float z){return std::clamp((int)std::floor((z+16.5f)/G),0,H-1);};
    auto pos=[](int x,int z){return Vector3{-20.25f+x*.5f,1.f,-16.25f+z*.5f};};
    int sx=gx(p.x),sz=gz(p.z),tx=gx(goal.x),tz=gz(goal.z);
    if(std::abs(sx-tx)+std::abs(sz-tz)>150){ Move(p,Vector3Scale(Vector3Normalize(Vector3Subtract(goal,p)),step),radius,ws); return; }
    std::vector<int> prev(N,-1),dist(N,1000000),open; open.reserve(N);
    int st=ix(sx,sz), tg=ix(tx,tz); dist[st]=0; open.push_back(st);
    for(size_t oi=0;oi<open.size();++oi){
        int cur=open[oi],cz=cur/W,cx=cur%W; if(cur==tg) break;
        const int dx[4]={1,-1,0,0},dz[4]={0,0,1,-1};
        for(int k=0;k<4;k++){int nx=cx+dx[k],nz=cz+dz[k];if(nx<0||nx>=W||nz<0||nz>=H)continue;int ni=ix(nx,nz);if(prev[ni]!=-1||ni==st)continue;Vector3 q=pos(nx,nz);if(NavBlocked(q,radius,ws))continue;prev[ni]=cur;dist[ni]=dist[cur]+1;open.push_back(ni);}
    }
    int cur=tg; if(prev[cur]==-1){Move(p,Vector3Scale(Vector3Normalize(Vector3Subtract(goal,p)),step),radius,ws);return;}
    while(prev[cur]!=st && prev[cur]!=-1) cur=prev[cur];
    Vector3 waypoint=pos(cur%W,cur/W); waypoint.y=p.y;
    Vector3 d=Vector3Subtract(waypoint,p); d.y=0;
    if(Vector3Length(d)>.01f) Move(p,Vector3Scale(Vector3Normalize(d),step),radius,ws);
}
'''
    s = s.replace(marker, nav+marker, 1)

s = s.replace('Move(enemy,Vector3Scale(face,1.45f*dt),.43f,w);', 'NavMove(enemy,target,1.45f*dt,.43f,w);')
s = s.replace('Move(enemy,Vector3Scale(face,2.0f*dt),.43f,w);', 'NavMove(enemy,target,2.0f*dt,.43f,w);')
s = s.replace('Move(enemy,Vector3Scale(face,1.05f*dt),.43f,w);', 'NavMove(enemy,target,1.05f*dt,.43f,w);')
s = s.replace('Move(enemy,Vector3Scale(face,4.05f*dt),.43f,w);', 'NavMove(enemy,target,4.05f*dt,.43f,w);')

p.write_text(s, encoding='utf-8')
