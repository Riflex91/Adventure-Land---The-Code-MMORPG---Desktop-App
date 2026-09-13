#include "icon_cache.hpp"
#include "icon_atlas_embedded.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace {
std::vector<unsigned char> decode_base64(const char* s){
    static int T[256]; static bool init=false;
    if(!init){std::fill(std::begin(T),std::end(T),-1);const char* a="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";for(int i=0;i<64;i++)T[(unsigned char)a[i]]=i;init=true;}
    std::vector<unsigned char> out;int val=0,valb=-8;
    for(const unsigned char* p=(const unsigned char*)s;*p;++p){if(*p=='=')break;int d=T[*p];if(d<0)continue;val=(val<<6)+d;valb+=6;if(valb>=0){out.push_back((unsigned char)((val>>valb)&0xFF));valb-=8;}}
    return out;
}
std::string lower(std::string s){for(char& c:s)c=(char)std::tolower((unsigned char)c);return s;}
}

IconCache::~IconCache(){shutdown();}

bool IconCache::initialize(std::string& error){
    shutdown();auto bytes=decode_base64(AL_ICON_ATLAS_PNG_BASE64);int channels=0;unsigned char* rgba=stbi_load_from_memory(bytes.data(),(int)bytes.size(),&width_,&height_,&channels,4);
    if(!rgba){error=stbi_failure_reason()?stbi_failure_reason():"stb_image decode failed";return false;}
    glGenTextures(1,&texture_);glBindTexture(GL_TEXTURE_2D,texture_);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width_,height_,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);glBindTexture(GL_TEXTURE_2D,0);stbi_image_free(rgba);
    regions_.reserve(AL_ICON_ENTRY_COUNT*2);
    for(std::size_t i=0;i<AL_ICON_ENTRY_COUNT;i++){auto&e=AL_ICON_ENTRIES[i];const float pad=0.5f;regions_[e.key]={(e.x+pad)/width_,(e.y+pad)/height_,(e.x+e.w-pad)/width_,(e.y+e.h-pad)/height_};}
    return true;
}
void IconCache::shutdown(){if(texture_){glDeleteTextures(1,&texture_);texture_=0;}regions_.clear();width_=height_=0;}

std::string IconCache::resolve_key(const std::string& section,const std::string& id,const std::string& kind) const{
    std::string exact=section+":"+id;if(regions_.contains(exact))return exact;
    if(section=="items"){
        std::string k=lower(kind);
        auto pick=[&](const char* name){std::string x="items:"+std::string(name);return regions_.contains(x)?x:std::string("items:__fallback");};
        if(k.find("weapon")!=std::string::npos||k.find("sword")!=std::string::npos||k.find("staff")!=std::string::npos||k.find("bow")!=std::string::npos||k.find("dagger")!=std::string::npos||k.find("mace")!=std::string::npos)return pick("__weapon");
        if(k.find("helmet")!=std::string::npos||k.find("armor")!=std::string::npos||k.find("chest")!=std::string::npos||k.find("pants")!=std::string::npos||k.find("shoe")!=std::string::npos||k.find("glove")!=std::string::npos||k.find("cape")!=std::string::npos)return pick("__armor");
        if(k.find("ring")!=std::string::npos||k.find("amulet")!=std::string::npos||k.find("earring")!=std::string::npos||k.find("belt")!=std::string::npos)return pick("__accessory");
        if(k.find("pot")!=std::string::npos||k.find("consum")!=std::string::npos)return pick("__consumable");
        if(k.find("material")!=std::string::npos||k.find("craft")!=std::string::npos)return pick("__material");
        if(k.find("scroll")!=std::string::npos)return pick("__scroll");
        if(k.find("token")!=std::string::npos)return pick("__token");
        if(k.find("orb")!=std::string::npos)return pick("__orb");
        return pick("__fallback");
    }
    std::string fallback=section+":__fallback";return regions_.contains(fallback)?fallback:std::string();
}
bool IconCache::has(const std::string& section,const std::string& id) const{return regions_.contains(section+":"+id);}
bool IconCache::draw(const std::string& section,const std::string& id,const std::string& kind,float size) const{
    if(!texture_)return false;auto key=resolve_key(section,id,kind);auto it=regions_.find(key);if(it==regions_.end())return false;const auto&r=it->second;ImGui::Image((ImTextureID)(intptr_t)texture_,ImVec2(size,size),ImVec2(r.u0,r.v0),ImVec2(r.u1,r.v1));return true;
}
