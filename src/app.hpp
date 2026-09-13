#pragma once
#include "analysis_engine.hpp"
#include "database.hpp"
#include "source_sync.hpp"
#include "icon_cache.hpp"
#include "research_index.hpp"
#include <atomic>
#include <chrono>
#include <future>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

struct ResearchTab {
    std::string id;
    std::string label;
    std::string route;
    std::string section;
    std::string entity_id;
    bool pinned=true;
};

struct PreparedSyncResult {
    bool ok=false;
    std::string error;
    SyncResult sync;
    std::string staged_db;
    std::string archive;
};

struct BuildState {
    std::string class_id="warrior";
    int level=80;
    std::map<std::string,std::pair<std::string,int>> slots;
};

class App {
    Database& db_;
    AnalysisEngine& engine_;
    SourceSync& sync_;
    IconCache icons_;
    ResearchIndex index_;
    std::string data_dir_;
    std::string state_path_;
    std::string route_="dashboard";
    std::string global_search_;
    bool focus_global_search_=false;
    bool command_open_=false;
    bool safe_mode_=false;
    bool onboarding_done_=false;
    std::string selected_section_,selected_id_;
    std::set<std::string> favorites_,watchlist_;
    nlohmann::json source_history_=nlohmann::json::array();
    nlohmann::json last_diff_=nlohmann::json::array();
    nlohmann::json characters_=nlohmann::json::array();
    nlohmann::json workspaces_=nlohmann::json::array();
    nlohmann::json geometry_=nlohmann::json::object();
    nlohmann::json runtime_={{"enabled",false},{"mode","normal"},{"pvp",false},{"events",{{"halloween",false},{"holidayseason",false},{"lunarnewyear",false},{"valentines",false},{"egghunt",false}}}};
    std::vector<std::string> recent_entities_;
    std::vector<ResearchTab> tabs_;
    std::string active_tab_;
    std::set<std::string> compare_selection_;
    BuildState build_a_,build_b_;
    std::unordered_map<std::string,std::string> sel_;
    std::vector<std::string> errors_;
    std::future<PreparedSyncResult> sync_future_;
    std::atomic<int> sync_done_{0},sync_total_{0};
    std::mutex sync_mu_;
    std::string sync_label_;
    std::unordered_map<std::string,std::string> sync_before_;
    bool sync_running_=false;
    double uptime_start_=0;
    double last_search_ms_=0;
    double last_sync_ms_=0;
    double last_frame_ms_=0;
    std::chrono::steady_clock::time_point sync_started_{};

public:
    App(Database& db,AnalysisEngine& engine,SourceSync& sync,std::string data_dir,bool safe_mode=false);
    void frame(bool& request_fullscreen_toggle,bool& request_close);
    void on_dataset_reloaded();
    void shutdown_graphics();
    void prepare_for_smoke_test();
    void set_route_for_test(const std::string& route);
    static std::vector<std::string> smoke_routes();

private:
    void load_state();
    void save_state();
    nlohmann::json serialize_state() const;
    void restore_state(const nlohmann::json& j);
    void push_error(std::string e);
    const std::vector<Entity>& cached(const std::string& section);
    bool entity_combo(const char* label,const std::string& section,std::string& value,bool allow_empty=false);
    void select_entity(const std::string& section,const std::string& id);
    std::string entity_key(const std::string& section,const std::string& id) const;
    void topbar(bool& request_fullscreen_toggle);
    void sidebar();
    void context_panel();
    void tabbar();
    void pin_current_tab(bool pinned=true);
    void activate_tab(const ResearchTab& tab);
    bool execute_command(const std::string& command);
    void command_palette();
    void render_main();
    void route_button(const char* icon,const char* label,const char* route,const char* badge=nullptr);
    void render_dashboard();
    void render_raw();
    void render_browser(const std::string& section);
    void render_favorites();
    void render_changes();
    void render_drop_engine();
    void render_forge();
    void render_craft_graph();
    void render_monster_compare();
    void render_build_designer(BuildState& build,const char* title,bool editable=true);
    void render_economy();
    void render_farming();
    void render_build_compare();
    void render_skill_sim();
    void render_spawn_routes();
    void render_acquisition();
    void render_world();
    void render_atlas();
    void render_graph();
    void render_universal_compare();
    void render_combat();
    void render_integrity();
    void render_sources();
    void render_runtime();
    void render_character_import();
    void render_search_lab();
    void render_workspaces();
    void render_schema();
    void render_snapshots();
    void render_selftest();
    void render_health();
    void start_sync();
    void poll_sync();
    std::unordered_map<std::string,std::string> dataset_signatures();
    nlohmann::json diff_signatures(const std::unordered_map<std::string,std::string>& before,const std::unordered_map<std::string,std::string>& after);
    bool archive_snapshot(const nlohmann::json& snap,std::string& path_out);
    bool restore_snapshot_file(const std::string& path);
    std::unordered_map<std::string,std::string> signatures_for(Database& db);
    bool swap_staged_database(const std::string& staged,std::string& error);
};
