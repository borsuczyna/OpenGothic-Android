#include "gamemenu.h"

#include <algorithm>
#include <cstdio>

#include "ui/menuroot.h"
#include "ui/touchdraw.h"
#include "utils/string_frm.h"
#include "gothic.h"
#include "resources.h"

#ifdef OPENGOTHIC_TOUCH_UI

#include "build.h"

using namespace Tempest;
using namespace touchdraw;

namespace {
const Color ColItem (0.93f,0.87f,0.70f,1.00f);
const Color ColItemHi(1.00f,0.97f,0.88f,1.00f);
const Color ColItemOff(0.55f,0.57f,0.58f,1.00f);
}

/// A menu where every row is a button: the main menu. The rows with a value (sliders, choices) are the options.
bool GameMenu::isButtonList() const {
  if(!touch.list || embedded)
    return false;
  const auto rows = touchRows();
  if(rows.empty())
    return false;
  for(auto r:rows) {
    const auto& h = hItems[touchControl(r)].handle;
    if(h->type==zenkit::MenuItemType::SLIDER || h->type==zenkit::MenuItemType::CHOICEBOX ||
       (h->type==zenkit::MenuItemType::INPUT && h->on_chg_set_option_section=="KEYS"))
      return false;
    }
  return true;
  }

GameMenu::MainRows GameMenu::mainRows() const {
  MainRows m;
  m.rows = touchRows();

  const float W = float(w());
  const float H = float(h());
  m.rowH = std::max(1,int(H*0.0616f));
  m.cx   = int(W*0.178f);

  // the buttons are in groups: a bigger step in the script is a divider
  std::vector<int> ys;
  for(auto r:m.rows)
    ys.push_back(hItems[r].handle->pos_y);
  int pitch = 0;
  for(size_t i=1; i<ys.size(); ++i) {
    const int d = ys[i]-ys[i-1];
    if(d>0 && (pitch==0 || d<pitch))
      pitch = d;
    }
  const int gap = int(H*0.016f);
  int       top = 0;
  for(size_t i=0; i<m.rows.size(); ++i) {
    m.top.push_back(top);
    const bool end = i+1<m.rows.size() && pitch>0 && float(ys[i+1]-ys[i])>float(pitch)*1.35f;
    m.groupEnd.push_back(end);
    top += m.rowH+(end ? gap : 0);
    }

  // under the logo and the title of the game
  float titleBottom = 0.365f*H-float(m.rowH)/2;
  {
    int            k = 0;
    Tempest::Rect  first;       // the place of the first image in the script
    Tempest::Rect  firstDraw;   // and where it is drawn
    for(const auto& it:hItems) {
      if(!it.handle || !it.visible || !it.img || it.handle->type!=zenkit::MenuItemType::TEXT ||
         isSelectable(it.handle) || isHidden(it.handle) || it.img->w()<256 || it.img->h()<=0)
        continue;
      if(float(it.img->w())/float(it.img->h())<2.5f || k>=2)
        continue;
      const int iw = int(W*0.30f);
      const int ih = iw*it.img->h()/it.img->w();
      const auto orig = itemRect(it);

      // Two pictures that lie over each other in the script (the logo and its shadow) are drawn over each other,
      // keeping the shift between them. Pictures that stand apart (the logo and the title) go one under another.
      bool overlay = false;
      if(k==1 && first.w>0 && first.h>0) {
        const int ox = std::max(0,std::min(first.x+first.w,orig.x+orig.w)-std::max(first.x,orig.x));
        const int oy = std::max(0,std::min(first.y+first.h,orig.y+orig.h)-std::max(first.y,orig.y));
        const int64_t inter = int64_t(ox)*int64_t(oy);
        const int64_t small = std::min(int64_t(first.w)*int64_t(first.h),int64_t(orig.w)*int64_t(orig.h));
        overlay = small>0 && inter*2>=small;
        }

      Tempest::Rect draw;
      if(overlay) {
        const float k0 = float(firstDraw.w)/float(std::max(1,first.w));
        const int   dx = std::clamp(int(float(orig.x-first.x)*k0),-iw/12,iw/12);
        const int   dy = std::clamp(int(float(orig.y-first.y)*k0),-ih/4, ih/4);
        draw = Tempest::Rect(firstDraw.x+dx,firstDraw.y+dy,iw,ih);
        } else {
        const int cy = int(H*0.173f)+int(float(ih)*0.85f*float(k));
        draw = Tempest::Rect(m.cx-iw/2,cy-ih/2,iw,ih);
        titleBottom = std::max(titleBottom,float(cy)+H*0.095f-float(m.rowH)/2);
        }
      if(k==0) {
        first     = orig;
        firstDraw = draw;
        }
      m.images.push_back(draw);
      m.imgTex.push_back(it.img);
      ++k;
      }
  }
  m.y0 = int(titleBottom);

  const float avail = H*0.96f-float(m.y0);
  m.visible = 0;
  for(size_t i=0; i<m.rows.size(); ++i) {
    if(float(m.top[i]+m.rowH)>avail)
      break;
    ++m.visible;
    }
  m.visible = std::max(1,m.visible);
  return m;
  }

int GameMenu::visibleRows() const {
  if(embedded)
    return panelGeom().visible;
  if(isButtonList())
    return mainRows().visible;
  return 9;   // TouchRows of gamemenu.cpp
  }

void GameMenu::paintMainList(Painter& p) {
  const int W = w();
  const int H = h();
  const auto m = mainRows();
  touch.offset = std::clamp(touch.offset,0,std::max(0,int(m.rows.size())-m.visible));

  // the left side is darker, the picture of the game is on the right
  bar(p,0,0,W,H,Color(0.f,0.f,0.f,0.20f));
  {
    const int strips = 48;
    const int width  = int(float(W)*0.48f);
    for(int k=0; k<strips; ++k) {
      const float t = 1.f-float(k)/float(strips);
      bar(p,k*width/strips,0,width/strips+1,H,Color(0.f,0.01f,0.02f,0.62f*t*t));
      }
  }

  // the logo and the title come from the menu of the game
  if(m.images.empty()) {
    auto fHead = fontOf(true,int(float(H)*0.075f),Cream);
    const std::string heading = Gothic::inst().version().game==2 ? "GOTHIC II" : "GOTHIC";
    fHead.drawText(p,m.cx-fHead.textSize(heading).w/2,int(float(H)*0.173f)+fHead.pixelSize()/2,heading);
    }
  for(size_t i=0; i<m.images.size(); ++i) {
    const auto& r = m.images[i];
    p.setBrush(Brush(*m.imgTex[i]));
    p.drawRect(r.x,r.y,r.w,r.h,0,0,m.imgTex[i]->w(),m.imgTex[i]->h());
    }

  auto fItem = fontOf(true,int(float(H)*0.052f),ColItem);
  auto fHi   = fontOf(true,int(float(H)*0.052f),ColItemHi);
  auto fOff  = fontOf(true,int(float(H)*0.052f),ColItemOff);

  const int base = m.top.empty() ? 0 : m.top[size_t(touch.offset)];
  for(int r=0; r<m.visible && size_t(touch.offset+r)<m.rows.size(); ++r) {
    const size_t   i       = size_t(touch.offset+r);
    const uint32_t index   = m.rows[i];
    auto&          item    = hItems[index];
    const bool     enabled = isEnabled(item.handle);
    const bool     chosen  = &item==selectedItem() && enabled;
    const int      top     = m.y0+m.top[i]-base;
    const int      yc      = top+m.rowH/2;

    if(chosen) {
      // a dot on the left of the chosen one
      const int bw = int(float(W)*0.206f);
      const int x0 = m.cx-bw/2-int(float(W)*0.01f);
      disc(p,float(x0)+float(W)*0.004f,float(yc),std::max(1.5f,float(H)*0.0035f),Gold);
      }

    auto& f = !enabled ? fOff : (chosen ? fHi : fItem);
    const std::string label = touchRowLabel(index,touchControl(index));
    f.drawText(p,m.cx-f.textSize(label).w/2,yc+f.pixelSize()/2,label);

    if(m.groupEnd[i] && r+1<m.visible)
      bar(p,m.cx-int(float(W)*0.085f),top+m.rowH+int(float(H)*0.008f),int(float(W)*0.17f),1,Hairline);
    }

  if(m.rows.size()>size_t(m.visible)) {
    auto small = fontOf(false,int(float(H)*0.028f),Grey);
    const int last = std::min(touch.offset+m.visible,int(m.rows.size()));
    string_frm page(touch.offset+1," - ",last," / ",int(m.rows.size()));
    small.drawText(p,int(float(W)*0.03f),int(float(H)*0.975f),page);
    }
  if(owner.hasVersionLine()) {
    auto small = fontOf(false,int(float(H)*0.026f),Grey);
    small.drawText(p,m.cx-small.textSize(appBuild).w/2,int(float(H)*0.975f),appBuild);
    }
  }

#endif
