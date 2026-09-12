#include "analysis_engine.hpp"
#include "database.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

int main(int argc,char**argv){
  if(argc<2) return 2;
  Database db; std::string e;
  if(!db.open(argv[1],e,true)){ std::cerr<<e<<"\n"; return 3; }
  if(db.total()<100 || db.count("items")<40 || db.count("monsters")<30 || db.count("skills")<40 || db.count("classes")<7 || db.count("maps")<5) return 4;
  auto q=db.query("items","HP Potion");
  auto hit=std::find_if(q.begin(),q.end(),[](const Entity& x){return x.id=="hpot0";});
  if(hit==q.end()) return 5;
  if(std::abs(AnalysisEngine::damage_multiplier(100)-0.9)>1e-9) return 6;
  AnalysisEngine engine(db);
  auto weapons=engine.advanced_search("section:items type:weapon","",20);
  if(weapons.empty()) return 7;
  auto drops=db.section_json("drops");
  if(!drops.is_object() || !drops.contains("monsters")) return 8;
  auto schema=engine.schema("items");
  if(schema.empty()) return 9;
  std::string check;
  if(!db.integrity_check(check)) return 10;
  auto snap=db.export_snapshot();
  if(!snap.contains("data") || !snap["data"].contains("items")) return 11;
  std::cout<<"PASS total="<<db.total()<<" items="<<db.count("items")<<" monsters="<<db.count("monsters")<<" skills="<<db.count("skills")<<" fts=hpot0 dsl="<<weapons.size()<<" schema="<<schema.size()<<"\n";
  return 0;
}
