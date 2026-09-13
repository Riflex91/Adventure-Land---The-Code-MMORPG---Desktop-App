#pragma once
#include <imgui.h>
#include <string>
#include <unordered_map>

struct IconRegion {
    float u0=0, v0=0, u1=1, v1=1;
};

class IconCache {
    unsigned int texture_=0;
    int width_=0,height_=0;
    std::unordered_map<std::string,IconRegion> regions_;
public:
    ~IconCache();
    bool initialize(std::string& error);
    void shutdown();
    bool ready() const { return texture_!=0; }
    std::string resolve_key(const std::string& section,const std::string& id,const std::string& kind="") const;
    bool has(const std::string& section,const std::string& id) const;
    bool draw(const std::string& section,const std::string& id,const std::string& kind,float size=26.0f) const;
    std::size_t region_count() const { return regions_.size(); }
    std::size_t gpu_bytes() const { return static_cast<std::size_t>(width_)*static_cast<std::size_t>(height_)*4u; }
};
