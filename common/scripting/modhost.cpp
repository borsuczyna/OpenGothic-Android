#include "modhost.h"

#include <algorithm>
#include <cstring>

#include "modjs.h"
#include "modutil.h"

namespace fs = std::filesystem;

using namespace Scripting;

ModHost& ModHost::inst() {
  static ModHost host;
  return host;
  }

ModHost::ModHost() {
  rt = JS_NewRuntime();
  JS_SetMemoryLimit(rt,size_t(192)*1024*1024);
  JS_SetMaxStackSize(rt,size_t(1)*1024*1024);
  JS_SetInterruptHandler(rt,interrupt,this);
  JS_SetModuleLoaderFunc(rt,normalizeModule,loadModule,this);
  now = steadyMs();
  }

ModHost::~ModHost() {
  stopAll();
  list.clear();
  if(rt!=nullptr)
    JS_FreeRuntime(rt);
  }

// a script that works too long is stopped
int ModHost::interrupt(JSRuntime*, void* self) {
  auto* h = static_cast<ModHost*>(self);
  return h->deadline!=0 && steadyMs()>h->deadline ? 1 : 0;
  }

// import "./file.mjs" in the files of a mod
char* ModHost::normalizeModule(JSContext* ctx, const char* base, const char* name, void*) {
  std::string joined = name;
  if(joined.rfind("./",0)==0 || joined.rfind("../",0)==0) {
    const auto dir = dirName(base);
    joined = dir.empty() ? joined : dir+"/"+joined;
    }

  std::string path;
  if(!normalizePath(joined,path)) {
    JS_ThrowReferenceError(ctx,"'%s' is outside of the mod",name);
    return nullptr;
    }
  auto* out = static_cast<char*>(js_malloc(ctx,path.size()+1));
  if(out==nullptr)
    return nullptr;
  std::memcpy(out,path.c_str(),path.size()+1);
  return out;
  }

JSModuleDef* ModHost::loadModule(JSContext* ctx, const char* name, void*) {
  auto*       m = static_cast<Mod*>(JS_GetContextOpaque(ctx));
  std::string code;
  if(m==nullptr || m->source==nullptr || !m->source->read(name,code)) {
    JS_ThrowReferenceError(ctx,"cannot find module '%s'",name);
    return nullptr;
    }

  JSValue f = JS_Eval(ctx,code.data(),code.size(),name,JS_EVAL_TYPE_MODULE|JS_EVAL_FLAG_COMPILE_ONLY);
  if(JS_IsException(f))
    return nullptr;
  auto* def = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(f));
  JS_FreeValue(ctx,f);
  return def;
  }

void ModHost::addRoot(const fs::path& dir) {
  for(auto& r:rootDirs) {
    if(r==dir)
      return;
    }
  rootDirs.push_back(dir);
  }

void ModHost::setDataDir(const fs::path& dir) {
  data = dir;
  loadEnabled();
  }

void ModHost::log(LogLine::Level level, std::string_view source, std::string_view text) {
  LogLine l;
  l.serial = nextSerial++;
  l.level  = level;
  l.source = source;
  l.text   = text;
  history.push_back(l);
  while(history.size()>600)
    history.pop_front();
  if(logSink)
    logSink(l);
  }

Mod* ModHost::find(std::string_view id) {
  for(auto& m:list) {
    if(m->manifest.id==id)
      return m.get();
    }
  return nullptr;
  }

size_t ModHost::runningCount() const {
  size_t n = 0;
  for(auto& m:list)
    n += m->isRunning() ? 1 : 0;
  return n;
  }

// --- list of the mods

void ModHost::loadEnabled() {
  enabledState.clear();
  std::string json;
  if(data.empty() || !readFile(data/"mods.json",json))
    return;

  JsonReader r(json);
  for(auto& id:r.strings("enabled"))
    enabledState[id] = true;
  for(auto& id:r.strings("disabled"))
    enabledState[id] = false;
  }

void ModHost::saveEnabled() const {
  if(data.empty())
    return;
  std::string on, off;
  for(auto& p:enabledState) {
    auto& dst = p.second ? on : off;
    if(!dst.empty())
      dst += ",";
    dst += "\""+p.first+"\"";
    }
  writeFile(data/"mods.json","{\"enabled\":["+on+"],\"disabled\":["+off+"]}\n");
  }

void ModHost::scan() {
  std::vector<fs::path> found;
  for(auto& root:rootDirs) {
    std::error_code ec;
    if(!fs::is_directory(root,ec))
      continue;
    for(auto& e:fs::directory_iterator(root,ec)) {
      const auto name = toUtf8(e.path().filename());
      if(name.empty() || name[0]=='.')
        continue;
      found.push_back(e.path());
      }
    }
  std::sort(found.begin(),found.end());

  // mods that are gone, the mods of the program stay
  for(size_t i=0; i<list.size(); ) {
    const bool gone = list[i]->path.string().rfind("builtin:",0)!=0 &&
                      std::find(found.begin(),found.end(),list[i]->path)==found.end();
    if(!gone) {
      ++i;
      continue;
      }
    stop(*list[i]);
    list.erase(list.begin()+std::ptrdiff_t(i));
    }

  for(auto& path:found) {
    bool known = false;
    for(auto& m:list)
      known = known || m->path==path;
    if(known)
      continue;

    auto src = ModSource::open(path);
    if(src==nullptr)
      continue;

    std::string json, error;
    if(!src->read("manifest.json",json)) {
      log(LogLine::Warn,"mods",toUtf8(path.filename())+": manifest.json cannot be read");
      continue;
      }

    Manifest manifest;
    manifest.id = toUtf8(path.stem());
    if(!Manifest::parse(json,manifest,error)) {
      log(LogLine::Warn,"mods",toUtf8(path.filename())+": "+error);
      continue;
      }
    if(auto* other = find(manifest.id)) {
      if(other->path.string().rfind("builtin:",0)==0) {
        // a newer one is in the folder: it replaces the mod of the program
        stop(*other);
        for(size_t i=0; i<list.size(); ++i) {
          if(list[i].get()==other) {
            list.erase(list.begin()+std::ptrdiff_t(i));
            break;
            }
          }
        } else {
        log(LogLine::Warn,"mods",toUtf8(path.filename())+": the id '"+manifest.id+"' is used by another mod");
        continue;
        }
      }

    auto mod = std::make_unique<Mod>();
    mod->manifest = manifest;
    mod->path     = path;
    mod->source   = src;
    auto st       = enabledState.find(manifest.id);
    mod->enabled  = st!=enabledState.end() ? st->second : enabledByDefault;
    list.push_back(std::move(mod));
    }

  // a mod of the program that a file of the folders does not replace
  for(auto& b:builtins) {
    if(find(b.id)!=nullptr)
      continue;   // already known, or replaced by a folder

    std::string json, error;
    Manifest manifest;
    manifest.id = b.id;
    if(!b.source->read("manifest.json",json) || !Manifest::parse(json,manifest,error)) {
      log(LogLine::Warn,"mods","builtin "+b.id+": "+error);
      continue;
      }
    manifest.id = b.id;

    auto mod = std::make_unique<Mod>();
    mod->manifest = manifest;
    mod->path     = fs::path("builtin:"+b.id);
    mod->source   = b.source;
    // a mod of the program is on unless the player has turned it off: it waits quietly until it is used
    auto st       = enabledState.find(manifest.id);
    mod->enabled  = st!=enabledState.end() ? st->second : true;
    list.push_back(std::move(mod));
    }
  }

void ModHost::addBuiltin(std::string_view id, const BuiltinFile* files, size_t count) {
  Builtin b;
  b.id     = std::string(id);
  b.source = ModSource::memory(files,count);
  builtins.push_back(std::move(b));
  }

void ModHost::setEnabled(Mod& mod, bool on) {
  mod.enabled = on;
  enabledState[mod.manifest.id] = on;
  saveEnabled();
  }

void ModHost::startEnabled() {
  for(auto& m:list) {
    if(m->enabled && !m->isRunning())
      start(*m);
    }
  }

void ModHost::stopAll() {
  for(auto& m:list)
    stop(*m);
  }

// --- calls into the scripts

void ModHost::report(Mod& mod, std::string_view what) {
  ++mod.errors;
  std::string msg = js::exception(mod.ctx);
  if(!what.empty())
    msg = std::string(what)+": "+msg;
  log(LogLine::Error,mod.manifest.id,msg);
  }

void ModHost::pumpJobs() {
  if(rt==nullptr)
    return;
  for(int guard=0; guard<10000 && JS_IsJobPending(rt); ++guard) {
    JSContext* c = nullptr;
    const int  r = JS_ExecutePendingJob(rt,&c);
    if(r<0 && c!=nullptr) {
      if(auto* m = static_cast<Mod*>(JS_GetContextOpaque(c)))
        report(*m,"promise");
      }
    if(r==0)
      break;
    }
  }

JSValue ModHost::invoke(Mod& mod, JSValueConst fn, JSValueConst self, int argc, JSValueConst* argv, std::string_view what) {
  Mod* prev = running;
  running   = &mod;
  deadline  = steadyMs()+400;
  JSValue r = JS_Call(mod.ctx,fn,self,argc,argv);
  if(JS_IsException(r))
    report(mod,what);
  deadline  = 0;
  running   = prev;
  pumpJobs();
  return r;
  }

void ModHost::emitTo(Mod& mod, const std::string& name, JSValueConst arg, bool hasArg) {
  if(mod.ctx==nullptr)
    return;
  auto it = mod.events.find(name);
  if(it==mod.events.end() || it->second.empty())
    return;

  // handlers may change the list
  struct Call { uint32_t id; JSValue fn; bool once; };
  std::vector<Call> calls;
  for(auto& h:it->second)
    calls.push_back({h.id,JS_DupValue(mod.ctx,h.fn),h.once});

  for(auto& c:calls) {
    if(c.once) {
      auto cur = mod.events.find(name);
      if(cur!=mod.events.end()) {
        auto& hs = cur->second;
        for(size_t i=0; i<hs.size(); ++i) {
          if(hs[i].id!=c.id)
            continue;
          JS_FreeValue(mod.ctx,hs[i].fn);
          hs.erase(hs.begin()+std::ptrdiff_t(i));
          break;
          }
        }
      }
    if(!mod.isRunning() && name!="shutdown")
      continue;
    JSValue r = invoke(mod,c.fn,JS_UNDEFINED,hasArg ? 1 : 0,const_cast<JSValue*>(&arg),"event '"+name+"'");
    JS_FreeValue(mod.ctx,r);
    }
  for(auto& c:calls)
    JS_FreeValue(mod.ctx,c.fn);
  }

bool ModHost::hasHandlers(std::string_view event) const {
  for(auto& m:list) {
    if(!m->isRunning())
      continue;
    auto it = m->events.find(std::string(event));
    if(it!=m->events.end() && !it->second.empty())
      return true;
    }
  return false;
  }

void ModHost::emit(std::string_view event, const ArgBuilder& builder) {
  const std::string name(event);
  for(auto& m:list) {
    if(!m->isRunning())
      continue;
    auto it = m->events.find(name);
    if(it==m->events.end() || it->second.empty())
      continue;

    if(builder) {
      JSValue arg = builder(m->ctx);
      emitTo(*m,name,arg,true);
      JS_FreeValue(m->ctx,arg);
      } else {
      emitTo(*m,name,JS_UNDEFINED,false);
      }
    }
  }

// --- time

void ModHost::runTimers(Mod& m) {
  for(int guard=0; guard<64 && m.isRunning(); ++guard) {
    // the timer that is due the first
    size_t at = m.timers.size();
    for(size_t i=0; i<m.timers.size(); ++i) {
      if(m.timers[i].due<=now && (at==m.timers.size() || m.timers[i].due<m.timers[at].due))
        at = i;
      }
    if(at==m.timers.size())
      break;

    // the callback may clear its own timer, so it works with the references of its own
    Mod::Timer t = m.timers[at];
    if(t.interval>0) {
      m.timers[at].due = now+t.interval;
      JS_DupValue(m.ctx,t.fn);
      for(auto& a:t.args)
        JS_DupValue(m.ctx,a);
      } else {
      m.timers.erase(m.timers.begin()+std::ptrdiff_t(at));
      }

    JSValue r = invoke(m,t.fn,JS_UNDEFINED,int(t.args.size()),t.args.data(),"timer");
    JS_FreeValue(m.ctx,r);
    JS_FreeValue(m.ctx,t.fn);
    for(auto& a:t.args)
      JS_FreeValue(m.ctx,a);
    }
  }

void ModHost::tick(uint64_t nowMs) {
  now = nowMs;
  for(auto& m:list) {
    if(!m->isRunning())
      continue;
    api::pollNet(*m);
    if(!m->isRunning())
      continue;
    runTimers(*m);
    if(m->isRunning() && m->storageDirty && now>m->storageSaved+30000)
      saveStorage(*m);
    }
  }

// --- storage

void ModHost::loadStorage(Mod& mod) {
  std::string json;
  if(data.empty() || !readFile(data/"storage"/toPath(mod.manifest.id+".json"),json))
    return;

  JSValue v = JS_ParseJSON(mod.ctx,json.data(),json.size(),"storage.json");
  if(JS_IsObject(v)) {
    JS_FreeValue(mod.ctx,mod.storage);
    mod.storage = v;
    return;
    }
  JS_FreeValue(mod.ctx,v);
  JS_FreeValue(mod.ctx,JS_GetException(mod.ctx));
  }

void ModHost::drawTab(Mod& mod, size_t index) {
  if(index>=mod.tabs.size() || !mod.isRunning() || mod.tabs[index].failed)
    return;
  JSValue fn    = JS_DupValue(mod.ctx,mod.tabs[index].fn);
  const auto title = mod.tabs[index].title;
  drawing = true;
  JSValue r = invoke(mod,fn,JS_UNDEFINED,0,nullptr,"tab '"+title+"'");
  drawing = false;
  // an error is told once, not every frame
  if(JS_IsException(r) && index<mod.tabs.size() && mod.tabs[index].title==title)
    mod.tabs[index].failed = true;
  JS_FreeValue(mod.ctx,r);
  JS_FreeValue(mod.ctx,fn);
  }

void ModHost::saveStorage(Mod& mod) {
  mod.storageDirty = false;
  mod.storageSaved = now;
  if(data.empty() || mod.ctx==nullptr || !JS_IsObject(mod.storage))
    return;

  JSValue j = JS_JSONStringify(mod.ctx,mod.storage,JS_UNDEFINED,JS_UNDEFINED);
  if(JS_IsException(j)) {
    report(mod,"storage cannot be saved");
    return;
    }
  if(!JS_IsUndefined(j))
    writeFile(data/"storage"/toPath(mod.manifest.id+".json"),js::str(mod.ctx,j));
  JS_FreeValue(mod.ctx,j);
  }

// --- life of a mod

bool ModHost::fastForwardWanted() const {
  for(const auto& m:list) {
    if(m->isRunning() && m->fastForward)
      return true;
    }
  return false;
  }

CombatOptions ModHost::combatOptions() const {
  for(const auto& m:list) {
    if(m->isRunning() && m->combat.active)
      return m->combat;
    }
  return CombatOptions();
  }

void ModHost::release(Mod& mod) {
  mod.fastForward = false;
  mod.combat      = CombatOptions();
  if(mod.ctx==nullptr)
    return;

  api::releaseNet(mod);
  JSContext* ctx = mod.ctx;
  for(auto& e:mod.events) {
    for(auto& h:e.second)
      JS_FreeValue(ctx,h.fn);
    }
  mod.events.clear();

  for(auto& t:mod.timers) {
    JS_FreeValue(ctx,t.fn);
    for(auto& a:t.args)
      JS_FreeValue(ctx,a);
    }
  mod.timers.clear();

  for(auto& c:mod.commands)
    JS_FreeValue(ctx,c.second.fn);
  mod.commands.clear();

  for(auto& t:mod.tabs)
    JS_FreeValue(ctx,t.fn);
  mod.tabs.clear();

  for(auto& m:mod.modules)
    JS_FreeValue(ctx,m.second);
  mod.modules.clear();

  JS_FreeValue(ctx,mod.storage);
  mod.storage = JS_UNDEFINED;

  JS_FreeContext(ctx);
  mod.ctx = nullptr;
  JS_RunGC(rt);
  }

bool ModHost::loadMain(Mod& mod) {
  std::string code;
  const std::string main = mod.manifest.main;
  if(!mod.source->read(main,code)) {
    mod.error = "cannot read "+main;
    return false;
    }

  Mod* prev = running;
  running   = &mod;
  deadline  = steadyMs()+4000;

  JSValue r = JS_Eval(mod.ctx,code.data(),code.size(),main.c_str(),
                      mod.manifest.module ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL);
  bool ok = true;
  if(JS_IsException(r)) {
    mod.error = js::exception(mod.ctx);
    ok = false;
    } else {
    deadline = steadyMs()+4000;
    pumpJobs();
    // a module is evaluated asynchronously, its failure comes as a rejected promise
    if(JS_IsObject(r) && JS_PromiseState(mod.ctx,r)==JS_PROMISE_REJECTED) {
      JSValue reason = JS_PromiseResult(mod.ctx,r);
      mod.error = js::str(mod.ctx,reason);
      JS_FreeValue(mod.ctx,reason);
      ok = false;
      }
    }
  JS_FreeValue(mod.ctx,r);

  deadline = 0;
  running  = prev;
  return ok;
  }

bool ModHost::start(Mod& mod) {
  if(mod.isRunning())
    return true;
  mod.error.clear();
  mod.errors = 0;

  const auto& games = mod.manifest.game;
  if(!games.empty() && std::find(games.begin(),games.end(),game)==games.end() &&
     std::find(games.begin(),games.end(),std::string("any"))==games.end()) {
    mod.error = "the mod is not made for "+game;
    mod.state = Mod::State::Failed;
    log(LogLine::Warn,mod.manifest.id,mod.error);
    return false;
    }
  if(mod.manifest.apiVersion>ApiVersion) {
    mod.error = "the mod needs a newer api, "+std::to_string(mod.manifest.apiVersion);
    mod.state = Mod::State::Failed;
    log(LogLine::Warn,mod.manifest.id,mod.error);
    return false;
    }

  mod.ctx = JS_NewContext(rt);
  if(mod.ctx==nullptr) {
    mod.error = "no memory";
    mod.state = Mod::State::Failed;
    return false;
    }
  JS_SetContextOpaque(mod.ctx,&mod);

  // every mod has a world of its own
  api::installInfo(mod.ctx,mod,*this);
  api::installConsole(mod.ctx);
  api::installEvents(mod.ctx);
  api::installTimers(mod.ctx);
  api::installStorage(mod.ctx,mod);
  api::installRequire(mod.ctx);
  api::installNet(mod.ctx);
  loadStorage(mod);
  if(binder) {
    Mod* prev = running;
    running   = &mod;
    binder(mod.ctx,mod);
    running   = prev;
    }

  mod.startedAt = now;
  mod.state     = Mod::State::Running;
  if(!loadMain(mod)) {
    log(LogLine::Error,mod.manifest.id,"cannot start: "+mod.error);
    release(mod);
    mod.state = Mod::State::Failed;
    return false;
    }

  log(LogLine::Info,"mods","started "+mod.manifest.name+" "+mod.manifest.version);
  emitTo(mod,"ready",JS_UNDEFINED,false);
  return true;
  }

void ModHost::stop(Mod& mod) {
  if(mod.ctx==nullptr) {
    if(mod.state==Mod::State::Running)
      mod.state = Mod::State::Stopped;
    return;
    }

  if(mod.isRunning())
    emitTo(mod,"shutdown",JS_UNDEFINED,false);
  saveStorage(mod);
  release(mod);
  mod.state = Mod::State::Stopped;
  log(LogLine::Info,"mods","stopped "+mod.manifest.name);
  }

bool ModHost::reset(Mod& mod) {
  stop(mod);

  // the files could have changed: the manifest is read again
  std::string json, error;
  Manifest    manifest = mod.manifest;
  if(mod.source!=nullptr && mod.source->read("manifest.json",json) && Manifest::parse(json,manifest,error) &&
     manifest.id==mod.manifest.id) {
    mod.manifest = manifest;
    }
  return start(mod);
  }
