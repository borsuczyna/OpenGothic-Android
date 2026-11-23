#include "rainfx.h"

#include <zenkit/addon/daedalus.hh>
#include <Tempest/Log>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

#include "graphics/pfx/particlefx.h"
#include "utils/androidinput.h"
#include "world/world.h"
#include "camera.h"
#include "gothic.h"

using namespace Tempest;

static uint32_t mix(uint32_t x) {
  x ^= x>>16;
  x *= 0x7feb352dU;
  x ^= x>>15;
  x *= 0x846ca68bU;
  x ^= x>>16;
  return x;
  }

/// The particle effect of the drops. It is not in the scripts (the original draws the rain by the engine), so it is built here:
/// the streaks of SKYRAIN.TGA fall from the box over the camera, stretched along the velocity.
static std::unique_ptr<ParticleFx> makeDropFx() {
  zenkit::IParticleEffect src;
  src.pps_value             = 800;
  src.pps_is_looping        = 1;
  src.pps_fps               = 1;

  src.shp_type_s            = "BOX";
  src.shp_for_s             = "WORLD";
  src.shp_offset_vec_s      = "0 800 0";
  src.shp_is_volume         = 1;
  src.shp_dim_s             = "900 0 900";

  src.dir_mode_s            = "DIR";
  src.dir_for_s             = "WORLD";
  src.dir_angle_head        = 30;
  src.dir_angle_head_var    = 15;
  src.dir_angle_elev        = -84;
  src.dir_angle_elev_var    = 1.5f;

  src.vel_avg               = 0.95f;
  src.vel_var               = 0.12f;
  src.lsp_part_avg          = 1000;
  src.lsp_part_var          = 100;
  src.fly_gravity_s         = "0 0 0";

  src.vis_name_s            = "SKYRAIN.TGA";
  src.vis_orientation_s     = "VELO3D";
  src.vis_tex_is_quadpoly   = 1;
  src.vis_tex_color_start_s = "205 215 235";
  src.vis_tex_color_end_s   = "205 215 235";
  src.vis_size_start_s      = "9 100";
  src.vis_size_end_scale    = 1;
  src.vis_alpha_func_s      = "BLEND";
  src.vis_alpha_start       = 190;
  src.vis_alpha_end         = 90;
  src.use_emitters_for      = 0;

  return std::unique_ptr<ParticleFx>(new ParticleFx(src,"RAIN_DROP"));
  }

RainFx::RainFx(World& owner)
  :owner(owner) {
  decl = makeDropFx();
  settingsTimer = 1000; // read the settings at the first tick
  }

RainFx::~RainFx() {
  }

/// Only the outdoor worlds of the original games have the rain
bool RainFx::isOutdoorWorld(std::string_view name) {
  std::string up(name);
  for(auto& c:up)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  return up.rfind("NEWWORLD",0)==0 || up.rfind("OLDWORLD",0)==0 || up.rfind("ADDONWORLD",0)==0 || up=="WORLD.ZEN" || up=="WORLD";
  }

/// The weather of the day: some days have a rain of 40 to 240 minutes, with the fade of 15 minutes at both ends. The day tells
/// everything, so the weather is the same after a save and a load, and the first day is dry.
RainFx::Plan RainFx::planOf(int64_t day) {
  Plan ret;
  if(day<1)
    return ret;

  const uint32_t h = mix(uint32_t(day)*2654435761u + 0x9e3779b9u);
  if(h%100>=35)
    return ret;

  ret.rain  = true;
  ret.start = 5*60 + int((h>>8)%(15*60));   // from 5:00 to 20:00
  ret.len   = 40 + int((h>>16)%200);        // from 40 to 240 minutes
  ret.peak  = ((h>>24)%3==0) ? 1.f : 0.65f;
  return ret;
  }

float RainFx::scheduled() const {
  if(forced>=0)
    return forced>0 ? 1.f : 0.f;

  const gtime t    = owner.time();
  const Plan  plan = planOf(t.day());
  if(!plan.rain)
    return 0;

  const int now = int(t.hour())*60 + int(t.minute());
  if(now<plan.start || now>=plan.start+plan.len)
    return 0;

  const float fade = 15.f;
  const float k    = std::min(1.f, std::min(float(now-plan.start), float(plan.start+plan.len-now))/fade);
  return k*plan.peak;
  }

void RainFx::updateShelter(const Vec3& pos) {
  bool roof = false;
  if(auto* dyn = owner.physic())
    roof = dyn->ray(pos+Vec3(0,30,0), pos+Vec3(0,1800,0)).hasCol;
  const bool room = !owner.roomAt(pos).empty();
  covered = roof || room;
  }

void RainFx::updateSound(const Vec3& pos, float volume) {
  if(noise.isEmpty()) {
    if(volume<0.02f)
      return;
    noise = ::Sound(owner,::Sound::T_Regular,"RAIN_01.WAV",pos,100000.f,false);
    noise.setAmbient(true);
    noise.setLooping(true);
    noise.play();
    }
  noise.setPosition(pos);
  noise.setVolume(volume);
  }

void RainFx::tick(uint64_t dt) {
  auto* cam = Gothic::inst().camera();
  if(cam==nullptr)
    return;
  const Vec3 pos = cam->originLwc();

  settingsTimer += dt;
  if(settingsTimer>=1000) {
    settingsTimer = 0;
    level   = std::clamp(AndroidInput::preferences().rain,0,3);
    outdoor = isOutdoorWorld(owner.name());
    if(level!=loggedLevel || outdoor!=loggedOutdoor) {
      loggedLevel   = level;
      loggedOutdoor = outdoor;
      Log::d("[rain] world ",owner.name(),", the setting ",level," (0 off .. 3 heavy), outdoor world ",outdoor ? "yes" : "no");
      }
    }

  // the plan of the day: when it rains is in the log
  const int64_t day = owner.time().day();
  if(day!=loggedDay) {
    loggedDay = day;
    const Plan plan = planOf(day);
    if(plan.rain)
      Log::d("[rain] day ",day,": the rain from ",plan.start/60,":",(plan.start%60)/10,plan.start%10," for ",plan.len," minutes (strength ",plan.peak,")");
    else
      Log::d("[rain] day ",day,": dry");
    }

  shelterTimer += dt;
  if(shelterTimer>=250) {
    shelterTimer = 0;
    updateShelter(pos);
    }

  // the weather comes and goes slowly
  const float target = (level>0 && outdoor) ? scheduled() : 0.f;
  const float step   = float(dt)/8000.f;
  weather += std::clamp(target-weather,-step,step);

  const float cover = std::min(1.f, float(dt)/500.f);
  shelter += ((covered ? 1.f : 0.f)-shelter)*cover;

  // what the player sees and hears: the weather without the roofs
  const float visible = weather*(1.f-shelter);
  const float density = visible*float(level)/3.f*float(LayerCount);
  const size_t count  = size_t(std::clamp(int(std::floor(density+0.5f)),0,int(LayerCount)));

  if(count!=loggedCount || covered!=loggedCovered) {
    loggedCount   = count;
    loggedCovered = covered;
    Log::d("[rain] layers ",count," of ",LayerCount,", the weather ",weather," (target ",target,"), the shelter ",shelter,
           covered ? " (a roof or a room is over the camera)" : "",", at ",pos.x," ",pos.y," ",pos.z);
    }

  for(size_t i=0; i<LayerCount; ++i) {
    if(layer[i].isEmpty()) {
      if(i>=count)
        continue;
      layer[i] = PfxEmitter(owner,decl.get());
      if(layer[i].isEmpty()) {
        static bool once = false;
        if(!once) {
          once = true;
          Log::e("[rain] the emitter of the drops is empty: the texture SKYRAIN.TGA is not loaded");
          }
        continue;
        }
      layer[i].setLooped(true);
      }
    layer[i].setPosition(pos);
    layer[i].setActive(i<count);
    }

  updateSound(pos, std::min(1.f, visible*float(level)/2.f)*0.6f);
  }
