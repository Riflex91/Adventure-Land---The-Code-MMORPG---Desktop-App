#include "app.hpp"
#include "database.hpp"
#include "platform.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

int main(int argc,char** argv){
    try{
        std::filesystem::path exe=std::filesystem::absolute(argv[0]).parent_path();
        std::filesystem::path dbpath=exe/"resources"/"reference.db";
        if(argc>1)dbpath=argv[1];
        Database db;std::string err;if(!db.open(dbpath.string(),err)){std::cerr<<"Database error: "<<err<<"\nExpected: "<<dbpath<<"\n";return 2;}
        auto platform=create_platform(1500,900,"Adventure Land Reference OS Desktop");
        if(!platform){std::cerr<<"Could not create native window\n";return 3;}
        App app(*platform,db);InputState in;
        while(platform->poll(in)){app.frame(in);std::this_thread::sleep_for(std::chrono::milliseconds(8));}
        return 0;
    }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
}
