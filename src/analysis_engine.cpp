#include "analysis_engine.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <regex>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

using json=nlohmann::json;
namespace {
double num(const json& j,const char* k,double def=0){if(j.is_object()&&j.contains(k)&&j[k].is_number())return j[k].get<double>();return def;}
std::string str(const json& j,const char* k,const std::string& def={}){if(j.is_object()&&j.contains(k)&&j[k].is_string())return j[k].get<std::string>();return def;}
std::string lower(std::string s){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return(char)std::tolower(c);});return s;}
void flatten_text(const json& j,std::string& out,int depth=0){if(depth>6)return;if(j.is_primitive()){if(j.is_string())out+=' '+j.get<std::string>();else out+=' '+j.dump();return;}if(j.is_array()){for(const auto&x:j)flatten_text(x,out,depth+1);return;}if(j.is_object())for(auto it=j.begin();it!=j.end();++it){out+=' '+it.key();flatten_text(it.value(),out,depth+1);}}
const json* get_path(const json& root,const std::string& path){const json* cur=&root;std::stringstream ss(path);std::string tok;while(std::getline(ss,tok,'.')){if(tok.empty())continue;if(!cur->is_object()||!cur->contains(tok))return nullptr;cur=&(*cur)[tok];}return cur;}
bool cmp_num(double a,const std::string& op,double b){if(op==">")return a>b;if(op=="<")return a<b;if(op==">=")return a>=b;if(op=="<=")return a<=b;if(op=="=")return a==b;if(op=="!=")return a!=b;return false;}
std::string jtype(const json& v){if(v.is_object())return"object";if(v.is_array())return"array";if(v.is_string())return"string";if(v.is_boolean())return"boolean";if(v.is_number())return"number";if(v.is_null())return"null";return"unknown";}
void schema_walk(const json& j,const std::string& p,std::map<std::string,std::map<std::string,int>>& acc,int depth=0){if(depth>6)return;if(j.is_object()){for(auto it=j.begin();it!=j.end();++it){const std::string q=p.empty()?it.key():p+"."+it.key();acc[q][jtype(it.value())]++;if(it.value().is_object())schema_walk(it.value(),q,acc,depth+1);}}}
std::string item_name(const Database& db,const std::string& id){auto e=db.entity("items",id);return e?e->name:id;}
}

std::vector<DropPath> AnalysisEngine::drop_paths(const std::string& root_type,const std::string& source,double root_multiplier) const{
    return drop_paths_from(db_.section_json("drops"),root_type,source,root_multiplier);
}
std::vector<DropPath> AnalysisEngine::drop_paths_from(const json& d,const std::string& root_type,const std::string& source,double root_multiplier) const{
    json entries;
    if(root_type=="monster")entries=d.value("monsters",json::object()).value(source,json::array());
    else if(root_type=="map")entries=d.value("maps",json::object()).value(source,json::array());
    else if(root_type=="global")entries=d.value("maps",json::object()).value(source,json::array());
    else entries=d.value(source,json::array());
    std::vector<DropPath> out;
    std::function<void(const json&,bool,double,double,const std::string&,int,int)> walk;
    walk=[&](const json& rows,bool normalized,double parent,double rootMult,const std::string& path,int rootIndex,int depth){
        if(depth>12||!rows.is_array())return;
        double total=1;
        if(normalized){
            total=0;
            for(const auto&r:rows)if(r.is_array()&&r.size()>=2&&r[0].is_number())total+=std::max(0.0,r[0].get<double>());
        }
        for(size_t i=0;i<rows.size();++i){
            const auto&r=rows[i];
            if(!r.is_array()||r.size()<2||!r[0].is_number()||!r[1].is_string())continue;
            const double raw=r[0].get<double>();
            double local=normalized?(total>0?raw/total:0):raw*rootMult;
            local=std::clamp(local,0.0,1.0);
            const double prob=parent*local;
            const std::string kind=r[1].get<std::string>();
            const int ri=rootIndex<0?(int)i:rootIndex;
            const std::string p=path+"["+std::to_string(i)+"]";
            if(kind=="open"&&r.size()>=3&&r[2].is_string()){
                const std::string pool=r[2].get<std::string>();
                out.push_back({"pool","",pool,raw,prob,1,ri,p+" -> G.drops."+pool});
                if(d.contains(pool)&&d[pool].is_array())walk(d[pool],true,prob,1.0,p+" -> G.drops."+pool,ri,depth+1);
                else out.push_back({"broken-pool","",pool,raw,prob,1,ri,p});
            }else{
                const double q=(r.size()>=3&&r[2].is_number())?r[2].get<double>():1.0;
                out.push_back({"item",kind,"",raw,prob,q,ri,p});
            }
        }
    };
    const bool normalized=root_type=="table";
    walk(entries,normalized,1.0,root_multiplier,
         root_type=="monster"?"G.drops.monsters."+source:
         root_type=="map"?"G.drops.maps."+source:
         root_type=="global"?"G.drops.maps.global":"G.drops."+source,-1,0);
    return out;
}
DropAggregate AnalysisEngine::aggregate_drop(const std::vector<DropPath>& paths,const std::string& item) const{DropAggregate a;std::unordered_map<int,double> roots;for(auto&r:paths)if(r.kind=="item"&&r.item==item){a.paths++;a.expected_units+=r.probability*r.quantity;roots[r.root_index]=std::min(1.0,roots[r.root_index]+r.probability);}a.roots=(int)roots.size();double miss=1;for(auto&[k,p]:roots)miss*=1-p;a.per_kill=1-miss;return a;}
double AnalysisEngine::probability_after(double p,long long n){if(p<0||p>1||n<0)return 0;return 1-std::pow(1-p,(double)n);}

double AnalysisEngine::damage_multiplier(double defense){auto min=[](double a,double b){return std::min(a,b);};auto max=[](double a,double b){return std::max(a,b);};return min(1.32,max(.05,1-(max(0,min(100,defense))*.001+max(0,min(100,defense-100))*.001+max(0,min(100,defense-200))*.00095+max(0,min(100,defense-300))*.00090+max(0,min(100,defense-400))*.00082+max(0,min(100,defense-500))*.00070+max(0,min(100,defense-600))*.00060+max(0,min(100,defense-700))*.00050+max(0,defense-800)*.00040)+max(0,min(50,0-defense))*.001+max(0,min(50,-50-defense))*.00075+max(0,min(50,-100-defense))*.00050+max(0,-150-defense)*.00025));}

SkillProjection AnalysisEngine::skill_projection(const std::string& skill,double attack,double heal,double attempts,double targets,double target_multiplier) const{auto s=db_.entity_json("skills",skill);SkillProjection o;double cd=num(s,"cooldown",0),maxrate=cd>0?1000.0/cd:std::numeric_limits<double>::infinity();o.cast_rate=std::min(std::max(0.0,attempts),maxrate);double dc=s.contains("damage_multiplier")&&s["damage_multiplier"].is_number()?attack*s["damage_multiplier"].get<double>():num(s,"damage",0);double hc=s.contains("heal_multiplier")&&s["heal_multiplier"].is_number()?heal*s["heal_multiplier"].get<double>():num(s,"heal",0);o.damage_cast=dc*std::max(0.0,target_multiplier);o.heal_cast=hc;o.damage_second=o.damage_cast*o.cast_rate*std::max(1.0,targets);o.heal_second=o.heal_cast*o.cast_rate*std::max(1.0,targets);o.mp_second=num(s,"mp",0)*o.cast_rate;return o;}

std::map<std::string,double> AnalysisEngine::build_stats(const std::map<std::string,std::pair<std::string,int>>& slots,const std::string& class_id,int level) const{
    static const std::unordered_set<std::string> statkeys={"attack","armor","resistance","str","dex","int","vit","for","hp","mp","speed","crit","frequency","range","apiercing","rpiercing","evasion","reflection","lifesteal","manasteal","mp_reduction","xp","gold","output"};std::map<std::string,double> out;
    if(!class_id.empty()){auto c=db_.entity_json("classes",class_id);for(auto s:{"str","dex","int","vit","for"}){double base=c.value("stats",json::object()).value(s,0.0),per=c.value("lstats",json::object()).value(s,0.0);out[s]+=base+std::max(0,level-1)*per;}for(auto&k:statkeys)if(c.contains(k)&&c[k].is_number())out[k]+=c[k].get<double>();}
    std::unordered_map<std::string,int> setcount;
    for(auto&[slot,spec]:slots){auto [id,lvl]=spec;if(id.empty())continue;auto d=db_.entity_json("items",id);if(!d.is_object())continue;for(auto&k:statkeys)if(d.contains(k)&&d[k].is_number())out[k]+=d[k].get<double>();const char* scaling=d.contains("compound")?"compound":"upgrade";if(lvl>0&&d.contains(scaling)&&d[scaling].is_object())for(auto it=d[scaling].begin();it!=d[scaling].end();++it)if(it.value().is_number()&&statkeys.count(it.key()))out[it.key()]+=it.value().get<double>()*lvl;if(d.contains("set")&&d["set"].is_string())setcount[d["set"].get<std::string>()]++;}
    for(auto&[sid,cnt]:setcount){auto s=db_.entity_json("sets",sid);if(!s.is_object())continue;for(auto it=s.begin();it!=s.end();++it){int n=0;try{n=std::stoi(it.key());}catch(...){continue;}if(n<=cnt&&it.value().is_object())for(auto st=it.value().begin();st!=it.value().end();++st)if(st.value().is_number()&&statkeys.count(st.key()))out[st.key()]+=st.value().get<double>();}}
    return out;
}

CombatResult AnalysisEngine::combat(const std::map<std::string,double>& ch,const std::string& dtype,const std::string& monster,const std::string& skill,double crit_bonus) const{
    return combat_with_monster(ch,dtype,db_.entity_json("monsters",monster),skill,crit_bonus);
}
CombatResult AnalysisEngine::combat_with_monster(const std::map<std::string,double>& ch,const std::string& dtype,const json& m,const std::string& skill,double crit_bonus) const{
    CombatResult o;
    auto cv=[&](const char*k,double d=0){auto it=ch.find(k);return it==ch.end()?d:it->second;};
    o.target_defense=(dtype=="magical"?num(m,"resistance")-cv("rpiercing"):dtype=="physical"?num(m,"armor")-cv("apiercing"):0);
    o.out_multiplier=(dtype=="pure"?1:damage_multiplier(o.target_defense));
    const double base=cv("attack")*o.out_multiplier*(cv("output",100)/100);
    const double critFactor=1+(std::max(0.0,cv("crit"))/100)*(std::max(0.0,crit_bonus)/100);
    o.basic_hit=base*critFactor;
    o.basic_dps=o.basic_hit*std::max(0.0,cv("frequency"));
    const std::string incoming=str(m,"damage_type","physical");
    o.incoming_defense=incoming=="magical"?cv("resistance")-num(m,"rpiercing"):incoming=="physical"?cv("armor")-num(m,"apiercing"):0;
    o.in_multiplier=incoming=="pure"?1:damage_multiplier(o.incoming_defense);
    o.incoming_hit=num(m,"attack")*o.in_multiplier*(num(m,"output",100)/100);
    o.incoming_dps=o.incoming_hit*std::max(0.0,num(m,"frequency",1));
    if(!skill.empty()){
        auto s=db_.entity_json("skills",skill);
        const double raw=s.contains("damage_multiplier")&&s["damage_multiplier"].is_number()?cv("attack")*s["damage_multiplier"].get<double>():num(s,"damage",0);
        const std::string st=str(s,"damage_type",dtype);
        const double def=st=="magical"?num(m,"resistance")-cv("rpiercing"):st=="physical"?num(m,"armor")-cv("apiercing"):0;
        const double mult=st=="pure"?1:damage_multiplier(def);
        const double rate=num(s,"cooldown")>0?1000.0/num(s,"cooldown"):0;
        o.skill_dps=raw*mult*rate;
        o.mp_per_second=num(s,"mp")*rate;
    }
    o.total_dps=o.basic_dps+o.skill_dps;
    o.ttk=o.total_dps>0?num(m,"hp")/o.total_dps:std::numeric_limits<double>::infinity();
    o.ttd=o.incoming_dps>0?cv("hp")/o.incoming_dps:std::numeric_limits<double>::infinity();
    o.margin=(std::isfinite(o.ttk)&&o.ttk>0)?o.ttd/o.ttk:std::numeric_limits<double>::infinity();
    return o;
}

json AnalysisEngine::craft_tree(const std::string& item,int max_depth) const{const json craft=db_.section_json("craft");std::unordered_set<std::string> stack;std::function<json(const std::string&,int,double)> walk=[&](const std::string&id,int depth,double q){json node={{"id",id},{"name",item_name(db_,id)},{"quantity",q}};if(depth>=max_depth||!craft.contains(id)||!craft[id].is_object())return node;if(stack.count(id)){node["cycle"]=true;return node;}stack.insert(id);auto r=craft[id];node["cost"]=r.value("cost",0.0)*q;node["ingredients"]=json::array();if(r.contains("items")&&r["items"].is_array())for(auto&x:r["items"])if(x.is_array()&&x.size()>=2&&x[0].is_number()&&x[1].is_string())node["ingredients"].push_back(walk(x[1].get<std::string>(),depth+1,q*x[0].get<double>()));stack.erase(id);return node;};return walk(item,0,1);}

json AnalysisEngine::acquisition(const std::string& item) const{json out={{"item",item},{"drops",json::array()},{"craft",json::array()},{"tokens",json::array()},{"npcs",json::array()},{"dismantle",json::array()}};auto drops=db_.section_json("drops");if(drops.contains("monsters")&&drops["monsters"].is_object())for(auto it=drops["monsters"].begin();it!=drops["monsters"].end();++it){auto paths=drop_paths("monster",it.key());auto a=aggregate_drop(paths,item);if(a.paths)out["drops"].push_back({{"monster",it.key()},{"paths",a.paths},{"per_kill",a.per_kill},{"expected",a.expected_units}});}auto craft=db_.section_json("craft");if(craft.contains(item))out["craft"].push_back(craft[item]);auto tok=db_.section_json("tokens");if(tok.is_object())for(auto it=tok.begin();it!=tok.end();++it)if(it.value().is_object()&&it.value().contains(item))out["tokens"].push_back({{"token",it.key()},{"cost",it.value()[item]}});for(auto&e:db_.all("npcs")){auto j=json::parse(e.raw,nullptr,false);if(j.is_discarded())continue;std::string text=j.dump();if(text.find('"'+item+'"')!=std::string::npos)out["npcs"].push_back({{"id",e.id},{"name",e.name}});}auto dis=db_.section_json("dismantle");if(dis.is_object())for(auto it=dis.begin();it!=dis.end();++it)if(it.value().dump().find('"'+item+'"')!=std::string::npos)out["dismantle"].push_back({{"source",it.key()},{"rule",it.value()}});return out;}

std::vector<std::string> AnalysisEngine::map_route(const std::string& from,const std::string& to) const{if(from==to)return{from};auto maps=db_.all("maps");std::unordered_map<std::string,std::vector<std::string>> adj;for(auto&e:maps){auto m=json::parse(e.raw,nullptr,false);if(m.is_discarded())continue;if(m.contains("doors")&&m["doors"].is_array())for(auto&d:m["doors"])if(d.is_array()&&d.size()>=5&&d[4].is_string())adj[e.id].push_back(d[4].get<std::string>());for(auto k:{"on_death","on_exit"})if(m.contains(k)&&m[k].is_array()&&!m[k].empty()&&m[k][0].is_string())adj[e.id].push_back(m[k][0].get<std::string>());}std::deque<std::string>q{from};std::unordered_map<std::string,std::string>prev;prev[from]="";while(!q.empty()){auto a=q.front();q.pop_front();for(auto&b:adj[a])if(!prev.count(b)){prev[b]=a;if(b==to){std::vector<std::string>p;for(std::string x=to;!x.empty();x=prev[x])p.push_back(x);std::reverse(p.begin(),p.end());return p;}q.push_back(b);}}return{};}

std::vector<IntegrityIssue> AnalysisEngine::integrity_scan() const{std::vector<IntegrityIssue> out;auto issue=[&](std::string sev,std::string code,std::string msg,std::string sec,std::string id,std::string path){out.push_back({std::move(sev),std::move(code),std::move(msg),std::move(sec),std::move(id),std::move(path)});};auto items=db_.section_json("items"),sets=db_.section_json("sets"),skills=db_.section_json("skills"),classes=db_.section_json("classes"),maps=db_.section_json("maps"),npcs=db_.section_json("npcs"),craft=db_.section_json("craft"),tokens=db_.section_json("tokens"),drops=db_.section_json("drops");
    if(craft.is_object())for(auto it=craft.begin();it!=craft.end();++it)if(it.value().contains("items")&&it.value()["items"].is_array())for(auto&x:it.value()["items"])if(x.is_array()&&x.size()>=2&&x[1].is_string()&&!items.contains(x[1].get<std::string>()))issue("error","CRAFT_ITEM_MISSING","Craft recipe references missing item "+x[1].get<std::string>(),"craft",it.key(),"G.craft."+it.key()+".items");
    if(sets.is_object())for(auto it=sets.begin();it!=sets.end();++it){if(it.value().contains("items")&&it.value()["items"].is_array())for(auto&x:it.value()["items"])if(x.is_string()&&!items.contains(x.get<std::string>()))issue("warn","SET_ITEM_MISSING","Set references missing item "+x.get<std::string>(),"sets",it.key(),"G.sets."+it.key()+".items");}
    if(items.is_object())for(auto it=items.begin();it!=items.end();++it){if(it.value().contains("set")&&it.value()["set"].is_string()&&!sets.contains(it.value()["set"].get<std::string>()))issue("warn","ITEM_SET_MISSING","Item references unknown set "+it.value()["set"].get<std::string>(),"items",it.key(),"G.items."+it.key()+".set");if(it.value().contains("class")){json cs=it.value()["class"];if(cs.is_string())cs=json::array({cs});if(cs.is_array())for(auto&c:cs)if(c.is_string()&&!classes.contains(c.get<std::string>()))issue("warn","ITEM_CLASS_MISSING","Item references unknown class "+c.get<std::string>(),"items",it.key(),"G.items."+it.key()+".class");}}
    if(skills.is_object())for(auto it=skills.begin();it!=skills.end();++it){json cs=it.value().value("class",json::array());if(cs.is_string())cs=json::array({cs});if(cs.is_array())for(auto&c:cs)if(c.is_string()&&!classes.contains(c.get<std::string>()))issue("error","SKILL_CLASS_MISSING","Skill references unknown class "+c.get<std::string>(),"skills",it.key(),"G.skills."+it.key()+".class");}
    if(maps.is_object())for(auto it=maps.begin();it!=maps.end();++it){auto&m=it.value();if(m.contains("doors")&&m["doors"].is_array())for(size_t i=0;i<m["doors"].size();++i){auto&d=m["doors"][i];if(d.is_array()&&d.size()>=5&&d[4].is_string()&&!maps.contains(d[4].get<std::string>()))issue("error","MAP_DOOR_TARGET","Door target does not exist: "+d[4].get<std::string>(),"maps",it.key(),"G.maps."+it.key()+".doors["+std::to_string(i)+"]");}if(m.contains("npcs")&&m["npcs"].is_array())for(auto&n:m["npcs"])if(n.is_object()&&n.contains("id")&&n["id"].is_string()&&!npcs.contains(n["id"].get<std::string>()))issue("warn","MAP_NPC_MISSING","Map references unknown NPC "+n["id"].get<std::string>(),"maps",it.key(),"G.maps."+it.key()+".npcs");if(m.contains("monsters")&&m["monsters"].is_array())for(auto&p:m["monsters"])if(p.is_object()&&p.contains("type")&&p["type"].is_string()&&!db_.entity("monsters",p["type"].get<std::string>()))issue("error","MAP_MONSTER_MISSING","Map references unknown monster "+p["type"].get<std::string>(),"maps",it.key(),"G.maps."+it.key()+".monsters");}
    std::function<void(const json&,std::string,std::set<std::string>)> scanDrops;scanDrops=[&](const json& rows,std::string path,std::set<std::string> stack){if(!rows.is_array())return;for(size_t i=0;i<rows.size();++i){auto&r=rows[i];if(!r.is_array()||r.size()<2){issue("error","DROP_SHAPE","Drop entry is not [factor, kind, ...]","drops","",path);continue;}if(r[1].is_string()&&r[1].get<std::string>()=="open"&&r.size()>=3&&r[2].is_string()){auto pool=r[2].get<std::string>();if(!drops.contains(pool))issue("error","DROP_POOL_MISSING","Missing drop pool "+pool,"drops",pool,path);else if(stack.count(pool))issue("error","DROP_CYCLE","Drop pool cycle at "+pool,"drops",pool,path);else{auto n=stack;n.insert(pool);scanDrops(drops[pool],path+"->"+pool,n);}}else if(r[1].is_string()){auto id=r[1].get<std::string>();if(!items.contains(id)&&id!="gold"&&id!="shells"&&id!="empty"&&id!="cxjar")issue("warn","DROP_ITEM_MISSING","Drop references missing item "+id,"drops",id,path);}}};if(drops.contains("monsters")&&drops["monsters"].is_object())for(auto it=drops["monsters"].begin();it!=drops["monsters"].end();++it)scanDrops(it.value(),"G.drops.monsters."+it.key(),{});
    return out;}

std::vector<Entity> AnalysisEngine::advanced_search(const std::string& query,const std::string& section,int limit) const{struct Term{bool neg=false;std::string field,op,value;};std::vector<Term> terms;std::istringstream ss(query);std::string t;std::regex re(R"(^([^:<>!=]+)(>=|<=|!=|>|<|=|:)(.+)$)");while(ss>>t){Term x;if(!t.empty()&&t[0]=='-'){x.neg=true;t.erase(0,1);}std::smatch m;if(std::regex_match(t,m,re)){x.field=m[1];x.op=m[2];x.value=m[3];}else{x.value=t;}terms.push_back(x);}auto rows=db_.all(section,100000);std::vector<Entity> out;for(auto&e:rows){auto j=json::parse(e.raw,nullptr,false);if(j.is_discarded())continue;bool ok=true;std::string flat=lower(e.id+" "+e.name+" "+e.kind+" "+e.description);flatten_text(j,flat);flat=lower(flat);for(auto&x:terms){bool hit=false;if(x.field.empty())hit=flat.find(lower(x.value))!=std::string::npos;else if(x.field=="section")hit=lower(e.section).find(lower(x.value))!=std::string::npos;else if(x.field=="id")hit=lower(e.id).find(lower(x.value))!=std::string::npos;else if(x.field=="name")hit=lower(e.name).find(lower(x.value))!=std::string::npos;else if(x.field=="kind"||x.field=="type")hit=lower(e.kind).find(lower(x.value))!=std::string::npos;else{const json* v=get_path(j,x.field);if(v){if(x.op==":"||x.op=="="||x.op=="!="){if(v->is_string())hit=lower(v->get<std::string>()).find(lower(x.value))!=std::string::npos;else if(v->is_boolean())hit=(lower(x.value)==((*v).get<bool>()?"true":"false"));else if(v->is_number()){try{hit=cmp_num(v->get<double>(),x.op==":"?"=":x.op,std::stod(x.value));}catch(...){}}else hit=lower(v->dump()).find(lower(x.value))!=std::string::npos;if(x.op=="!=")hit=!hit;}else if(v->is_number()){try{hit=cmp_num(v->get<double>(),x.op,std::stod(x.value));}catch(...){}}} }if(x.neg)hit=!hit;if(!hit){ok=false;break;}}if(ok){out.push_back(e);if((int)out.size()>=limit)break;}}return out;}

std::vector<SchemaField> AnalysisEngine::schema(const std::string& section) const{auto rows=db_.all(section,100000);std::map<std::string,std::map<std::string,int>>acc;for(auto&e:rows){auto j=json::parse(e.raw,nullptr,false);if(!j.is_discarded())schema_walk(j,"",acc);}std::vector<SchemaField>out;for(auto&[p,types]:acc){auto best=std::max_element(types.begin(),types.end(),[](auto&a,auto&b){return a.second<b.second;});int total=0;for(auto&[t,n]:types)total+=n;out.push_back({p,best==types.end()?"unknown":best->first,total,rows.empty()?0.0:double(total)/rows.size()});}std::sort(out.begin(),out.end(),[](auto&a,auto&b){return a.path<b.path;});return out;}

std::vector<std::pair<std::string,double>> AnalysisEngine::spawn_route(const std::string& map_id) const{auto m=db_.entity_json("maps",map_id);struct P{std::string label;double x=0,y=0;};std::vector<P>pts;if(m.contains("monsters")&&m["monsters"].is_array())for(size_t i=0;i<m["monsters"].size();++i){auto&p=m["monsters"][i];double x=0,y=0;bool good=false;if(p.contains("position")&&p["position"].is_array()&&p["position"].size()>=2){x=p["position"][0].get<double>();y=p["position"][1].get<double>();good=true;}else if(p.contains("boundary")&&p["boundary"].is_array()&&p["boundary"].size()>=4){x=(p["boundary"][0].get<double>()+p["boundary"][2].get<double>())/2;y=(p["boundary"][1].get<double>()+p["boundary"][3].get<double>())/2;good=true;}if(good)pts.push_back({p.value("type","spawn")+"#"+std::to_string(i),x,y});}std::vector<std::pair<std::string,double>>out;if(pts.empty())return out;std::vector<bool>used(pts.size());size_t cur=0;used[0]=true;out.push_back({pts[0].label,0});for(size_t n=1;n<pts.size();++n){double best=1e100;size_t bi=0;for(size_t i=0;i<pts.size();++i)if(!used[i]){double dx=pts[cur].x-pts[i].x,dy=pts[cur].y-pts[i].y,d=std::hypot(dx,dy);if(d<best){best=d;bi=i;}}used[bi]=true;cur=bi;out.push_back({pts[bi].label,best});}return out;}

json AnalysisEngine::world_summary(const std::string& map_id) const{auto m=db_.entity_json("maps",map_id);json out={{"map",map_id},{"name",m.value("name",map_id)},{"npcs",m.value("npcs",json::array())},{"monsters",m.value("monsters",json::array())},{"doors",m.value("doors",json::array())},{"spawns",m.value("spawns",json::array())}};for(auto k:{"on_death","on_exit"})if(m.contains(k))out[k]=m[k];return out;}
