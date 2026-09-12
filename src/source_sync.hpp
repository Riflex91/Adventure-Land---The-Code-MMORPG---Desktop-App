#pragma once
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
struct SyncResult { bool ok=false; nlohmann::json snapshot; std::string error; };
class SourceSync {
public:
    using Progress = std::function<void(int,int,const std::string&)>;
    SyncResult run(Progress progress={}) const;
};
