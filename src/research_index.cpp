#include "research_index.hpp"
#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>
#include <unordered_set>

using json=nlohmann::json;
namespace {
const std::vector<Entity> EMPTY_ENTITIES;
const std::vector<IndexedRelation> EMPTY_RELATIONS;
std::string key_of(const std::string&s,const std::string&i){return s+":"+i;}
void collect_strings(const json&j,const std::string&path,std::vector<std::pair<std::string,std::string>>&out,int depth=0){
    if(depth>7||out.size()>800)return;
    if(j.is_string()){out.emplace_back(path,j.get<std::string>());return;}
    if(j.is_object())for(auto it=j.begin();it!=j.end();++it)collect_strings(it.value(),path.empty()?it.key():path+"."+it.key(),out,depth+1);
    else if(j.is_array())for(std::size_t i=0;i<j.size()&&i<300;i++)collect_strings(j[i],path+"["+std::to_string(i)+"]",out,depth+1);
}
}
void ResearchIndex::clear(){sections_.clear();entities_.clear();relations_.clear();ids_.clear();build_ms_=0;approx_bytes_=0;}
void ResearchIndex::rebuild(Database&db){
    clear();auto start=std::chrono::steady_clock::now();
    for(const auto&s:db.sections()){
        auto rows=db.all(s,100000);if(rows.empty())continue;auto&dst=sections_[s];dst=std::move(rows);
        for(auto&e:dst){entities_[key_of(e.section,e.id)]=&e;ids_[e.id].push_back(key_of(e.section,e.id));approx_bytes_+=sizeof(Entity)+e.section.size()+e.id.size()+e.name.size()+e.kind.size()+e.description.size()+e.raw.size();}
    }
    for(auto&[sec,rows]:sections_)for(auto&e:rows){
        json raw;try{raw=json::parse(e.raw);}catch(...){continue;}std::vector<std::pair<std::string,std::string>> values;collect_strings(raw,"",values);std::unordered_set<std::string> seen;auto&out=relations_[key_of(sec,e.id)];
        for(auto&[path,v]:values){auto hit=ids_.find(v);if(hit==ids_.end())continue;for(auto&target:hit->second){if(target==key_of(sec,e.id)||!seen.insert(target).second)continue;out.push_back({target,path});approx_bytes_+=target.size()+path.size()+sizeof(IndexedRelation);if(out.size()>=200)break;}if(out.size()>=200)break;}
    }
    auto end=std::chrono::steady_clock::now();build_ms_=std::chrono::duration<double,std::milli>(end-start).count();
}
const std::vector<Entity>& ResearchIndex::section(const std::string&name)const{auto it=sections_.find(name);return it==sections_.end()?EMPTY_ENTITIES:it->second;}
const Entity* ResearchIndex::find(const std::string&section,const std::string&id)const{auto it=entities_.find(key_of(section,id));return it==entities_.end()?nullptr:it->second;}
const std::vector<IndexedRelation>& ResearchIndex::related(const std::string&section,const std::string&id)const{auto it=relations_.find(key_of(section,id));return it==relations_.end()?EMPTY_RELATIONS:it->second;}
std::size_t ResearchIndex::relation_count()const{std::size_t n=0;for(auto&[k,v]:relations_)n+=v.size();return n;}
