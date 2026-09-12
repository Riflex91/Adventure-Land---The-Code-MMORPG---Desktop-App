#include "database.hpp"
#include <algorithm>
#include <iostream>
int main(int argc,char**argv){
  if(argc<2) return 2;
  Database db; std::string e;
  if(!db.open(argv[1],e)){ std::cerr<<e<<"\n"; return 3; }
  if(db.total()<100 || db.count("items")<40 || db.count("monsters")<30 || db.count("skills")<40 || db.count("classes")<7) return 4;
  auto q=db.query("items","HP Potion");
  auto hit=std::find_if(q.begin(),q.end(),[](const Entity& x){return x.id=="hpot0";});
  if(hit==q.end()) return 5;
  auto q2=db.query("monsters","Goo");
  if(q2.empty()) return 6;
  std::cout<<"PASS total="<<db.total()<<" item_search=hpot0 monster_search="<<q2[0].id<<"\n";
  return 0;
}
