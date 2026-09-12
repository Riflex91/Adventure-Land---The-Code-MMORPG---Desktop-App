#include "source_sync.hpp"
#include "http.hpp"
#include "sha256.hpp"
#include <quickjs.h>
#include <array>
#include <chrono>
#include <cstring>
#include <sstream>

using json=nlohmann::json;
namespace {
constexpr const char* REPO="kaansoral/adventureland_mongodb";
constexpr std::array<const char*,23> FILES={"design/items.js","design/skills.js","design/monsters.js","design/classes.js","design/npcs.js","design/maps.js","design/recipes.js","design/upgrades.js","design/multipliers.js","design/dimensions.js","design/sprites.js","design/drops.js","design/tokens.js","design/conditions.js","design/quests.js","design/events.js","design/achievements.js","design/animations.js","design/cosmetics.js","design/projectiles.js","design/titles.js","design/games.js","design/levels.js"};
std::string js_error(JSContext* c){JSValue ex=JS_GetException(c);const char* s=JS_ToCString(c,ex);std::string out=s?s:"JavaScript exception";if(s)JS_FreeCString(c,s);JS_FreeValue(c,ex);return out;}
bool eval(JSContext* c,const std::string& src,const char* file,std::string& err){JSValue v=JS_Eval(c,src.c_str(),src.size(),file,JS_EVAL_TYPE_GLOBAL);if(JS_IsException(v)){err=js_error(c);JS_FreeValue(c,v);return false;}JS_FreeValue(c,v);return true;}
}

SyncResult SourceSync::run(Progress progress) const{
    SyncResult res;json revision={{"sha","main"},{"date",""},{"message",""},{"repository",REPO},{"branch","main"}};
    if(progress)progress(0,(int)FILES.size()+2,"Revision laden");
    auto rev=http_get(std::string("https://api.github.com/repos/")+REPO+"/commits/main");
    if(rev.ok())try{auto j=json::parse(rev.body);revision["sha"]=j.value("sha","main");revision["date"]=j.value("commit",json::object()).value("committer",json::object()).value("date","");revision["message"]=j.value("commit",json::object()).value("message","");}catch(...){}
    std::string ref=revision.value("sha","main");if(ref.size()!=40)ref="main";
    JSRuntime* rt=JS_NewRuntime();if(!rt){res.error="QuickJS runtime konnte nicht erstellt werden";return res;}JS_SetMemoryLimit(rt,256ull*1024*1024);JS_SetMaxStackSize(rt,4ull*1024*1024);JSContext* ctx=JS_NewContext(rt);if(!ctx){JS_FreeRuntime(rt);res.error="QuickJS context konnte nicht erstellt werden";return res;}
    std::string e;eval(ctx,"var module={exports:{}};var exports=module.exports;function require(){return {}};var window=globalThis;var self=globalThis;var global=globalThis;var min=Math.min,max=Math.max,round=Math.round,floor=Math.floor,ceil=Math.ceil;var console={log:function(){},warn:function(){},error:function(){}};","bootstrap",e);
    json sources=json::array(),errors=json::array();int successful=0;
    for(size_t i=0;i<FILES.size();++i){const std::string file=FILES[i];if(progress)progress((int)i+1,(int)FILES.size()+2,file);const std::string url=std::string("https://raw.githubusercontent.com/")+REPO+"/"+ref+"/"+file;auto r=http_get(url);if(!r.ok()){errors.push_back({{"file",file},{"error",r.error.empty()?"HTTP "+std::to_string(r.status):r.error}});continue;}sources.push_back({{"path",file},{"bytes",r.body.size()},{"sha256",sha256_hex(r.body)}});std::string err;if(!eval(ctx,r.body,file.c_str(),err)){errors.push_back({{"file",file},{"error",err}});}else successful++;}
    if(successful<8){res.error="Zu wenige Adventure-Land-Quelldateien konnten ausgewertet werden ("+std::to_string(successful)+")";JS_FreeContext(ctx);JS_FreeRuntime(rt);return res;}
    if(progress)progress((int)FILES.size()+1,(int)FILES.size()+2,"Datensatz serialisieren");
    const char* expr=R"JS(JSON.stringify({items:typeof items==='undefined'?{}:items,sets:typeof sets==='undefined'?{}:sets,skills:typeof skills==='undefined'?{}:skills,monsters:typeof monsters==='undefined'?{}:monsters,classes:typeof classes==='undefined'?{}:classes,npcs:typeof npcs==='undefined'?{}:npcs,maps:typeof maps==='undefined'?{}:maps,craft:typeof craft==='undefined'?{}:craft,dismantle:typeof dismantle==='undefined'?{}:dismantle,drops:typeof drops==='undefined'?{}:drops,tokens:typeof tokens==='undefined'?{}:tokens,conditions:typeof conditions==='undefined'?{}:conditions,quests:typeof quests==='undefined'?{}:quests,events:typeof events==='undefined'?{}:events,achievements:typeof achievements==='undefined'?{}:achievements,animations:typeof animations==='undefined'?{}:animations,cosmetics:typeof cosmetics==='undefined'?{}:cosmetics,projectiles:typeof projectiles==='undefined'?{}:projectiles,titles:typeof titles==='undefined'?{}:titles,games:typeof games==='undefined'?{}:games,levels:typeof levels==='undefined'?{}:levels,positions:typeof positions==='undefined'?{}:positions,dimensions:typeof dimensions==='undefined'?{}:dimensions,sprites:typeof sprites==='undefined'?{}:sprites,imagesets:typeof imagesets==='undefined'?{}:imagesets,tilesets:typeof tilesets==='undefined'?{}:tilesets,upgrades:typeof upgrades==='undefined'?{}:upgrades,compounds:typeof compounds==='undefined'?{}:compounds,multipliers:typeof multipliers==='undefined'?{}:multipliers},function(k,v){return typeof v==='function'?undefined:v}))JS";
    JSValue val=JS_Eval(ctx,expr,std::strlen(expr),"serialize",JS_EVAL_TYPE_GLOBAL);if(JS_IsException(val)){res.error=js_error(ctx);JS_FreeValue(ctx,val);JS_FreeContext(ctx);JS_FreeRuntime(rt);return res;}const char* text=JS_ToCString(ctx,val);if(!text){res.error="QuickJS JSON serialization failed";JS_FreeValue(ctx,val);JS_FreeContext(ctx);JS_FreeRuntime(rt);return res;}json data;try{data=json::parse(text);}catch(const std::exception& ex){res.error=ex.what();}JS_FreeCString(ctx,text);JS_FreeValue(ctx,val);JS_FreeContext(ctx);JS_FreeRuntime(rt);if(!res.error.empty())return res;
    for(auto key:{"items","skills","monsters","classes","maps","drops"})if(!data.contains(key)||!data[key].is_object()||data[key].empty()){res.error=std::string("Pflichtsektion leer: ")+key;return res;}
    std::string manifestText;for(auto& s:sources)manifestText+=s.value("path","")+":"+s.value("sha256","")+"\n";
    auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    json meta={{"schema_version","2"},{"source","Adventure Land official GitHub"},{"source_mode","live-native"},{"revision",revision.value("sha","main")},{"revision_date",revision.value("date","")},{"revision_message",revision.value("message","")},{"repository",REPO},{"branch","main"},{"manifest_sha256",sha256_hex(manifestText)},{"source_errors",errors},{"synced_unix",(long long)now}};
    res.snapshot={{"meta",meta},{"data",data},{"sources",sources},{"errors",errors}};res.ok=true;if(progress)progress((int)FILES.size()+2,(int)FILES.size()+2,"Fertig");return res;
}
