#include "app.hpp"
#include <algorithm>
#include <array>
#include <sstream>

namespace {
constexpr Color BG{7,14,25}, PANEL{10,22,37}, PANEL2{13,28,47}, BORDER{31,66,88}, TEXT{220,232,243}, MUTED{132,155,177}, CYAN{58,225,213}, PURPLE{165,139,255}, WHITE{245,249,255}, RED{255,104,104};
struct Nav { const char* key; const char* label; const char* icon; };
constexpr std::array<Nav,6> NAV{{
 {"","Dashboard","#"},{"items","Items","I"},{"monsters","Monster","M"},{"skills","Skills","S"},{"classes","Klassen","C"},{"maps","Maps","W"}
}};
std::string upper(std::string s){for(char& c:s)c=(char)std::toupper((unsigned char)c);return s;}
}

void App::refresh(){ rows_=db_.query(section_,search_); selected_=rows_.empty()?-1:std::clamp(selected_,0,(int)rows_.size()-1); scroll_=std::max(0,scroll_); dirty_=false; }

void App::frame(const InputState& in){
    if(in.f11) p_.toggle_fullscreen();
    if(in.escape && !search_.empty()){search_.clear();dirty_=true;}
    if(in.backspace && !search_.empty()){search_.pop_back();dirty_=true;}
    if(!in.text.empty()){ for(unsigned char c:in.text) if(c>=32 && c!=127){ search_.push_back((char)c); dirty_=true; } }
    if(dirty_)refresh();
    p_.clear(BG);
    const int top=54, side=205, inspect=std::max(300,p_.width()/4);
    p_.fill({0,0,p_.width(),top},PANEL); p_.stroke({0,top-1,p_.width(),1},BORDER);
    p_.text(18,20,p_.width()<1050?"AL // REF OS":"ADVENTURE LAND // REF OS DESKTOP",CYAN,true);
    p_.text(p_.width()-280,20,p_.fullscreen()?"F11  FULLSCREEN // ON":"F11  FULLSCREEN",MUTED,false);
    const int search_x=std::max(side+18,330);
    Rect sr{search_x,10,std::max(200,p_.width()-search_x-inspect-30),34}; p_.fill(sr,PANEL2);p_.stroke(sr,BORDER);
    p_.text(sr.x+12,sr.y+11,search_.empty()?"Search items, monsters, skills ...":search_,search_.empty()?MUTED:TEXT,false);
    sidebar(in,0,top,side,p_.height()-top);
    if(section_.empty()) dashboard(side,top,p_.width()-side,p_.height()-top);
    else {
        content(in,side,top,p_.width()-side-inspect,p_.height()-top);
        inspector(p_.width()-inspect,top,inspect,p_.height()-top);
    }
    p_.present();
}

void App::sidebar(const InputState& in,int x,int y,int w,int h){
    p_.fill({x,y,w,h},PANEL);p_.stroke({w-1,y,1,h},BORDER);
    int yy=y+14;
    for(auto& n:NAV){
        Rect r{x+10,yy,w-20,42}; bool active=section_==n.key;
        if(active){p_.fill(r,PANEL2);p_.stroke(r,CYAN);} 
        p_.fill({r.x+8,r.y+7,28,28},active?Color{16,54,66}:Color{14,34,50});
        p_.text(r.x+18,r.y+17,n.icon,active?CYAN:MUTED,true);
        p_.text(r.x+48,r.y+15,n.label,active?WHITE:TEXT,active);
        if(n.key[0]){auto c=std::to_string(db_.count(n.key));p_.text(r.x+r.w-p_.text_width(c)-10,r.y+15,c,MUTED,false);}
        if(in.mouse_clicked && r.contains(in.mouse_x,in.mouse_y)){section_=n.key;selected_=-1;scroll_=0;dirty_=true;}
        yy+=48;
    }
    p_.text(18,y+h-58,"NATIVE C++23",CYAN,true);p_.text(18,y+h-38,"SQLite FTS5 // X11/Win32",MUTED,false);p_.text(18,y+h-20,"No Chromium. No PHP.",MUTED,false);
}

void App::dashboard(int x,int y,int w,int h){
    p_.fill({x,y,w,h},BG); int xx=x+42, yy=y+42;
    p_.text(xx,yy,"NATIVE DESKTOP CORE",CYAN,true); yy+=42;
    p_.text(xx,yy,"Adventure Land as a fast local research tool.",WHITE,true); yy+=28;
    p_.text(xx,yy,"Current build reads a pre-indexed SQLite snapshot and renders only visible rows.",MUTED,false); yy+=50;
    const char* keys[]={"items","monsters","skills","classes","maps"}; const char* labels[]={"ITEMS","MONSTERS","SKILLS","CLASSES","MAPS"};
    for(int i=0;i<5;i++){
        int cw=180,ch=86,cx=xx+(i%3)*(cw+14),cy=yy+(i/3)*(ch+14);p_.fill({cx,cy,cw,ch},PANEL);p_.stroke({cx,cy,cw,ch},BORDER);
        p_.text(cx+16,cy+20,std::to_string(db_.count(keys[i])),i%2?CYAN:PURPLE,true);p_.text(cx+16,cy+52,labels[i],MUTED,true);
    }
    int sy=yy+210;p_.text(xx,sy,"SPEED DESIGN",CYAN,true);sy+=28;
    p_.text(xx,sy,"- SQLite FTS5 search index",TEXT,false);sy+=22;p_.text(xx,sy,"- virtualized entity rows",TEXT,false);sy+=22;p_.text(xx,sy,"- immediate native drawing",TEXT,false);sy+=22;p_.text(xx,sy,"- F11 native fullscreen",TEXT,false);
}

void App::content(const InputState& in,int x,int y,int w,int h){
    p_.fill({x,y,w,h},BG);p_.stroke({x+w-1,y,1,h},BORDER);
    const int rowh=48, header=54; int visible=std::max(1,(h-header)/rowh);
    scroll_=std::clamp(scroll_-in.wheel*3,0,std::max(0,(int)rows_.size()-visible));
    p_.text(x+18,y+18,upper(section_),WHITE,true);std::string meta=std::to_string(rows_.size())+" RESULTS";p_.text(x+w-p_.text_width(meta)-18,y+18,meta,MUTED,false);
    int yy=y+header;
    for(int i=0;i<visible && scroll_+i<(int)rows_.size();++i){int idx=scroll_+i;auto& e=rows_[idx];Rect r{x+8,yy+i*rowh,w-16,rowh-2};
        if(idx==selected_)p_.fill(r,PANEL2); else if(i%2)p_.fill(r,Color{8,18,30});
        if(idx==selected_)p_.stroke(r,CYAN);
        p_.text(r.x+12,r.y+10,e.name,idx==selected_?WHITE:TEXT,idx==selected_);p_.text(r.x+12,r.y+28,e.id,MUTED,false);
        if(!e.kind.empty())p_.text(r.x+r.w-p_.text_width(e.kind)-12,r.y+18,e.kind,PURPLE,false);
        if(in.mouse_clicked && r.contains(in.mouse_x,in.mouse_y))selected_=idx;
    }
    if(rows_.empty())p_.text(x+24,y+78,"No matching records.",MUTED,false);
}

void App::wrapped_text(int x,int& y,int maxw,const std::string& s,Color c,int max_lines){
    std::istringstream is(s);std::string word,line;int lines=0;
    while(is>>word && lines<max_lines){std::string test=line.empty()?word:line+" "+word;if(p_.text_width(test)>maxw && !line.empty()){p_.text(x,y,line,c,false);y+=20;line=word;++lines;}else line=test;}
    if(!line.empty()&&lines<max_lines){p_.text(x,y,line,c,false);y+=20;}
}

void App::inspector(int x,int y,int w,int h){
    p_.fill({x,y,w,h},PANEL); if(selected_<0 || selected_>=(int)rows_.size()){p_.text(x+18,y+20,"ENTITY INSPECTOR",CYAN,true);p_.text(x+18,y+52,"Select a row.",MUTED,false);return;}
    auto&e=rows_[selected_];int yy=y+20;p_.text(x+18,yy,"ENTITY INSPECTOR",CYAN,true);yy+=34;p_.text(x+18,yy,e.name,WHITE,true);yy+=24;p_.text(x+18,yy,e.section+" / "+e.id,MUTED,false);yy+=34;
    if(!e.description.empty()){p_.text(x+18,yy,"DESCRIPTION",PURPLE,true);yy+=24;wrapped_text(x+18,yy,w-36,e.description,TEXT,7);yy+=12;}
    auto fs=db_.fields(e.section,e.id);if(!fs.empty()){p_.text(x+18,yy,"FIELDS",PURPLE,true);yy+=26;for(auto&f:fs){if(yy>y+h-40)break;p_.text(x+18,yy,f.key,MUTED,false);p_.text(x+w/2,yy,f.value,TEXT,false);yy+=20;}}
    if(fs.empty()){p_.text(x+18,yy,"SOURCE",PURPLE,true);yy+=24;p_.text(x+18,yy,"Local normalized snapshot",MUTED,false);yy+=22;p_.text(x+18,yy,"Raw data kept in SQLite",MUTED,false);}
}
