#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>

struct Entity {
    std::string section,id,name,kind,description,raw;
};
struct Field { std::string key,value; };

class Database {
    sqlite3* db_=nullptr;
public:
    ~Database();
    bool open(const std::string& path, std::string& error);
    int count(const std::string& section) const;
    int total() const;
    std::vector<Entity> query(const std::string& section,const std::string& search,int limit=3000) const;
    std::vector<Field> fields(const std::string& section,const std::string& id) const;
    std::string meta(const std::string& key) const;
};
