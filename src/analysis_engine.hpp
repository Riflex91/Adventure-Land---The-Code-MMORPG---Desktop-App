#pragma once
#include "database.hpp"
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct DropPath {
    std::string kind;
    std::string item;
    std::string pool;
    double raw=0.0;
    double probability=0.0;
    double quantity=1.0;
    int root_index=-1;
    std::string path;
};
struct DropAggregate { int paths=0; int roots=0; double per_kill=0; double expected_units=0; };
struct IntegrityIssue { std::string severity,code,message,section,id,path; };
struct CombatResult {
    double target_defense=0,out_multiplier=1,basic_hit=0,basic_dps=0,skill_dps=0,total_dps=0;
    double incoming_defense=0,in_multiplier=1,incoming_hit=0,incoming_dps=0,ttk=0,ttd=0,margin=0,mp_per_second=0;
};
struct SkillProjection { double cast_rate=0,damage_cast=0,damage_second=0,heal_cast=0,heal_second=0,mp_second=0; };
struct SchemaField { std::string path; std::string type; int count=0; double prevalence=0; };

class AnalysisEngine {
    Database& db_;
public:
    explicit AnalysisEngine(Database& db):db_(db){}
    std::vector<DropPath> drop_paths(const std::string& root_type,const std::string& source,double root_multiplier=1.0) const;
    std::vector<DropPath> drop_paths_from(const nlohmann::json& drops,const std::string& root_type,const std::string& source,double root_multiplier=1.0) const;
    DropAggregate aggregate_drop(const std::vector<DropPath>& paths,const std::string& item) const;
    static double probability_after(double p,long long n);
    static double damage_multiplier(double defense);
    SkillProjection skill_projection(const std::string& skill,double attack,double heal,double attempts,double targets,double target_multiplier) const;
    std::map<std::string,double> build_stats(const std::map<std::string,std::pair<std::string,int>>& slots,const std::string& class_id="",int level=1) const;
    CombatResult combat(const std::map<std::string,double>& character,const std::string& damage_type,const std::string& monster,const std::string& skill="",double crit_bonus=100.0) const;
    CombatResult combat_with_monster(const std::map<std::string,double>& character,const std::string& damage_type,const nlohmann::json& monster,const std::string& skill="",double crit_bonus=100.0) const;
    nlohmann::json craft_tree(const std::string& item,int max_depth=8) const;
    nlohmann::json acquisition(const std::string& item) const;
    std::vector<std::string> map_route(const std::string& from,const std::string& to) const;
    std::vector<IntegrityIssue> integrity_scan() const;
    std::vector<Entity> advanced_search(const std::string& query,const std::string& section="",int limit=1000) const;
    std::vector<SchemaField> schema(const std::string& section) const;
    std::vector<std::pair<std::string,double>> spawn_route(const std::string& map_id) const;
    nlohmann::json world_summary(const std::string& map_id) const;
};
