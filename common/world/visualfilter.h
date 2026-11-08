#pragma once

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

/// Visuals of the static objects of the world that are not drawn: a mod asks for it (world.setHiddenVisuals), for example
/// to see the enemies in the grass better. The list is empty until a running mod fills it, so the game is vanilla without
/// such a mod. A pattern is a part of the name of the visual, the case does not matter ("GRASS" hides "NW_NATURE_GRASS_01.3DS").
/// Everything is in the header, so the file needs no entry in the build system.
class VisualFilter final {
  public:
    static VisualFilter& inst() {
      static VisualFilter f;
      return f;
      }

    /// patterns of all the running mods; changes `revision` when the list is not the same
    void set(std::vector<std::string> patterns) {
      auto next = std::make_shared<std::vector<std::string>>();
      for(auto& p:patterns) {
        std::string u = upper(p);
        if(!u.empty())
          next->push_back(std::move(u));
        }
      std::sort(next->begin(),next->end());
      next->erase(std::unique(next->begin(),next->end()),next->end());

      std::lock_guard<std::mutex> guard(sync);
      const bool same = (list==nullptr && next->empty()) || (list!=nullptr && *list==*next);
      if(same)
        return;
      on.store(!next->empty());
      list = std::move(next);
      rev.fetch_add(1);
      }

    bool     active() const { return on.load(std::memory_order_relaxed); }
    uint32_t revision() const { return rev.load(std::memory_order_relaxed); }

    /// the visual (name of the file) matches one of the patterns. Can be called from the thread that loads the world.
    bool hides(std::string_view visual) const {
      if(!on.load(std::memory_order_relaxed) || visual.empty())
        return false;
      std::shared_ptr<const std::vector<std::string>> l;
      {
      std::lock_guard<std::mutex> guard(sync);
      l = list;
      }
      if(l==nullptr)
        return false;
      const std::string name = upper(visual);
      for(auto& p:*l)
        if(name.find(p)!=std::string::npos)
          return true;
      return false;
      }

  private:
    static std::string upper(std::string_view s) {
      std::string r(s);
      for(auto& c:r)
        c = char(std::toupper(static_cast<unsigned char>(c)));
      return r;
      }

    mutable std::mutex                              sync;
    std::shared_ptr<const std::vector<std::string>> list;
    std::atomic<bool>                               on  {false};
    std::atomic<uint32_t>                           rev {0};
  };
