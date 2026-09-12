#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct Color { uint8_t r,g,b; };
struct Rect { int x,y,w,h; bool contains(int px,int py) const { return px>=x && py>=y && px<x+w && py<y+h; } };

struct InputState {
    int mouse_x=0, mouse_y=0;
    bool mouse_clicked=false;
    int wheel=0;
    bool backspace=false, escape=false, enter=false, f11=false;
    std::string text;
};

class Platform {
public:
    virtual ~Platform() = default;
    virtual bool poll(InputState& input)=0;
    virtual int width() const=0;
    virtual int height() const=0;
    virtual void clear(Color c)=0;
    virtual void fill(Rect r, Color c)=0;
    virtual void stroke(Rect r, Color c)=0;
    virtual void text(int x,int y,std::string_view s,Color c,bool bold=false)=0;
    virtual int text_width(std::string_view s,bool bold=false) const=0;
    virtual void present()=0;
    virtual void toggle_fullscreen()=0;
    virtual bool fullscreen() const=0;
    virtual void set_title(std::string_view title)=0;
};

std::unique_ptr<Platform> create_platform(int w,int h,const char* title);
