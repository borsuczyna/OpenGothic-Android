#include "safegpu.h"

#include <Tempest/Log>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <system_error>

#include "commandline.h"

using namespace Tempest;

namespace {

const char* const LockFile  = "safegpu.lock";
const char* const ModeFile  = "safegpu.mode";
const char* const CrashFile = "crash.log";

struct State {
  std::mutex        sync;
  std::atomic<int>  level{SafeGpu::Normal};
  std::atomic<bool> noHiz{false};
  std::string       identity;
  std::FILE*        lock   = nullptr;
  int               armed  = 0;
  bool              lockFailed = false;
  bool              ready  = false;   // init() has read the lock of the previous start: a lock that is made before is not a crash
  };

State& state() {
  static State s;
  return s;
  }

struct Saved {
  bool        valid = false;
  int         level = 0;
  std::string identity;
  std::string shader;
  bool        noHiz = false;
  };

Saved readFile(const char* name) {
  Saved r;
  std::ifstream in(name);
  if(!in)
    return r;
  std::string line;
  while(std::getline(in,line)) {
    while(!line.empty() && (line.back()=='\r' || line.back()=='\n'))
      line.pop_back();
    const auto eq = line.find('=');
    if(eq==std::string::npos)
      continue;
    const std::string key = line.substr(0,eq);
    const std::string val = line.substr(eq+1);
    if(key=="level") {
      try { r.level = std::stoi(val); } catch(...) { r.level = 0; }
      }
    else if(key=="gpu")
      r.identity = val;
    else if(key=="shader")
      r.shader = val;
    else if(key=="nohiz")
      r.noHiz = (val=="1");
    }
  r.valid = !r.identity.empty();
  return r;
  }

void removeFile(const char* name) {
  std::error_code ec;
  std::filesystem::remove(name,ec);
  }

void writeMode(const std::string& identity, int level, const std::string& shader, bool noHiz) {
  std::ofstream out(ModeFile,std::ios::trunc);
  if(!out) {
    Log::e("Safe GPU mode: unable to write ",ModeFile," (it will be forgotten after the next start)");
    return;
    }
  out << "level=" << level << "\n";
  out << "gpu="   << identity << "\n";
  out << "shader=" << shader << "\n";
  out << "nohiz=" << (noHiz ? 1 : 0) << "\n";
  }

/// The crash.log of a death in a command of the driver (not in its compiler, so the lock knows nothing): the stack names the build of HiZ.
/// The log tells the GPU of the crash ("GPU: Adreno (TM) 618"), another GPU is not the reason to leave HiZ.
bool crashInHiZ(const std::string& identity) {
  std::ifstream in(CrashFile);
  if(!in)
    return false;
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if(text.find("buildHiZ")==std::string::npos)
    return false;
  const auto gpu = text.find("GPU: ");
  if(gpu!=std::string::npos) {
    const auto end  = text.find_first_of("\r\n",gpu);
    const std::string name = text.substr(gpu+5,end==std::string::npos ? std::string::npos : end-gpu-5);
    if(name!=identity)
      return false;
    }
  return true;
  }

/// the crash.log that is newer than the lock: the crash handler has seen the death (not a kill of the application by the user)
bool crashAfterLock() {
  std::error_code ec;
  const auto lockTime  = std::filesystem::last_write_time(LockFile,ec);
  if(ec)
    return false;
  const auto crashTime = std::filesystem::last_write_time(CrashFile,ec);
  if(ec)
    return false;
  return crashTime>=lockTime;
  }

// the lock keeps the name of the shader; the whole file is rewritten (it has to stay small, one write of the system)
void writeLock(State& s, std::string_view shader) {
  if(s.lock==nullptr)
    return;
  std::rewind(s.lock);
  // a record of a fixed size, the rest of it is empty lines: a shorter name does not leave the tail of the previous one
  char buf[512];
  std::fill(std::begin(buf),std::end(buf),'\n');
  std::snprintf(buf,sizeof(buf)-1,"gpu=%s\nshader=%.*s",s.identity.c_str(),int(std::min<size_t>(shader.size(),160)),shader.data());
  buf[std::strlen(buf)] = '\n';
  std::fwrite(buf,1,sizeof(buf),s.lock);
  std::fflush(s.lock);
  }

}

void SafeGpu::init(std::string_view gpuName) {
  auto& s = state();
  std::lock_guard<std::mutex> guard(s.sync);

  s.identity = std::string(gpuName);
  std::replace(s.identity.begin(),s.identity.end(),'\n',' ');

  const int forced = CommandLine::inst().safeGpuMode();
  if(forced==0) {
    removeFile(ModeFile);
    removeFile(LockFile);
    s.level = Normal;
    Log::i("Safe GPU mode: off (command line)");
    return;
    }

  int         level  = Normal;
  bool        noHiz  = false;
  std::string shader;
  const Saved mode = readFile(ModeFile);
  if(mode.valid) {
    if(mode.identity==s.identity) {
      level  = std::clamp(mode.level,int(Normal),int(Minimal));
      noHiz  = mode.noHiz;
      shader = mode.shader;
      } else {
      Log::i("Safe GPU mode: the saved state is of another GPU (",mode.identity.c_str(),"), ignored");
      removeFile(ModeFile);
      }
    }

  const Saved lock = readFile(LockFile);
  if(lock.valid) {
    if(lock.identity==s.identity) {
      // the previous start never finished the compilation of the shaders
      const bool crashed = crashAfterLock();
      Log::e("Safe GPU mode: the previous start died while the driver was compiling the shader '",lock.shader.c_str(),"'",
             crashed ? " (crash.log confirms)" : "");
      if(level==Normal)
        level = Safe; else
      if(level==Safe && crashed)
        level = Minimal;
      shader = lock.shader;
      writeMode(s.identity,level,shader,noHiz);
      } else {
      Log::i("Safe GPU mode: the lock is of another GPU (",lock.identity.c_str(),"), ignored");
      }
    removeFile(LockFile);
    }

  if(!noHiz && crashInHiZ(s.identity)) {
    noHiz = true;
    Log::e("Safe GPU mode: the previous start died in the build of HiZ (crash.log), HiZ is off for ",s.identity.c_str());
    writeMode(s.identity,level,shader,noHiz);
    }

  if(forced==1)
    level = std::max(level,int(Safe));
  else if(forced==2)
    level = Minimal;

  s.level = level;
  s.noHiz = noHiz;
  s.ready = true;
  if(level!=Normal)
    Log::e("Safe GPU mode: level ",level," for ",s.identity.c_str(),
           " (-safegpu off turns it off, or delete ",ModeFile,")");
  }

SafeGpu::Level SafeGpu::level() {
  return Level(state().level.load());
  }

bool SafeGpu::noHiZ() {
  return state().noHiz.load();
  }

void SafeGpu::trace(std::string_view shader) {
  auto& s = state();
  std::lock_guard<std::mutex> guard(s.sync);
  writeLock(s,shader);
  }

namespace {
void arm(State& s, std::string_view shader) {
  if(s.armed++>0)
    return;
  s.lock = std::fopen(LockFile,"wb");
  if(s.lock==nullptr && !s.lockFailed) {
    s.lockFailed = true;
    Log::e("Safe GPU mode: unable to create ",LockFile,", a crash of the driver will not be remembered");
    }
  writeLock(s,shader);
  }

void disarm(State& s) {
  if(s.armed==0 || --s.armed>0)
    return;
  if(s.lock!=nullptr) {
    std::fclose(s.lock);
    s.lock = nullptr;
    removeFile(LockFile);
    }
  }
}

SafeGpu::Scope::Scope() {
  auto& s = state();
  std::lock_guard<std::mutex> guard(s.sync);
  arm(s,"(start)");
  }

SafeGpu::Scope::~Scope() {
  auto& s = state();
  std::lock_guard<std::mutex> guard(s.sync);
  disarm(s);
  }

namespace {
thread_local bool driverArmed = false;  // this thread has begun a compilation of Tempest and has not finished it
}

void SafeGpu::driverCompile(std::string_view shaders) {
  auto& s = state();
  std::lock_guard<std::mutex> guard(s.sync);
  if(!s.ready || driverArmed)
    return;
  driverArmed = true;
  arm(s,shaders);
  writeLock(s,shaders); // the name of this pipeline, if the lock was already there for another
  }

void SafeGpu::driverDone() {
  auto& s = state();
  std::lock_guard<std::mutex> guard(s.sync);
  if(!driverArmed)
    return;
  driverArmed = false;
  disarm(s);
  }

bool SafeGpu::onLog(std::string_view line) {
  static const std::string_view tag = "vkCreateGraphicsPipelines: ";
  if(line.substr(0,tag.size())!=tag)
    return false;
  line.remove_prefix(tag.size());
  if(line=="done")
    driverDone(); else
    driverCompile(line);
  return true;
  }
