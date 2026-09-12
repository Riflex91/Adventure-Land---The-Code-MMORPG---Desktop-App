#include "database.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

using json = nlohmann::json;

namespace {
std::string col_text(sqlite3_stmt* st,int i){
    const unsigned char* p=sqlite3_column_text(st,i);
    return p?std::string(reinterpret_cast<const char*>(p)):std::string();
}
std::string fts_query(const std::string& q){
    std::istringstream is(q); std::string tok,out;
    while(is>>tok){
        std::string clean;
        for(unsigned char c:tok) if(c>=128 || std::isalnum(c) || c=='_' || c=='-') clean+=(char)c;
        if(clean.empty()) continue;
        if(!out.empty()) out+=' ';
        out+='"'; out+=clean; out+="\"*";
    }
    return out;
}
std::string stringify_scalar(const json& v){
    if(v.is_null()) return "null";
    if(v.is_boolean()) return v.get<bool>()?"true":"false";
    if(v.is_string()) return v.get<std::string>();
    if(v.is_number_integer()) return std::to_string(v.get<long long>());
    if(v.is_number_unsigned()) return std::to_string(v.get<unsigned long long>());
    if(v.is_number_float()) { std::ostringstream o; o<<v.get<double>(); return o.str(); }
    return v.dump();
}
std::string entity_name(const std::string& id,const json& d){
    for(auto* k:{"name","title","officialName"}) if(d.contains(k)&&d[k].is_string()) return d[k].get<std::string>();
    return id;
}
std::string entity_desc(const json& d){
    for(auto* k:{"explanation","description","officialDescription"}) if(d.contains(k)&&d[k].is_string()) return d[k].get<std::string>();
    return {};
}
std::string entity_kind(const json& d){
    for(auto* k:{"type","kind","role","damage_type"}) if(d.contains(k)&&d[k].is_string()) return d[k].get<std::string>();
    return {};
}
void flatten_fields(const json& j,const std::string& prefix,std::vector<Field>& out,int depth=0){
    if(depth>4 || out.size()>300) return;
    if(j.is_object()){
        for(auto it=j.begin();it!=j.end();++it){
            std::string p=prefix.empty()?it.key():prefix+"."+it.key();
            if(it.value().is_primitive()) out.push_back({p,stringify_scalar(it.value())});
            else flatten_fields(it.value(),p,out,depth+1);
        }
    }else if(j.is_array()){
        for(size_t i=0;i<j.size()&&i<80;i++){
            std::string p=prefix+"["+std::to_string(i)+"]";
            if(j[i].is_primitive()) out.push_back({p,stringify_scalar(j[i])});
            else flatten_fields(j[i],p,out,depth+1);
        }
    }
}
}

Database::~Database(){ close(); }

bool Database::open(const std::string& path,std::string& error,bool read_only){
    close(); path_=path;
    int flags=read_only?(SQLITE_OPEN_READONLY|SQLITE_OPEN_NOMUTEX):(SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_NOMUTEX);
    if(sqlite3_open_v2(path.c_str(),&db_,flags,nullptr)!=SQLITE_OK){
        error=db_?sqlite3_errmsg(db_):"sqlite open failed"; close(); return false;
    }
    sqlite3_busy_timeout(db_,3000);
    if(!read_only){
        exec("PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL; PRAGMA temp_store=MEMORY; PRAGMA cache_size=-32768;");
    }else exec("PRAGMA query_only=ON; PRAGMA temp_store=MEMORY; PRAGMA cache_size=-16384;");
    return true;
}
void Database::close(){ if(db_){sqlite3_close(db_);db_=nullptr;} }

bool Database::exec(const char* sql,std::string* error) const{
    char* msg=nullptr; const int rc=sqlite3_exec(db_,sql,nullptr,nullptr,&msg);
    if(rc!=SQLITE_OK){ if(error)*error=msg?msg:sqlite3_errmsg(db_); if(msg)sqlite3_free(msg); return false; }
    return true;
}

int Database::count(const std::string& section) const{
    if(!db_)return 0; sqlite3_stmt* st=nullptr; int out=0;
    sqlite3_prepare_v2(db_,"SELECT count(*) FROM entities WHERE section=?1",-1,&st,nullptr);
    sqlite3_bind_text(st,1,section.c_str(),-1,SQLITE_TRANSIENT);
    if(sqlite3_step(st)==SQLITE_ROW)out=sqlite3_column_int(st,0); sqlite3_finalize(st); return out;
}
int Database::total() const{
    if(!db_)return 0; sqlite3_stmt* st=nullptr;int out=0;sqlite3_prepare_v2(db_,"SELECT count(*) FROM entities",-1,&st,nullptr);
    if(sqlite3_step(st)==SQLITE_ROW)out=sqlite3_column_int(st,0);sqlite3_finalize(st);return out;
}
std::vector<std::string> Database::sections() const{
    std::vector<std::string> out; if(!db_)return out; sqlite3_stmt* st=nullptr;
    sqlite3_prepare_v2(db_,"SELECT section FROM section_blobs ORDER BY section",-1,&st,nullptr);
    while(sqlite3_step(st)==SQLITE_ROW)out.push_back(col_text(st,0)); sqlite3_finalize(st);return out;
}
std::vector<Entity> Database::query(const std::string& section,const std::string& search,int limit) const{
    std::vector<Entity> out; if(!db_)return out; sqlite3_stmt* st=nullptr;
    if(search.empty()){
        sqlite3_prepare_v2(db_,"SELECT section,id,name,kind,description,raw_json FROM entities WHERE (?1='' OR section=?1) ORDER BY sort_name LIMIT ?2",-1,&st,nullptr);
        sqlite3_bind_text(st,1,section.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_int(st,2,limit);
    }else{
        const auto fq=fts_query(search); if(fq.empty())return out;
        sqlite3_prepare_v2(db_,"SELECT e.section,e.id,e.name,e.kind,e.description,e.raw_json FROM entities_fts f JOIN entities e ON e.section=f.section AND e.id=f.id WHERE entities_fts MATCH ?1 AND (?2='' OR e.section=?2) ORDER BY bm25(entities_fts),e.sort_name LIMIT ?3",-1,&st,nullptr);
        sqlite3_bind_text(st,1,fq.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,2,section.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_int(st,3,limit);
    }
    while(sqlite3_step(st)==SQLITE_ROW)out.push_back({col_text(st,0),col_text(st,1),col_text(st,2),col_text(st,3),col_text(st,4),col_text(st,5)});
    sqlite3_finalize(st);return out;
}
std::optional<Entity> Database::entity(const std::string& section,const std::string& id) const{
    if(!db_)return std::nullopt;sqlite3_stmt* st=nullptr;std::optional<Entity> out;
    sqlite3_prepare_v2(db_,"SELECT section,id,name,kind,description,raw_json FROM entities WHERE section=?1 AND id=?2",-1,&st,nullptr);
    sqlite3_bind_text(st,1,section.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,2,id.c_str(),-1,SQLITE_TRANSIENT);
    if(sqlite3_step(st)==SQLITE_ROW)out=Entity{col_text(st,0),col_text(st,1),col_text(st,2),col_text(st,3),col_text(st,4),col_text(st,5)};
    sqlite3_finalize(st);return out;
}
json Database::entity_json(const std::string& section,const std::string& id) const{
    auto e=entity(section,id);if(!e)return json::object();try{return json::parse(e->raw);}catch(...){return json::object();}
}
json Database::section_json(const std::string& section) const{
    if(!db_)return json::object();sqlite3_stmt* st=nullptr;json out=json::object();
    sqlite3_prepare_v2(db_,"SELECT json FROM section_blobs WHERE section=?1",-1,&st,nullptr);sqlite3_bind_text(st,1,section.c_str(),-1,SQLITE_TRANSIENT);
    if(sqlite3_step(st)==SQLITE_ROW){try{out=json::parse(col_text(st,0));}catch(...){}}
    sqlite3_finalize(st);return out;
}
std::vector<Field> Database::fields(const std::string& section,const std::string& id) const{
    std::vector<Field> out;if(!db_)return out;sqlite3_stmt* st=nullptr;
    sqlite3_prepare_v2(db_,"SELECT key,value FROM entity_fields WHERE section=?1 AND id=?2 ORDER BY ord",-1,&st,nullptr);
    sqlite3_bind_text(st,1,section.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,2,id.c_str(),-1,SQLITE_TRANSIENT);
    while(sqlite3_step(st)==SQLITE_ROW)out.push_back({col_text(st,0),col_text(st,1)});sqlite3_finalize(st);return out;
}
std::string Database::meta(const std::string& key) const{
    if(!db_)return {};sqlite3_stmt* st=nullptr;std::string out;sqlite3_prepare_v2(db_,"SELECT value FROM meta WHERE key=?1",-1,&st,nullptr);sqlite3_bind_text(st,1,key.c_str(),-1,SQLITE_TRANSIENT);
    if(sqlite3_step(st)==SQLITE_ROW)out=col_text(st,0);sqlite3_finalize(st);return out;
}
bool Database::set_meta(const std::string& key,const std::string& value,std::string* error){
    sqlite3_stmt* st=nullptr;if(sqlite3_prepare_v2(db_,"INSERT INTO meta(key,value) VALUES(?1,?2) ON CONFLICT(key) DO UPDATE SET value=excluded.value",-1,&st,nullptr)!=SQLITE_OK){if(error)*error=sqlite3_errmsg(db_);return false;}
    sqlite3_bind_text(st,1,key.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(st,2,value.c_str(),-1,SQLITE_TRANSIENT);const bool ok=sqlite3_step(st)==SQLITE_DONE;if(!ok&&error)*error=sqlite3_errmsg(db_);sqlite3_finalize(st);return ok;
}

bool Database::replace_dataset(const json& snapshot,std::string& error){
    if(!db_){error="database not open";return false;}
    if(!snapshot.is_object()||!snapshot.contains("data")||!snapshot["data"].is_object()){error="snapshot missing data";return false;}
    if(!exec("BEGIN IMMEDIATE",&error))return false;
    auto rollback=[&]{exec("ROLLBACK");};
    const char* schema=R"SQL(
CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY,value TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS entities(section TEXT NOT NULL,id TEXT NOT NULL,name TEXT NOT NULL,kind TEXT,description TEXT,sort_name TEXT NOT NULL,raw_json TEXT NOT NULL,PRIMARY KEY(section,id));
CREATE TABLE IF NOT EXISTS entity_fields(section TEXT NOT NULL,id TEXT NOT NULL,ord INTEGER NOT NULL,key TEXT NOT NULL,value TEXT,PRIMARY KEY(section,id,ord));
CREATE TABLE IF NOT EXISTS section_blobs(section TEXT PRIMARY KEY,json TEXT NOT NULL);
CREATE VIRTUAL TABLE IF NOT EXISTS entities_fts USING fts5(section UNINDEXED,id UNINDEXED,name,kind,description,body,tokenize='unicode61 remove_diacritics 2');
)SQL";
    if(!exec(schema,&error)){rollback();return false;}
    if(!exec("DELETE FROM entities;DELETE FROM entity_fields;DELETE FROM section_blobs;DELETE FROM entities_fts;",&error)){rollback();return false;}
    sqlite3_stmt *ie=nullptr,*iff=nullptr,*ib=nullptr,*ifx=nullptr;
    sqlite3_prepare_v2(db_,"INSERT INTO entities(section,id,name,kind,description,sort_name,raw_json) VALUES(?1,?2,?3,?4,?5,lower(?3),?6)",-1,&ie,nullptr);
    sqlite3_prepare_v2(db_,"INSERT INTO entity_fields(section,id,ord,key,value) VALUES(?1,?2,?3,?4,?5)",-1,&iff,nullptr);
    sqlite3_prepare_v2(db_,"INSERT INTO section_blobs(section,json) VALUES(?1,?2)",-1,&ib,nullptr);
    sqlite3_prepare_v2(db_,"INSERT INTO entities_fts(section,id,name,kind,description,body) VALUES(?1,?2,?3,?4,?5,?6)",-1,&ifx,nullptr);
    const std::unordered_set<std::string> entity_sections={"items","monsters","skills","classes","maps","npcs","craft","sets","tokens","dismantle","conditions","quests","events","achievements","projectiles","cosmetics","titles","games"};
    try{
        for(auto sit=snapshot["data"].begin();sit!=snapshot["data"].end();++sit){
            const std::string section=sit.key();const json& obj=sit.value();const std::string blob=obj.dump();
            sqlite3_reset(ib);sqlite3_clear_bindings(ib);sqlite3_bind_text(ib,1,section.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(ib,2,blob.c_str(),-1,SQLITE_TRANSIENT);if(sqlite3_step(ib)!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(db_));
            if(!entity_sections.count(section)||!obj.is_object())continue;
            for(auto it=obj.begin();it!=obj.end();++it){
                const std::string id=it.key();const json& d=it.value();if(!d.is_object()&&!d.is_array())continue;
                const std::string name=d.is_object()?entity_name(id,d):id,desc=d.is_object()?entity_desc(d):std::string(),kind=d.is_object()?entity_kind(d):std::string(),raw=d.dump();
                std::vector<Field> fs;flatten_fields(d,"",fs);
                std::string body=id+" "+name+" "+desc+" "+kind;for(const auto& f:fs){body+=' ';body+=f.key;body+=' ';body+=f.value;}
                sqlite3_reset(ie);sqlite3_clear_bindings(ie);sqlite3_bind_text(ie,1,section.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(ie,2,id.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(ie,3,name.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(ie,4,kind.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(ie,5,desc.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(ie,6,raw.c_str(),-1,SQLITE_TRANSIENT);if(sqlite3_step(ie)!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(db_));
                int ord=0;for(const auto& f:fs){sqlite3_reset(iff);sqlite3_clear_bindings(iff);sqlite3_bind_text(iff,1,section.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(iff,2,id.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_int(iff,3,ord++);sqlite3_bind_text(iff,4,f.key.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(iff,5,f.value.c_str(),-1,SQLITE_TRANSIENT);if(sqlite3_step(iff)!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(db_));}
                sqlite3_reset(ifx);sqlite3_clear_bindings(ifx);sqlite3_bind_text(ifx,1,section.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(ifx,2,id.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(ifx,3,name.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(ifx,4,kind.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(ifx,5,desc.c_str(),-1,SQLITE_TRANSIENT);sqlite3_bind_text(ifx,6,body.c_str(),-1,SQLITE_TRANSIENT);if(sqlite3_step(ifx)!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(db_));
            }
        }
        sqlite3_finalize(ie);sqlite3_finalize(iff);sqlite3_finalize(ib);sqlite3_finalize(ifx);ie=iff=ib=ifx=nullptr;
        if(snapshot.contains("meta")&&snapshot["meta"].is_object())for(auto it=snapshot["meta"].begin();it!=snapshot["meta"].end();++it)set_meta(it.key(),it.value().is_string()?it.value().get<std::string>():it.value().dump());
        set_meta("entity_count",std::to_string(total()));
        if(!exec("COMMIT",&error)){rollback();return false;}return true;
    }catch(const std::exception& e){error=e.what();if(ie)sqlite3_finalize(ie);if(iff)sqlite3_finalize(iff);if(ib)sqlite3_finalize(ib);if(ifx)sqlite3_finalize(ifx);rollback();return false;}
}

bool Database::integrity_check(std::string& error) const{
    if(!db_){error="database not open";return false;}sqlite3_stmt* st=nullptr;sqlite3_prepare_v2(db_,"PRAGMA quick_check",-1,&st,nullptr);bool ok=false;if(sqlite3_step(st)==SQLITE_ROW){auto s=col_text(st,0);ok=s=="ok";if(!ok)error=s;}sqlite3_finalize(st);return ok;
}

json Database::export_snapshot() const{
    json snap;snap["meta"]=json::object();snap["data"]=json::object();if(!db_)return snap;
    sqlite3_stmt* st=nullptr;sqlite3_prepare_v2(db_,"SELECT key,value FROM meta",-1,&st,nullptr);while(sqlite3_step(st)==SQLITE_ROW){auto k=col_text(st,0),v=col_text(st,1);try{snap["meta"][k]=json::parse(v);}catch(...){snap["meta"][k]=v;}}sqlite3_finalize(st);
    sqlite3_prepare_v2(db_,"SELECT section,json FROM section_blobs",-1,&st,nullptr);while(sqlite3_step(st)==SQLITE_ROW){auto k=col_text(st,0),v=col_text(st,1);try{snap["data"][k]=json::parse(v);}catch(...){snap["data"][k]=json::object();}}sqlite3_finalize(st);return snap;
}
