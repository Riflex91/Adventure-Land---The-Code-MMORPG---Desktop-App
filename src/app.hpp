#pragma once
#include "database.hpp"
#include "platform.hpp"
#include <string>
#include <vector>

class App {
    Platform& p_; Database& db_;
    std::string section_="";
    std::string search_;
    std::vector<Entity> rows_;
    int selected_=-1;
    int scroll_=0;
    bool dirty_=true;
public:
    App(Platform& p,Database& db):p_(p),db_(db){}
    void frame(const InputState& in);
private:
    void refresh();
    void sidebar(const InputState& in,int x,int y,int w,int h);
    void content(const InputState& in,int x,int y,int w,int h);
    void inspector(int x,int y,int w,int h);
    void dashboard(int x,int y,int w,int h);
    void wrapped_text(int x,int& y,int maxw,const std::string& s,Color c,int max_lines=8);
};
