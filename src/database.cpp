#include "database.hpp"
#include <sstream>

Database::~Database(){ if(db_) sqlite3_close(db_); }

bool Database::open(const std::string& path,std::string& error){
    if(sqlite3_open_v2(path.c_str(),&db_,SQLITE_OPEN_READONLY|SQLITE_OPEN_NOMUTEX,nullptr)!=SQLITE_OK){
        error=db_?sqlite3_errmsg(db_):"sqlite open failed"; return false;
    }
    sqlite3_exec(db_,"PRAGMA query_only=ON; PRAGMA temp_store=MEMORY; PRAGMA cache_size=-8192;",nullptr,nullptr,nullptr);
    return true;
}

int Database::count(const std::string& section) const{
    sqlite3_stmt* st=nullptr; int out=0;
    sqlite3_prepare_v2(db_,"SELECT count(*) FROM entities WHERE section=?1",-1,&st,nullptr);
    sqlite3_bind_text(st,1,section.c_str(),-1,SQLITE_TRANSIENT);
    if(sqlite3_step(st)==SQLITE_ROW) out=sqlite3_column_int(st,0);
    sqlite3_finalize(st); return out;
}
int Database::total() const{
    sqlite3_stmt* st=nullptr; int out=0; sqlite3_prepare_v2(db_,"SELECT count(*) FROM entities",-1,&st,nullptr);
    if(sqlite3_step(st)==SQLITE_ROW) out=sqlite3_column_int(st,0);
    sqlite3_finalize(st);
    return out;
}

static std::string fts_query(const std::string& q){
    std::istringstream is(q); std::string tok,out;
    while(is>>tok){
        std::string clean; for(char c:tok) if((unsigned char)c>=128 || std::isalnum((unsigned char)c) || c=='_' || c=='-') clean+=c;
        if(clean.empty()) continue;
        if(!out.empty()) out+=' ';
        out+='"'; out+=clean; out+="\"*";
    }
    return out;
}

std::vector<Entity> Database::query(const std::string& section,const std::string& search,int limit) const{
    std::vector<Entity> out; sqlite3_stmt* st=nullptr;
    if(search.empty()){
        const char* sql="SELECT section,id,name,kind,description,raw_json FROM entities WHERE (?1='' OR section=?1) ORDER BY sort_name LIMIT ?2";
        sqlite3_prepare_v2(db_,sql,-1,&st,nullptr); sqlite3_bind_text(st,1,section.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_int(st,2,limit);
    }else{
        const char* sql="SELECT e.section,e.id,e.name,e.kind,e.description,e.raw_json FROM entities_fts f JOIN entities e ON e.section=f.section AND e.id=f.id WHERE entities_fts MATCH ?1 AND (?2='' OR e.section=?2) ORDER BY bm25(entities_fts),e.sort_name LIMIT ?3";
        sqlite3_prepare_v2(db_,sql,-1,&st,nullptr); auto fq=fts_query(search); sqlite3_bind_text(st,1,fq.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(st,2,section.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_int(st,3,limit);
    }
    while(sqlite3_step(st)==SQLITE_ROW){
        Entity e;
        auto col=[&](int i){const unsigned char* p=sqlite3_column_text(st,i); return p?std::string((const char*)p):std::string();};
        e.section=col(0);e.id=col(1);e.name=col(2);e.kind=col(3);e.description=col(4);e.raw=col(5);out.push_back(std::move(e));
    }
    sqlite3_finalize(st); return out;
}
std::vector<Field> Database::fields(const std::string& section,const std::string& id) const{
    std::vector<Field> out; sqlite3_stmt* st=nullptr;
    sqlite3_prepare_v2(db_,"SELECT key,value FROM entity_fields WHERE section=?1 AND id=?2 ORDER BY ord",-1,&st,nullptr);
    sqlite3_bind_text(st,1,section.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_text(st,2,id.c_str(),-1,SQLITE_TRANSIENT);
    while(sqlite3_step(st)==SQLITE_ROW){ const char* a=(const char*)sqlite3_column_text(st,0); const char* b=(const char*)sqlite3_column_text(st,1); out.push_back({a?a:"",b?b:""}); }
    sqlite3_finalize(st); return out;
}
std::string Database::meta(const std::string& key) const{
    sqlite3_stmt* st=nullptr; std::string out; sqlite3_prepare_v2(db_,"SELECT value FROM meta WHERE key=?1",-1,&st,nullptr); sqlite3_bind_text(st,1,key.c_str(),-1,SQLITE_TRANSIENT);
    if(sqlite3_step(st)==SQLITE_ROW){const char* p=(const char*)sqlite3_column_text(st,0); if(p)out=p;} sqlite3_finalize(st); return out;
}
