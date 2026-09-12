#ifndef _WIN32
#include "platform.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <unordered_map>
#include <cstring>
#include <stdexcept>
#include <algorithm>

class X11Platform final: public Platform{
    Display* d_=nullptr; Window w_=0; GC gc_=0; Pixmap back_=0; XFontStruct* font_=nullptr; XFontStruct* bold_=nullptr;
    int width_=0,height_=0; bool fullscreen_=false; Atom wm_delete_=0, net_wm_state_=0, net_wm_fullscreen_=0;
    std::unordered_map<unsigned,int> colors_;
    unsigned long pixel(Color c){unsigned k=(unsigned(c.r)<<16)|(unsigned(c.g)<<8)|c.b;auto it=colors_.find(k);if(it!=colors_.end())return it->second;XColor xc{};xc.red=c.r*257;xc.green=c.g*257;xc.blue=c.b*257;xc.flags=DoRed|DoGreen|DoBlue;XAllocColor(d_,DefaultColormap(d_,DefaultScreen(d_)),&xc);colors_[k]=xc.pixel;return xc.pixel;}
    void resize_back(int nw,int nh){if(nw<=0||nh<=0)return;width_=nw;height_=nh;if(back_)XFreePixmap(d_,back_);back_=XCreatePixmap(d_,w_,width_,height_,DefaultDepth(d_,DefaultScreen(d_)));}
public:
    X11Platform(int w,int h,const char* title){
        d_=XOpenDisplay(nullptr);if(!d_)throw std::runtime_error("XOpenDisplay failed");int s=DefaultScreen(d_);width_=w;height_=h;
        w_=XCreateSimpleWindow(d_,RootWindow(d_,s),50,50,w,h,0,BlackPixel(d_,s),BlackPixel(d_,s));
        XSelectInput(d_,w_,ExposureMask|KeyPressMask|ButtonPressMask|PointerMotionMask|StructureNotifyMask);
        wm_delete_=XInternAtom(d_,"WM_DELETE_WINDOW",False);XSetWMProtocols(d_,w_,&wm_delete_,1);
        net_wm_state_=XInternAtom(d_,"_NET_WM_STATE",False);net_wm_fullscreen_=XInternAtom(d_,"_NET_WM_STATE_FULLSCREEN",False);
        XStoreName(d_,w_,title);gc_=XCreateGC(d_,w_,0,nullptr);font_=XLoadQueryFont(d_,"9x15");if(!font_)font_=XLoadQueryFont(d_,"fixed");bold_=XLoadQueryFont(d_,"9x15bold");if(!bold_)bold_=font_;resize_back(w,h);XMapWindow(d_,w_);XFlush(d_);
    }
    ~X11Platform()override{if(back_)XFreePixmap(d_,back_);if(gc_)XFreeGC(d_,gc_);if(font_)XFreeFont(d_,font_);if(bold_&&bold_!=font_)XFreeFont(d_,bold_);if(w_)XDestroyWindow(d_,w_);if(d_)XCloseDisplay(d_);}
    bool poll(InputState& in)override{
        in=InputState{};Window rr,cr;int rx,ry,wx,wy;unsigned mask;XQueryPointer(d_,w_,&rr,&cr,&rx,&ry,&wx,&wy,&mask);in.mouse_x=wx;in.mouse_y=wy;
        while(XPending(d_)){
            XEvent e;XNextEvent(d_,&e);
            if(e.type==ClientMessage && (Atom)e.xclient.data.l[0]==wm_delete_)return false;
            if(e.type==ConfigureNotify && (e.xconfigure.width!=width_||e.xconfigure.height!=height_))resize_back(e.xconfigure.width,e.xconfigure.height);
            if(e.type==MotionNotify){in.mouse_x=e.xmotion.x;in.mouse_y=e.xmotion.y;}
            if(e.type==ButtonPress){in.mouse_x=e.xbutton.x;in.mouse_y=e.xbutton.y;if(e.xbutton.button==Button1)in.mouse_clicked=true;else if(e.xbutton.button==Button4)in.wheel=1;else if(e.xbutton.button==Button5)in.wheel=-1;}
            if(e.type==KeyPress){
                char buf[32]{};KeySym ks=0;int n=XLookupString(&e.xkey,buf,sizeof(buf)-1,&ks,nullptr);
                if(ks==XK_F11)in.f11=true;else if(ks==XK_Escape)in.escape=true;else if(ks==XK_BackSpace)in.backspace=true;else if(ks==XK_Return)in.enter=true;else if(n>0)in.text.append(buf,n);
            }
        }
        return true;
    }
    int width()const override{return width_;}int height()const override{return height_;}
    void clear(Color c)override{fill({0,0,width_,height_},c);}
    void fill(Rect r,Color c)override{XSetForeground(d_,gc_,pixel(c));XFillRectangle(d_,back_,gc_,r.x,r.y,std::max(0,r.w),std::max(0,r.h));}
    void stroke(Rect r,Color c)override{XSetForeground(d_,gc_,pixel(c));XDrawRectangle(d_,back_,gc_,r.x,r.y,std::max(0,r.w-1),std::max(0,r.h-1));}
    void text(int x,int y,std::string_view s,Color c,bool b)override{if(s.empty())return;auto*f=b?bold_:font_;XSetFont(d_,gc_,f->fid);XSetForeground(d_,gc_,pixel(c));std::string q(s);XDrawString(d_,back_,gc_,x,y+f->ascent,q.c_str(),(int)q.size());}
    int text_width(std::string_view s,bool b)const override{auto*f=b?bold_:font_;return XTextWidth(f,s.data(),(int)s.size());}
    void present()override{XCopyArea(d_,back_,w_,gc_,0,0,width_,height_,0,0);XFlush(d_);}
    void toggle_fullscreen()override{XEvent xev{};xev.type=ClientMessage;xev.xclient.window=w_;xev.xclient.message_type=net_wm_state_;xev.xclient.format=32;xev.xclient.data.l[0]=2;xev.xclient.data.l[1]=net_wm_fullscreen_;xev.xclient.data.l[2]=0;xev.xclient.data.l[3]=1;XSendEvent(d_,DefaultRootWindow(d_),False,SubstructureRedirectMask|SubstructureNotifyMask,&xev);fullscreen_=!fullscreen_;}
    bool fullscreen()const override{return fullscreen_;}
    void set_title(std::string_view t)override{std::string s(t);XStoreName(d_,w_,s.c_str());}
};
std::unique_ptr<Platform> create_platform(int w,int h,const char* title){return std::make_unique<X11Platform>(w,h,title);}
#endif
