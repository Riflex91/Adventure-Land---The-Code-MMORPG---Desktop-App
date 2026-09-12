#pragma once
#include <nlohmann/json.hpp>
#include <sqlite3.h>
#include <optional>
#include <string>
#include <vector>

struct Entity {
    std::string section;
    std::string id;
    std::string name;
    std::string kind;
    std::string description;
    std::string raw;
};

struct Field { std::string key, value; };

class Database {
    sqlite3* db_ = nullptr;
    std::string path_;
public:
    ~Database();
    Database() = default;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool open(const std::string& path, std::string& error, bool read_only=false);
    void close();
    bool valid() const { return db_ != nullptr; }
    const std::string& path() const { return path_; }

    int count(const std::string& section) const;
    int total() const;
    std::vector<std::string> sections() const;
    std::vector<Entity> query(const std::string& section, const std::string& search, int limit=5000) const;
    std::vector<Entity> all(const std::string& section, int limit=100000) const { return query(section, "", limit); }
    std::optional<Entity> entity(const std::string& section, const std::string& id) const;
    nlohmann::json entity_json(const std::string& section, const std::string& id) const;
    nlohmann::json section_json(const std::string& section) const;
    std::vector<Field> fields(const std::string& section, const std::string& id) const;
    std::string meta(const std::string& key) const;
    bool set_meta(const std::string& key, const std::string& value, std::string* error=nullptr);

    bool replace_dataset(const nlohmann::json& snapshot, std::string& error);
    nlohmann::json export_snapshot() const;
    bool integrity_check(std::string& error) const;

private:
    bool exec(const char* sql, std::string* error=nullptr) const;
};
