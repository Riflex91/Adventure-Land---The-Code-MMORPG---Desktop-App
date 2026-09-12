#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include "platform.hpp"
#include <windows.h>
#include <windowsx.h>
#include <stdexcept>
#include <unordered_map>
#include <algorithm>

static LRESULT CALLBACK wndproc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcA(h,m,w,l);}
class WinPlatform final:public Platform{
    HWND hwnd_{}; HDC win_{}; HDC mem_{}; HBITMAP bmp_{}; HFONT font_{},bold_{};int width_{},height_{};bool fs_=false;WINDOWPLACEMENT wp_{sizeof(WINDOWPLACEMENT)};DWORD style_{};InputState pending_{};
    void backbuffer(){if(bmp_)DeleteObject(bmp_);RECT r;GetClientRect(hwnd_,&r);width_=r.right;height_=r.bottom;bmp_=CreateCompatibleBitmap(win_,std::max(1,width_),std::max(1,height_));SelectObject(mem_,bmp_);}
    static COLORREF px(Color c){return RGB(c.r,c.g,c.b);} 
public:
    WinPlatform(int w,int h,const char* title){HINSTANCE hi=GetModuleHandleA(nullptr);WNDCLASSA wc{};wc.lpfnWndProc=wndproc;wc.hInstance=hi;wc.lpszClassName="ALRefDesktop";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassA(&wc);hwnd_=CreateWindowExA(0,wc.lpszClassName,title,WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,w,h,nullptr,nullptr,hi,nullptr);if(!hwnd_)throw std::runtime_error("CreateWindow failed");win_=GetDC(hwnd_);mem_=CreateCompatibleDC(win_);font_=CreateFontA(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH,"Consolas");bold_=CreateFontA(-16,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH,"Consolas");backbuffer();}
    ~WinPlatform()override{DeleteObject(font_);DeleteObject(bold_);if(bmp_)DeleteObject(bmp_);DeleteDC(mem_);ReleaseDC(hwnd_,win_);}
    bool poll(InputState& in)override{in=InputState{};POINT p;GetCursorPos(&p);ScreenToClient(hwnd_,&p);in.mouse_x=p.x;in.mouse_y=p.y;MSG msg;while(PeekMessageA(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)return false;if(msg.hwnd==hwnd_){if(msg.message==WM_CHAR){if(msg.wParam==8)in.backspace=true;else if(msg.wParam==13)in.enter=true;else if(msg.wParam>=32&&msg.wParam<127)in.text.push_back((char)msg.wParam);}else if(msg.message==WM_KEYDOWN){if(msg.wParam==VK_F11)in.f11=true;else if(msg.wParam==VK_ESCAPE)in.escape=true;}else if(msg.message==WM_LBUTTONDOWN){in.mouse_clicked=true;in.mouse_x=GET_X_LPARAM(msg.lParam);in.mouse_y=GET_Y_LPARAM(msg.lParam);}else if(msg.message==WM_MOUSEWHEEL)in.wheel=GET_WHEEL_DELTA_WPARAM(msg.wParam)>0?1:-1;else if(msg.message==WM_SIZE)backbuffer();}TranslateMessage(&msg);DispatchMessageA(&msg);}return true;}
    int width()const override{return width_;}int height()const override{return height_;}
    void clear(Color c)override{fill({0,0,width_,height_},c);}void fill(Rect r,Color c)override{RECT q{r.x,r.y,r.x+r.w,r.y+r.h};HBRUSH b=CreateSolidBrush(px(c));FillRect(mem_,&q,b);DeleteObject(b);}void stroke(Rect r,Color c)override{HPEN p=CreatePen(PS_SOLID,1,px(c));auto old=SelectObject(mem_,p);auto ob=SelectObject(mem_,GetStockObject(NULL_BRUSH));Rectangle(mem_,r.x,r.y,r.x+r.w,r.y+r.h);SelectObject(mem_,ob);SelectObject(mem_,old);DeleteObject(p);}void text(int x,int y,std::string_view s,Color c,bool b)override{SelectObject(mem_,b?bold_:font_);SetBkMode(mem_,TRANSPARENT);SetTextColor(mem_,px(c));TextOutA(mem_,x,y,s.data(),(int)s.size());}int text_width(std::string_view s,bool b)const override{SelectObject(mem_,b?bold_:font_);SIZE z{};GetTextExtentPoint32A(mem_,s.data(),(int)s.size(),&z);return z.cx;}void present()override{BitBlt(win_,0,0,width_,height_,mem_,0,0,SRCCOPY);} 
    void toggle_fullscreen()override{if(!fs_){style_=GetWindowLongA(hwnd_,GWL_STYLE);GetWindowPlacement(hwnd_,&wp_);MONITORINFO mi{sizeof(mi)};GetMonitorInfoA(MonitorFromWindow(hwnd_,MONITOR_DEFAULTTONEAREST),&mi);SetWindowLongA(hwnd_,GWL_STYLE,style_&~WS_OVERLAPPEDWINDOW);SetWindowPos(hwnd_,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_FRAMECHANGED|SWP_NOOWNERZORDER);fs_=true;}else{SetWindowLongA(hwnd_,GWL_STYLE,style_);SetWindowPlacement(hwnd_,&wp_);SetWindowPos(hwnd_,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED|SWP_NOOWNERZORDER);fs_=false;}}bool fullscreen()const override{return fs_;}void set_title(std::string_view t)override{std::string s(t);SetWindowTextA(hwnd_,s.c_str());}
};
std::unique_ptr<Platform> create_platform(int w,int h,const char* title){return std::make_unique<WinPlatform>(w,h,title);}
#endif
