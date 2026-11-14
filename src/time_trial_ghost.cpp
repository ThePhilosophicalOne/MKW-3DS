#include <mkw/time_trial_ghost.hpp>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>

namespace mkw {
namespace {
// Kinoko's RawGhostFile copies RkgSize bytes from an uncompressed ghost, so
// the buffer is at least that long; size is the file's own length.
bool readWhole(FILE* f,std::vector<uint8_t>& out,std::size_t& size){
    bool ok=std::fseek(f,0,SEEK_END)==0;
    const long length=ok?std::ftell(f):-1;
    ok=ok&&length>0&&length<=0x10000&&std::fseek(f,0,SEEK_SET)==0;
    if(ok){
        size=std::size_t(length);
        out.assign(std::max(size,RkgSize),0);
        ok=std::fread(out.data(),1,size,f)==size;
    }
    return ok;
}
bool readFile(const char* path,std::vector<uint8_t>& out,std::size_t& size){
    FILE* f=std::fopen(path,"rb");
    if(!f)return false;
    const bool ok=readWhole(f,out,size);
    std::fclose(f);
    return ok;
}
unsigned license=0;
// The licence's own folder: licence 1 keeps the v0.7.0 names (ghosts/pb-XX.rkg),
// licences 2-4 use ghosts/license-N/, so no licence overwrites another's ghost.
void ownFolder(char* out,std::size_t n){
    if(!license)std::snprintf(out,n,"%s",TimeTrialGhost::Folder);
    else std::snprintf(out,n,"%s/license-%u",TimeTrialGhost::Folder,license+1);
}
void bestPath(char* out,std::size_t n,unsigned course){char folder[96];ownFolder(folder,sizeof folder);std::snprintf(out,n,"%s/pb-%02x.rkg",folder,course);}
// Licences 2-4 do not race licence 1's own ghosts (the pb-XX.rkg names in the
// top folder); every other .rkg there is another player's ghost for everyone.
bool otherLicenseGhost(const char* name){return license&&!strncasecmp(name,"pb-",3);}
// Every RKG in the folder whose header and CRC check out: visit(file,size,header,name).
// visit(file,size,header,own): own = the licence's personal-best file.
template<class Visit> void eachGhost(unsigned course,Visit&& visit){
    std::vector<uint8_t> file;
    char own[16];std::snprintf(own,sizeof own,"pb-%02x.rkg",course);
    if(license){
        char path[128];bestPath(path,sizeof path,course);
        std::size_t size=0;GhostHeader h;
        if(readFile(path,file,size)&&readRkgHeader(file.data(),size,h))visit(file,size,h,true);
    }
    DIR* dir=opendir(TimeTrialGhost::Folder);
    if(!dir)return;
    while(const dirent* e=readdir(dir)){
        const std::size_t length=std::strlen(e->d_name);
        if(length<4||strcasecmp(e->d_name+length-4,".rkg")||otherLicenseGhost(e->d_name))continue;
        char path[300];
        std::snprintf(path,sizeof path,"%s/%s",TimeTrialGhost::Folder,e->d_name);
        std::size_t size=0;GhostHeader h;
        if(readFile(path,file,size)&&readRkgHeader(file.data(),size,h))visit(file,size,h,!license&&!strcasecmp(e->d_name,own));
    }
    closedir(dir);
}
}

bool TimeTrialGhost::load(unsigned course,bool(*usable)(unsigned,unsigned)){
    clear();
    uint32_t best=~0u;
    std::vector<uint8_t> section;
    std::vector<GhostFrame> frames;
    eachGhost(course,[&](std::vector<uint8_t>& file,std::size_t size,const GhostHeader& h,bool own){
        const uint32_t time=rkgMilliseconds(h.finish);
        if(h.course!=course||!usable(h.character,h.vehicle)||!time||time>=best)return;
        if(!rkgInputSection(file.data(),size,section)||!decodeGhostInputs(section.data(),section.size(),frames))return;
        best=time;file_.swap(file);frames_.swap(frames);header_=h;own_=own;
    });
    return ready();
}

void TimeTrialGhost::fastest(std::array<uint32_t,64>& times,bool(*usable)(unsigned,unsigned)){
    times={};
    auto faster=[&](const GhostHeader& h){
        const uint32_t time=rkgMilliseconds(h.finish);
        return h.course<times.size()&&time&&usable(h.character,h.vehicle)&&(!times[h.course]||time<times[h.course]);
    };
    // The header ranks a ghost. Only one that would beat its course's best so
    // far is read whole and checked (CRC-32) as eachGhost() does, so the
    // minimum over the valid ghosts is the same with far less reading.
    std::vector<uint8_t> file;
    auto consider=[&](const char* path){
        FILE* f=std::fopen(path,"rb");
        if(!f)return;
        uint8_t head[0x10];GhostHeader h;std::size_t size=0;
        const bool candidate=std::fread(head,1,sizeof head,f)==sizeof head&&peekRkgHeader(head,sizeof head,h)&&faster(h);
        const bool whole=candidate&&readWhole(f,file,size);
        std::fclose(f);
        if(whole&&readRkgHeader(file.data(),size,h)&&faster(h))times[h.course]=rkgMilliseconds(h.finish);
    };
    char path[300];
    if(license){
        char folder[96];ownFolder(folder,sizeof folder);
        if(DIR* own=opendir(folder)){
            while(const dirent* e=readdir(own)){
                const std::size_t length=std::strlen(e->d_name);
                if(length<4||strcasecmp(e->d_name+length-4,".rkg"))continue;
                std::snprintf(path,sizeof path,"%s/%s",folder,e->d_name);consider(path);
            }
            closedir(own);
        }
    }
    DIR* dir=opendir(Folder);
    if(!dir)return;
    while(const dirent* e=readdir(dir)){
        const std::size_t length=std::strlen(e->d_name);
        if(length<4||strcasecmp(e->d_name+length-4,".rkg")||otherLicenseGhost(e->d_name))continue;
        std::snprintf(path,sizeof path,"%s/%s",Folder,e->d_name);
        consider(path);
    }
    closedir(dir);
}

void TimeTrialGhost::setLicense(unsigned slot){license=slot<4?slot:0;}

uint32_t TimeTrialGhost::savedTime(unsigned course){
    char path[128];bestPath(path,sizeof path,course);
    std::vector<uint8_t> file;std::size_t size=0;GhostHeader h;
    return readFile(path,file,size)&&readRkgHeader(file.data(),size,h)?rkgMilliseconds(h.finish):0;
}

// A temporary file first, so a pulled card keeps the previous ghost.
bool TimeTrialGhost::save(const GhostRecorder& run,const GhostInfo& info){
    std::vector<uint8_t> rkg(RkgSize);
    if(!buildRkg(run,info,rkg.data()))return false;
    mkdir("sdmc:/3ds",0777);mkdir("sdmc:/3ds/mkw-3ds",0777);mkdir(Folder,0777);
    {char folder[96];ownFolder(folder,sizeof folder);mkdir(folder,0777);}
    char path[128],temporary[128];
    bestPath(path,sizeof path,info.course);
    {char folder[96];ownFolder(folder,sizeof folder);std::snprintf(temporary,sizeof temporary,"%s/pb.tmp",folder);}
    FILE* f=std::fopen(temporary,"wb");
    if(!f)return false;
    const bool ok=std::fwrite(rkg.data(),1,rkg.size(),f)==rkg.size();
    if(std::fclose(f)!=0||!ok){std::remove(temporary);return false;}
    std::remove(path);
    return std::rename(temporary,path)==0;
}
}
