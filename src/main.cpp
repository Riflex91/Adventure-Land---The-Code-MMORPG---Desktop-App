#include "app.hpp"
#include "analysis_engine.hpp"
#include "database.hpp"
#include "source_sync.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs=std::filesystem;

namespace {
fs::path executable_dir(const char* argv0){
    std::error_code ec;fs::path p=fs::absolute(argv0,ec);if(!ec){auto c=fs::weakly_canonical(p,ec);if(!ec)p=c;}return p.parent_path();
}
fs::path default_data_dir(){
#ifdef _WIN32
    if(const char* p=std::getenv("LOCALAPPDATA"))return fs::path(p)/"AdventureLandReferenceOS";
    if(const char* p=std::getenv("APPDATA"))return fs::path(p)/"AdventureLandReferenceOS";
    return fs::current_path()/"AdventureLandReferenceOS-data";
#else
    if(const char* p=std::getenv("XDG_DATA_HOME"))return fs::path(p)/"AdventureLandReferenceOS";
    if(const char* p=std::getenv("HOME"))return fs::path(p)/".local"/"share"/"AdventureLandReferenceOS";
    return fs::current_path()/"AdventureLandReferenceOS-data";
#endif
}
void report_error(const std::string& message){
#ifdef _WIN32
    MessageBoxA(nullptr,message.c_str(),"Adventure Land Reference OS",MB_OK|MB_ICONERROR|MB_TASKMODAL);
#else
    std::cerr<<message<<"\n";
#endif
}
int db_total(const fs::path& p){Database d;std::string e;if(!d.open(p.string(),e,true))return 0;return d.total();}
}

int app_main(int argc,char** argv){
    try{
        const fs::path exe_dir=executable_dir(argv[0]);
        bool windowed=false,safe=false,reset_data=false;
        fs::path data_dir=fs::exists(exe_dir/"portable.flag")?(exe_dir/"data"):default_data_dir();
        for(int i=1;i<argc;i++){
            std::string a=argv[i];
            if(a=="--windowed")windowed=true;
            else if(a=="--safe")safe=true;
            else if(a=="--reset-data")reset_data=true;
            else if(a=="--data-dir"&&i+1<argc)data_dir=argv[++i];
        }
        fs::create_directories(data_dir);
        const fs::path packaged_db=exe_dir/"resources"/"reference.db";
        const fs::path local_db=data_dir/"dataset.db";
        if(!fs::exists(packaged_db)){report_error("Packaged database missing: "+packaged_db.string());return 2;}
        if(reset_data&&fs::exists(local_db)){std::error_code ec;fs::rename(local_db,data_dir/("dataset.pre-reset.db"),ec);if(ec)fs::remove(local_db,ec);}
        if(!fs::exists(local_db))fs::copy_file(packaged_db,local_db,fs::copy_options::overwrite_existing);
        else{
            const int local_count=db_total(local_db),packaged_count=db_total(packaged_db);
            if(local_count<300&&packaged_count>local_count){
                std::error_code ec;fs::copy_file(local_db,data_dir/"dataset.alpha-backup.db",fs::copy_options::overwrite_existing,ec);fs::copy_file(packaged_db,local_db,fs::copy_options::overwrite_existing);
            }
        }

        if(!glfwInit()){report_error("GLFW init failed");return 3;}
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
#endif
        GLFWmonitor* monitor=windowed?nullptr:glfwGetPrimaryMonitor();const GLFWvidmode* vm=monitor?glfwGetVideoMode(monitor):nullptr;
        int width=vm?vm->width:1500,height=vm?vm->height:900;
        GLFWwindow* win=glfwCreateWindow(width,height,"Adventure Land Reference OS",monitor,nullptr);
        if(!win&&monitor){monitor=nullptr;win=glfwCreateWindow(1500,900,"Adventure Land Reference OS",nullptr,nullptr);}
        if(!win){glfwTerminate();report_error("Could not create OpenGL window");return 4;}
        glfwMakeContextCurrent(win);glfwSwapInterval(1);

        IMGUI_CHECKVERSION();ImGui::CreateContext();ImGuiIO& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;io.IniFilename=nullptr;
        ImGui::StyleColorsDark();auto& style=ImGui::GetStyle();style.WindowRounding=0;style.ChildRounding=4;style.FrameRounding=4;style.ScrollbarRounding=4;style.WindowPadding=ImVec2(8,8);style.ItemSpacing=ImVec2(7,6);
        ImGui_ImplGlfw_InitForOpenGL(win,true);ImGui_ImplOpenGL3_Init("#version 330");

        Database db;std::string error;if(!db.open(local_db.string(),error,false)){report_error("Database error: "+error);return 5;}
        AnalysisEngine engine(db);SourceSync sync;App app(db,engine,sync,data_dir.string(),safe);

        bool fullscreen=monitor!=nullptr;int wx=80,wy=80,ww=1500,wh=900;
        auto toggle_fullscreen=[&]{
            if(fullscreen){const GLFWvidmode* m=glfwGetVideoMode(glfwGetPrimaryMonitor());glfwSetWindowMonitor(win,nullptr,wx,wy,ww,wh,m?m->refreshRate:GLFW_DONT_CARE);fullscreen=false;}
            else{glfwGetWindowPos(win,&wx,&wy);glfwGetWindowSize(win,&ww,&wh);GLFWmonitor* mon=glfwGetPrimaryMonitor();const GLFWvidmode* m=glfwGetVideoMode(mon);if(m){glfwSetWindowMonitor(win,mon,0,0,m->width,m->height,m->refreshRate);fullscreen=true;}}
        };

        while(!glfwWindowShouldClose(win)){
            glfwPollEvents();
            ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
            bool fs_toggle=false,close=false;app.frame(fs_toggle,close);if(fs_toggle)toggle_fullscreen();if(fullscreen&&ImGui::IsKeyPressed(ImGuiKey_Escape,false))toggle_fullscreen();if(close)glfwSetWindowShouldClose(win,GLFW_TRUE);
            ImGui::Render();int fw=0,fh=0;glfwGetFramebufferSize(win,&fw,&fh);glViewport(0,0,fw,fh);glClearColor(0.018f,0.035f,0.055f,1.0f);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glfwSwapBuffers(win);
        }
        ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(win);glfwTerminate();return 0;
    }catch(const std::exception& e){report_error(std::string("Fatal: ")+e.what());return 1;}
}

#ifdef _WIN32
extern int __argc;
extern char** __argv;
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int){return app_main(__argc,__argv);}
#else
int main(int argc,char** argv){return app_main(argc,argv);}
#endif
