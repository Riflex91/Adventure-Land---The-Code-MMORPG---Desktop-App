#pragma once
#include "database.hpp"
#include <string>
#include <unordered_map>
#include <vector>

struct IndexedRelation {
    std::string target_key;
    std::string path;
};

class ResearchIndex {
    std::unordered_map<std::string,std::vector<Entity>> sections_;
    std::unordered_map<std::string,const Entity*> entities_;
    std::unordered_map<std::string,std::vector<IndexedRelation>> relations_;
    std::unordered_map<std::string,std::vector<std::string>> ids_;
    double build_ms_=0.0;
    std::size_t approx_bytes_=0;
public:
    void rebuild(Database& db);
    void clear();
    const std::vector<Entity>& section(const std::string& name) const;
    const Entity* find(const std::string& section,const std::string& id) const;
    const std::vector<IndexedRelation>& related(const std::string& section,const std::string& id) const;
    std::size_t entity_count() const { return entities_.size(); }
    std::size_t relation_count() const;
    std::size_t approx_bytes() const { return approx_bytes_; }
    double build_ms() const { return build_ms_; }
};
