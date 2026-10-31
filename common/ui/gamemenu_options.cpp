#include "gamemenu.h"

#include <algorithm>
#include <cstdio>

#include "ui/menuroot.h"
#include "ui/touchdraw.h"
#include "utils/keycodec.h"
#include "gothic.h"
#include "resources.h"

#ifdef OPENGOTHIC_TOUCH_UI

using namespace Tempest;
using namespace touchdraw;

namespace {

const Color Inactive(0.78f,0.82f,0.86f,1.00f);
const Color Track   (0.26f,0.31f,0.36f,0.95f);
const Color Knob    (0.98f,0.92f,0.76f,1.00f);

/// the place of a category in the list on the left
Rect categoryRect(const Rect& s, int i) {
  const int x = s.x+int(float(s.w)*0.065f);
  const int w = int(float(s.w)*0.872f);
  const int y = s.y+int(float(s.h)*0.218f)+i*int(float(s.h)*0.116f);
  return Rect(x,y,w,int(float(s.h)*0.109f));
  }

}

Rect GameMenu::optionsSidebar() const {
  return Rect(int(float(w())*0.015f),int(float(h())*0.126f),int(float(w())*0.249f),int(float(h())*0.7545f));
  }

Rect GameMenu::optionsPaneRect() const {
  return Rect(int(float(w())*0.3018f),int(float(h())*0.126f),int(float(w())*0.6382f),int(float(h())*0.7545f));
  }

bool GameMenu::scanPolish() const {
  static bool known = false;
  if(known)
    return true;
  if(Gothic::settingsGetI("GAME","language")==2) {
    known = true;
    return true;
    }
  static const char* words[] = {"Nowa gra","Wczytaj","Zapisz","Ustawienia","Opcje","Wyj\x9C" "ie","Wyjd\x9F","Autorzy","Wprowadzenie","Wzn\xF3" "w"};
  for(const auto& it:hItems) {
    if(!it.handle)
      continue;
    for(auto* word:words)
      if(it.handle->text[0].find(word)==0) {
        known = true;
        return true;
        }
    }
  return false;
  }

GameMenu::PanelGeom GameMenu::panelGeom() const {
  const float W = float(w());
  const float H = float(h());
  PanelGeom g;
  g.rowH    = std::max(1,int(H*0.113f));
  g.rowsTop = int(H*0.19f);
  g.visible = std::max(1,int((H-float(g.rowsTop)-H*0.03f)/float(g.rowH)));
  g.padX    = int(W*0.052f);
  g.trackX0 = int(W*0.566f);
  g.trackX1 = int(W*0.876f);
  g.valueR  = int(W*0.95f);
  return g;
  }

/// The categories are the buttons of the menu that open another menu. The menu of the chosen one is made as a part of this screen.
void GameMenu::optionsUpdate() {
  if(!optView.built && vm!=nullptr) {
    optView.built = true;
    for(uint32_t i:touchRows()) {
      const auto& h = hItems[i].handle;
      for(size_t a=0; a<zenkit::IMenuItem::select_action_count; ++a) {
        if(h->on_sel_action[a]==int(zenkit::MenuItemSelectAction::START_MENU) &&
           vm->find_symbol_by_name(h->on_sel_action_s[a])!=nullptr) {
          optView.cats.push_back(i);
          break;
          }
        }
      }
    if(!optView.cats.empty())
      optionsSelect(0);
    }

  if(optView.pane!=nullptr) {
    const auto r = optionsPaneRect();
    optView.pane->setPosition(r.x,r.y);
    optView.pane->resize(r.w,r.h);
    }
  }

void GameMenu::optionsSelect(int category) {
  if(category<0 || size_t(category)>=optView.cats.size() || vm==nullptr)
    return;
  if(category==optView.active && optView.pane!=nullptr)
    return;

  const auto& h = hItems[optView.cats[size_t(category)]].handle;
  std::string target;
  for(size_t a=0; a<zenkit::IMenuItem::select_action_count; ++a)
    if(h->on_sel_action[a]==int(zenkit::MenuItemSelectAction::START_MENU))
      target = h->on_sel_action_s[a];
  if(target.empty())
    return;

  removeAllWidgets();
  optView.pane   = nullptr;
  optView.active = category;

  auto* g = new GameMenu(owner,keyCodec,*vm,target,kClose);
  g->embedded   = true;
  g->panelTitle = h->text[0];
  addWidget(g);
  optView.pane = g;

  const auto r = optionsPaneRect();
  g->setPosition(r.x,r.y);
  g->resize(r.w,r.h);
  update();
  }

void GameMenu::optionsTouch(float x, float y, int phase) {
  if(optView.pane==nullptr)
    return;
  const auto r = optionsPaneRect();
  if(phase==0)
    optView.drag = r.contains(int(x),int(y));
  if(!optView.drag)
    return;
  optView.pane->mobileTouch(x-float(r.x),y-float(r.y),phase);
  if(phase>=2)
    optView.drag = false;
  update();
  }

void GameMenu::optionsTap(float x, float y) {
  const auto s = optionsSidebar();
  for(size_t i=0; i<optView.cats.size(); ++i) {
    if(!categoryRect(s,int(i)).contains(int(x),int(y)))
      continue;
    if(int(i)!=optView.active) {
      Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_BROWSE"));
      optionsSelect(int(i));
      }
    return;
    }

  const auto r = optionsPaneRect();
  if(optView.pane!=nullptr && r.contains(int(x),int(y))) {
    optView.pane->mobileTap(x-float(r.x),y-float(r.y));
    update();
    }
  }

void GameMenu::paintOptionsHost(Painter& p) {
  const int  W  = w();
  const int  H  = h();
  const auto s  = optionsSidebar();
  const bool pl = scanPolish();

  bar(p,0,0,W,H,Dim);
  frame(p,s,1,PanelLine,PanelFill);

  const int pad = int(float(s.w)*0.07f);
  auto fTiny    = fontOf(false,int(float(s.h)*0.026f),Grey);
  drawSpaced(p,fTiny,s.x+pad,s.y+int(float(s.h)*0.062f)+fTiny.pixelSize()/2,
             pl ? "DOSTOSUJ PRZYGOD\xCA" : "CUSTOMIZE YOUR ADVENTURE",std::max(1,int(float(H)*0.0035f)));

  // the heading of the menu is its first text
  std::string heading;
  {
    int best = 1800;
    for(const auto& candidate:hItems) {
      if(!candidate.handle || !candidate.visible || candidate.handle->type!=zenkit::MenuItemType::TEXT ||
         candidate.handle->text[0].empty() || isSelectable(candidate.handle) || candidate.handle->pos_y>=best)
        continue;
      best    = candidate.handle->pos_y;
      heading = candidate.handle->text[0];
      }
  }
  auto fTitle = fontOf(true,int(float(s.h)*0.088f),Cream);
  fTitle.drawText(p,s.x+pad,s.y+int(float(s.h)*0.126f)+fTitle.pixelSize()/2,fit(fTitle,heading,s.w-2*pad));

  auto fItem = fontOf(true,int(float(s.h)*0.052f),Inactive);
  auto fHi   = fontOf(true,int(float(s.h)*0.052f),Cream);
  for(size_t i=0; i<optView.cats.size(); ++i) {
    const auto  r      = categoryRect(s,int(i));
    const bool  active = int(i)==optView.active;
    const auto& label  = hItems[optView.cats[i]].handle->text[0];
    if(active) {
      bar(p,r.x,r.y,r.w,r.h,RowSel);
      bar(p,r.x,r.y,std::max(2,int(float(H)*0.0035f)),r.h,Gold);
      }
    auto& f = active ? fHi : fItem;
    f.drawText(p,r.x+int(float(s.w)*0.053f),r.y+r.h/2+f.pixelSize()/2,fit(f,label,r.w-int(float(s.w)*0.1f)));
    }
  }

/// The menu of a category in the panel: the title, the rows with their values
void GameMenu::paintPanel(Painter& p) {
  // the place of the panel on the screen, the triangles need it
  // the window itself is not counted: its place is the place on the desktop, the painter starts inside of it
  Point place;
  for(const Widget* wx=this; wx->owner()!=nullptr; wx=wx->owner())
    place += wx->pos();
  Origin originGuard(place);

  const int W = w();
  const int H = h();
  frame(p,Rect(0,0,W,H),1,PanelLine,PanelFill);

  const auto g    = panelGeom();
  const auto rows = touchRows();
  touch.offset = std::clamp(touch.offset,0,std::max(0,int(rows.size())-g.visible));

  auto fTitle = fontOf(true,int(float(H)*0.092f),Cream);
  fTitle.drawText(p,g.padX,int(float(H)*0.074f)+fTitle.pixelSize()/2,fit(fTitle,panelTitle,W-2*g.padX));

  auto fLabel = fontOf(true,int(float(H)*0.058f),Cream);
  auto fOff   = fontOf(true,int(float(H)*0.058f),Grey);
  auto fValue = fontOf(false,int(float(H)*0.036f),Gold);
  const int th = std::max(2,int(float(H)*0.0075f));

  for(int r=0; r<g.visible && size_t(touch.offset+r)<rows.size(); ++r) {
    const uint32_t index     = rows[size_t(touch.offset+r)];
    const uint32_t controlId = touchControl(index);
    auto&          item      = hItems[index];
    auto&          control   = hItems[controlId];
    const bool     enabled   = isEnabled(item.handle);
    const int      top       = g.rowsTop+r*g.rowH;
    const int      yc        = top+g.rowH/2;

    if(&item==selectedItem() && enabled)
      bar(p,1,top,W-2,g.rowH,Color(0.84f,0.70f,0.40f,0.07f));

    auto& fnt = enabled ? fLabel : fOff;
    fnt.drawText(p,g.padX,yc+fnt.pixelSize()/2,fit(fnt,touchRowLabel(index,controlId),g.trackX0-g.padX-int(float(W)*0.03f)));

    const auto type = control.handle->type;
    if(type==zenkit::MenuItemType::SLIDER) {
      const float value = int(controlId)==touch.slider ? touch.preview :
        Gothic::settingsGetF(control.handle->on_chg_set_option_section,control.handle->on_chg_set_option);
      const float v   = std::clamp(value,0.f,1.f);
      const int   len = g.trackX1-g.trackX0;
      bar(p,g.trackX0,yc-th/2,len,th,Track);
      bar(p,g.trackX0,yc-th/2,int(float(len)*v),th,Gold);
      disc(p,float(g.trackX0)+float(len)*v,float(yc),float(H)*0.017f,Knob);

      char num[16] = {};
      std::snprintf(num,sizeof(num),"%d%%",int(v*100.f+0.5f));
      fValue.drawText(p,g.valueR-fValue.textSize(num).w,yc+fValue.pixelSize()/2,num);
      } else
    if(type==zenkit::MenuItemType::CHOICEBOX) {
      getText(control,textBuf);
      const std::string text = textBuf.data();
      if(strEnumSize(control.handle->text[0])==2) {
        // a switch
        const bool on = Gothic::settingsGetI(control.handle->on_chg_set_option_section,control.handle->on_chg_set_option)!=0;
        const int  pw = int(float(H)*0.086f);
        const int  ph = int(float(H)*0.03f);
        const int  px = g.trackX0;
        const float rr = float(ph)/2;
        const Color fill = on ? Gold : Color(0.19f,0.24f,0.29f,1.f);
        bar(p,px+int(rr),yc-ph/2,pw-int(rr)*2,ph,fill);
        disc(p,float(px)+rr,float(yc),rr,fill);
        disc(p,float(px+pw)-rr,float(yc),rr,fill);
        disc(p,on ? float(px+pw)-rr : float(px)+rr,float(yc),rr*0.72f,on ? Knob : Grey);
        fValue.drawText(p,px+pw+int(float(W)*0.03f),yc+fValue.pixelSize()/2,text);
        } else {
        const float ch = float(H)*0.03f;
        const float mid = float(g.trackX0+g.trackX1)/2;
        chevron(p,float(g.trackX0)+ch*0.5f,float(yc),ch,std::max(2.f,float(H)*0.0035f),false,Gold);
        chevron(p,float(g.trackX1)-ch*0.5f,float(yc),ch,std::max(2.f,float(H)*0.0035f),true,Gold);
        const auto shown = fit(fValue,text,g.trackX1-g.trackX0-int(ch*3));
        fValue.drawText(p,int(mid)-fValue.textSize(shown).w/2,yc+fValue.pixelSize()/2,shown);
        }
      } else
    if(type==zenkit::MenuItemType::INPUT && control.handle->on_chg_set_option_section=="KEYS") {
      const std::string keys{std::string_view(KeyCodec::keysStr(Gothic::settingsGetS(control.handle->on_chg_set_option_section,control.handle->on_chg_set_option)))};
      fValue.drawText(p,g.trackX0,yc+fValue.pixelSize()/2,fit(fValue,keys,g.valueR-g.trackX0));
      } else {
      // a button that opens a menu
      chevron(p,float(g.valueR),float(yc),float(H)*0.034f,std::max(2.f,float(H)*0.0035f),true,Gold);
      }

    bar(p,g.padX,top+g.rowH-1,W-2*g.padX,1,Hairline);
    }

  if(rows.size()>size_t(g.visible)) {
    const int bw = std::max(3,int(float(W)*0.004f));
    const int ah = g.rowH*g.visible;
    const int bh = std::max(bw*4,int(float(ah)*float(g.visible)/float(rows.size())));
    const int by = g.rowsTop+int(float(ah-bh)*float(touch.offset)/float(int(rows.size())-g.visible));
    bar(p,W-bw-4,by,bw,bh,Gold);
    }

  }
  #endif
