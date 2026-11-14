#include "savescreen.h"

#include <Tempest/Application>
#include <Tempest/Log>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>

#include "game/gamescript.h"
#include "game/questlog.h"
#include "ui/touchdraw.h"
#include "utils/gthfont.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;
using namespace touchdraw;

namespace {

/// the letters of the Polish alphabet in the code page of the font
uint8_t polishByte(uint32_t code) {
  switch(code) {
    case 0x105: return 0xB9; case 0x107: return 0xE6; case 0x119: return 0xEA; case 0x142: return 0xB3;
    case 0x144: return 0xF1; case 0x0F3: return 0xF3; case 0x15B: return 0x9C; case 0x17A: return 0x9F;
    case 0x17C: return 0xBF;
    case 0x104: return 0xA5; case 0x106: return 0xC6; case 0x118: return 0xCA; case 0x141: return 0xA3;
    case 0x143: return 0xD1; case 0x0D3: return 0xD3; case 0x15A: return 0x8C; case 0x179: return 0x8F;
    case 0x17B: return 0xAF;
    default: return 0;
    }
  }

}

struct SaveScreen::Layout {
  int  W = 0, H = 0;
  Rect back;
  Rect list;        // the panel
  Rect inner;       // where the rows are
  Rect detail;      // the panel
  Rect thumb;
  Rect content;     // inside of the detail panel
  int  rowH     = 0;
  int  buttonH  = 0;
  int  buttonY  = 0;
  int  pad      = 0;
  };

SaveScreen::SaveScreen(Mode mode, bool polish) : mode(mode), polish(polish) {
  if(auto* q = Gothic::inst().questLog())
    freshName = std::string(q->lastUpdated());
  if(freshName.empty()) {
    if(auto* w = Gothic::inst().world())
      freshName = SaveStore::placeName(w->name(),polish);
    }
  if(freshName.empty())
    freshName = tr("Save","Zapis");

  reload("");
  }

SaveScreen::~SaveScreen() {
  Resources::device().waitIdle(); // safe-delete of the picture
  }

SaveScreen::Layout SaveScreen::layout(int w, int h) const {
  Layout l;
  l.W = w;
  l.H = h;
  const int left = int(float(w)*0.062f);
  const int top  = int(float(h)*0.297f);
  const int ph   = int(float(h)*0.590f);
  l.pad    = std::max(2,int(float(h)*0.0107f));
  l.back   = Rect(left-l.pad,int(float(h)*0.030f),int(float(w)*0.12f),int(float(h)*0.062f));
  l.list   = Rect(left,top,int(float(w)*0.483f),ph);
  l.detail = Rect(int(float(w)*0.5726f),top,int(float(w)*0.368f),ph);
  l.inner  = Rect(l.list.x+1,l.list.y+l.pad,l.list.w-2,l.list.h-2*l.pad);
  l.rowH   = int(float(h)*0.1155f);

  const int dp = int(float(w)*0.0113f);
  l.content = Rect(l.detail.x+dp,l.detail.y+int(float(h)*0.0203f),l.detail.w-2*dp,l.detail.h-int(float(h)*0.0203f)-int(float(h)*0.0214f));
  l.thumb   = Rect(l.content.x,l.content.y,l.content.w,int(float(h)*0.166f));
  l.buttonH = int(float(h)*0.059f);
  l.buttonY = l.content.y+l.content.h-l.buttonH;
  return l;
  }

const SaveScreen::Row* SaveScreen::current() const {
  return sel>=0 && size_t(sel)<rows.size() ? &rows[size_t(sel)] : nullptr;
  }

bool SaveScreen::canSave() const {
  auto* w = Gothic::inst().world();
  if(w==nullptr || w->currentCs()!=nullptr)
    return false;
  return !w->script().dmaSaveDisabled();
  }

bool SaveScreen::canLoad() const {
  auto* w = Gothic::inst().world();
  return w==nullptr || !w->script().dmaLoadDisabled();
  }

SaveGameHeader SaveScreen::liveHeader(int32_t& chapter, int32_t& level) const {
  SaveGameHeader hdr;
  chapter = 0;
  level   = 0;
  const time_t now = std::time(nullptr);
  if(const tm* t = std::localtime(&now))
    hdr.pcTime = *t;
  if(auto* w = Gothic::inst().world()) {
    hdr.world    = w->name();
    hdr.playTime = w->gameSession().playTime();
    chapter      = w->script().chapter();
    if(auto* pl = w->player())
      level = pl->level();
    }
  return hdr;
  }

void SaveScreen::reload(const std::string& keepFile) {
  rows.clear();
  if(mode==Save) {
    Row r;
    r.fresh = true;
    rows.push_back(r);
    }
  for(auto& e:SaveStore::list()) {
    if(mode==Save && e.isQuick())
      continue;
    Row r;
    r.e = e;
    rows.push_back(std::move(r));
    }

  int at = 0;
  if(!keepFile.empty()) {
    for(size_t i=0; i<rows.size(); ++i)
      if(!rows[i].fresh && rows[i].e.file==keepFile)
        at = int(i);
    }
  thumbFile.clear();
  select(at);
  }

void SaveScreen::select(int row) {
  sel = rows.empty() ? 0 : std::clamp(row,0,int(rows.size())-1);
  loadThumb();
  }

void SaveScreen::loadThumb() {
  auto* r = current();
  if(r==nullptr || r->fresh) {
    hasThumb  = false;
    thumbFile.clear();
    return;
    }
  if(thumbFile==r->e.file)
    return;

  thumbFile = r->e.file;
  Pixmap pm = SaveStore::preview(r->e);
  hasThumb  = !pm.isEmpty();
  if(hasThumb) {
    Resources::device().waitIdle(); // the old picture may be in use
    thumb = Resources::loadTexturePm(pm);
    }
  }

void SaveScreen::ensureVisible(const Layout& l) {
  const float top    = float(sel*l.rowH);
  const float bottom = top+float(l.rowH);
  if(top<scroll)
    scroll = top;
  if(bottom>scroll+float(l.inner.h))
    scroll = bottom-float(l.inner.h);
  scroll = std::clamp(scroll,0.f,scrollMax);
  }

std::string SaveScreen::countText(size_t n) const {
  std::string num = std::to_string(n);
  std::string noun;
  if(polish) {
    const size_t m10  = n%10;
    const size_t m100 = n%100;
    if(n==1)
      noun = "zapis"; else
    if(m10>=2 && m10<=4 && !(m100>=12 && m100<=14))
      noun = "zapisy"; else
      noun = "zapis\xF3w";
    return num+" "+noun+"\x1f"+"bez limitu slot\xF3w";
    }
  return num+(n==1 ? " save" : " saves")+"\x1f"+"no slot limit";
  }

std::vector<SaveScreen::Button> SaveScreen::buttons(const Layout& l) const {
  std::vector<Button> ret;

  if(state==State::Confirm || state==State::Rename) {
    const int  cw = int(float(l.W)*0.46f);
    const int  ch = int(float(l.H)*(state==State::Confirm ? 0.30f : 0.28f));
    const Rect card((l.W-cw)/2,state==State::Confirm ? (l.H-ch)/2 : int(float(l.H)*0.17f),cw,ch);
    const int  bw = int(float(cw)*0.38f);
    const int  bh = l.buttonH;
    const int  by = card.y+card.h-bh-int(float(l.H)*0.025f);
    Button no, yes;
    no.rect  = Rect(card.x+int(float(cw)*0.08f),by,bw,bh);
    yes.rect = Rect(card.x+cw-int(float(cw)*0.08f)-bw,by,bw,bh);
    no.act   = Act::No;
    yes.act  = Act::Yes;
    yes.primary = true;
    if(state==State::Confirm) {
      no.label  = tr("No","Nie");
      yes.label = tr("Yes","Tak");
      } else {
      no.label  = tr("Cancel","Anuluj");
      yes.label = "OK";
      }
    ret.push_back(no);
    ret.push_back(yes);
    return ret;
    }

  {
  Button b;
  b.rect  = l.back;
  b.act   = Act::Back;
  ret.push_back(b);
  }

  const Row* r = current();
  if(r==nullptr)
    return ret;

  const int iw  = l.content.w;
  const int gap = int(float(iw)*0.031f);
  int       x   = l.content.x;
  auto add = [&](int w, Act act, const std::string& label, bool primary, bool enabled, bool arrow) {
    Button b;
    b.rect    = Rect(x,l.buttonY,w,l.buttonH);
    b.act     = act;
    b.label   = label;
    b.primary = primary;
    b.enabled = enabled;
    b.arrow   = arrow;
    ret.push_back(b);
    x += w+gap;
    };

  const bool hasDelete = !r->fresh;
  const int  wDelete   = int(float(iw)*0.20f);
  const int  wRename   = int(float(iw)*0.30f);
  int        wMain     = iw;
  if(hasDelete)
    wMain -= wDelete+gap;
  wMain -= wRename+gap;

  if(hasDelete)
    add(wDelete,Act::Delete,tr("Delete","Usu\xF1"),false,true,false);
  add(wRename,Act::Rename,tr("Rename","Zmie\xF1 nazw\xEA"),false,true,false);
  if(mode==Load)
    add(wMain,Act::Primary,tr("Load game","Wczytaj gr\xEA"),true,canLoad(),true);
  else
    add(wMain,Act::Primary,r->fresh ? tr("Save game","Zapisz gr\xEA") : tr("Overwrite","Nadpisz"),true,canSave(),true);
  return ret;
  }

void SaveScreen::paint(Painter& p, int W, int H) {
  lastW = W;
  lastH = H;
  const auto l = layout(W,H);
  const int  n = int(rows.size());
  scrollMax = float(std::max(0,n*l.rowH-l.inner.h));
  scroll    = std::clamp(scroll,0.f,scrollMax);

  const int small = int(float(H)*0.030f);
  auto fSmall = fontOf(false,small,Grey);
  auto fText  = fontOf(false,int(float(H)*0.034f),White);
  auto fTiny  = fontOf(false,int(float(H)*0.024f),Grey);

  auto textC = [&](const GthFont& f, int x, int cy, const std::string& s) {
    f.drawText(p,x,cy+f.pixelSize()/2,s);
    };

  bar(p,0,0,W,H,Dim);

  // the way back
  {
  const int   cy = l.back.y+l.back.h/2;
  const float th = std::max(2.f,float(H)*0.0035f);
  const float ch = float(H)*0.026f;
  chevron(p,float(l.back.x+l.pad)+ch*0.3f,float(cy),ch,th,false,Grey);
  textC(fSmall,l.back.x+l.pad+int(ch*1.1f),cy,tr("Back","Powr\xF3t"));
  }

  // the heading
  const int left = l.list.x;
  {
  const std::string eyebrow = upper(mode==Load ? tr("Your story","Twoja historia") : tr("Your adventure","Twoja przygoda"));
  drawSpaced(p,fTiny,left,int(float(H)*0.138f)+fTiny.pixelSize()/2,eyebrow,std::max(1,int(float(H)*0.0035f)));

  auto title = fontOf(true,int(float(H)*0.092f),Cream);
  title.drawText(p,left,int(float(H)*0.200f)+title.pixelSize()/2,mode==Load ? tr("Load game","Wczytaj gr\xEA") : tr("Save game","Zapisz gr\xEA"));

  const auto sub = countText(rows.size()-(mode==Save && !rows.empty() ? size_t(1) : size_t(0)));
  drawJoined(p,fSmall,left,int(float(H)*0.250f)+fSmall.pixelSize()/2,sub,int(float(H)*0.007f),Grey);
  }

  // the list of the saves
  frame(p,l.list,1,PanelLine,PanelFill);
  p.setScissor(l.inner.x,l.inner.y,l.inner.w,l.inner.h);
  {
  auto fNum    = fontOf(false,int(float(H)*0.028f),Grey);
  auto fName   = fontOf(true,int(float(H)*0.045f),Cream);
  const int numX  = l.list.x+int(float(W)*0.0185f);
  const int nameX = l.list.x+int(float(W)*0.0524f);
  const int chevX = l.list.x+l.list.w-int(float(W)*0.027f);
  const int textW = chevX-nameX-int(float(W)*0.02f);

  int number = 0;
  for(int i=0; i<n; ++i) {
    const int y = l.inner.y+i*l.rowH-int(scroll);
    if(y+l.rowH<l.inner.y || y>l.inner.y+l.inner.h)
      continue;
    auto& r  = rows[size_t(i)];
    const bool chosen = i==sel;
    if(!r.fresh)
      ++number;

    const int rx = l.list.x+int(float(W)*0.006f);
    const int rw = l.list.w-int(float(W)*0.012f);
    if(chosen) {
      bar(p,rx,y,rw,l.rowH,RowSel);
      bar(p,rx,y,std::max(2,int(float(H)*0.0035f)),l.rowH,Gold);
      }
    if(i+1<n && !chosen && i+1!=sel)
      bar(p,rx+int(float(W)*0.01f),y+l.rowH-1,rw-int(float(W)*0.02f),1,Hairline);

    char num[16] = {};
    std::snprintf(num,sizeof(num),"%02d",number);
    textC(fNum,numX,y+int(float(l.rowH)*0.36f),r.fresh ? "+" : num);

    std::string title = r.fresh ? tr("New save","Nowy zapis") : (r.e.isQuick() ? quickTitle(r.e) : r.e.hdr.name);
    if(title.empty())
      title = "-";
    const auto shown = fit(fName,title,textW);
    textC(fName,nameX,y+int(float(l.rowH)*0.36f),shown);

    std::string sub;
    if(r.fresh) {
      sub = freshName;
      } else {
      sub = SaveStore::dateText(r.e.hdr);
      if(r.e.level>0)
        sub += "\x1f"+tr("Level ","Poziom ")+std::to_string(r.e.level);
      }
    if(r.fresh)
      textC(fTiny,nameX,y+int(float(l.rowH)*0.70f),fit(fTiny,sub,textW)); else
      drawJoined(p,fTiny,nameX,y+int(float(l.rowH)*0.70f)+fTiny.pixelSize()/2,sub,int(float(H)*0.006f),Grey);

    chevron(p,float(chevX),float(y)+float(l.rowH)*0.5f,float(H)*0.026f,std::max(2.f,float(H)*0.0035f),true,Gold);
    }
  }
  p.setScissor(0,0,W,H);

  if(scrollMax>0) {
    const int bw = std::max(3,int(float(W)*0.0035f));
    const int th = std::max(bw*4,int(float(l.inner.h)*float(l.inner.h)/float(l.inner.h+int(scrollMax))));
    const int ty = l.inner.y+int(float(l.inner.h-th)*scroll/scrollMax);
    bar(p,l.list.x+l.list.w-bw-3,ty,bw,th,Gold);
    }

  // the details
  frame(p,l.detail,1,PanelLine,PanelFill);
  if(const Row* r = current()) {
    int32_t chapter = 0, level = 0;
    SaveGameHeader live;
    const SaveGameHeader* hdr = nullptr;
    if(r->fresh) {
      live    = liveHeader(chapter,level);
      hdr     = &live;
      } else {
      hdr     = &r->e.hdr;
      chapter = r->e.chapter;
      level   = r->e.level;
      }

    // the picture
    const auto& t = l.thumb;
    if(!r->fresh && hasThumb && thumb.w()>0 && thumb.h()>0) {
      const float ta = float(t.w)/float(t.h);
      float sw = float(thumb.w()), sh = float(thumb.h());
      if(sw/sh>ta) sw = sh*ta; else sh = sw/ta;
      const int sx = int((float(thumb.w())-sw)/2);
      const int sy = int((float(thumb.h())-sh)/2);
      p.setBrush(Brush(thumb));
      p.drawRect(t.x,t.y,t.w,t.h,sx,sy,sx+int(sw),sy+int(sh));   // the last two are the far corner, not the size
      } else {
      frame(p,t,1,PanelLine,Color(0.02f,0.04f,0.07f,0.95f));
      const int   cx = t.x+t.w/2;
      const int   cy = t.y+t.h/2;
      const float s  = float(t.h)*0.18f;
      if(r->fresh) {
        bar(p,cx-int(s),cy-1,int(s*2),3,GoldDim);
        bar(p,cx-1,cy-int(s),3,int(s*2),GoldDim);
        }
      }
    bar(p,t.x,t.y,t.w,1,PanelLine);
    bar(p,t.x,t.y+t.h-1,t.w,1,PanelLine);
    bar(p,t.x,t.y,1,t.h,PanelLine);
    bar(p,t.x+t.w-1,t.y,1,t.h,PanelLine);

    // the place and the name
    int y = t.y+t.h+int(float(H)*0.030f);
    const std::string place = upper(SaveStore::placeName(hdr->world,polish));
    drawSpaced(p,fTiny,l.content.x,y+fTiny.pixelSize()/2,place,std::max(1,int(float(H)*0.0035f)));
    y += int(float(H)*0.030f);

    auto fTitle = fontOf(true,int(float(H)*0.052f),Cream);
    std::string name = r->fresh ? freshName : (r->e.isQuick() ? quickTitle(r->e) : r->e.hdr.name);
    if(name.empty())
      name = "-";
    const auto  ts   = fTitle.textSize(l.content.w,name);
    const int   maxH = int(float(H)*0.105f);
    if(ts.h>maxH) {
      // too long: the first lines only
      auto cut = name;
      while(!cut.empty() && fTitle.textSize(l.content.w,cut+"...").h>maxH)
        cut.pop_back();
      name = cut+"...";
      }
    const int th = fTitle.textSize(l.content.w,name).h;
    fTitle.drawText(p,l.content.x,y+fTitle.pixelSize(),l.content.w,0,name,AlignLeft);
    y += th+int(float(H)*0.018f);

    // the facts
    struct Fact { std::string label, value; };
    std::vector<Fact> facts = {
      {tr("Chapter","Rozdzia\xB3"),          chapter>0 ? std::to_string(chapter) : "-"},
      {tr("Hero level","Poziom bohatera"),   level>0 ? std::to_string(level) : "-"},
      {tr("Play time","Czas gry"),           SaveStore::playTimeText(*hdr)},
      {tr("Saved","Data zapisu"),            SaveStore::dateText(*hdr)},
      };
    const int rowGap = int(float(H)*0.0428f);
    for(auto& f:facts) {
      if(y+small>l.buttonY-int(float(H)*0.02f))
        break;
      textC(fSmall,l.content.x,y,f.label);
      const int vw = joinedWidth(fText,f.value,int(float(H)*0.007f));
      drawJoined(p,fText,l.content.x+l.content.w-vw,y+fText.pixelSize()/2,f.value,int(float(H)*0.007f),Grey);
      y += rowGap;
      }

    if(mode==Save && !canSave()) {
      const std::string msg = tr("Saving is not possible now","Nie mo\xBFna teraz zapisa\xE6 gry");
      auto fAlert = fontOf(false,small,Alert);
      textC(fAlert,l.content.x,l.buttonY-int(float(H)*0.035f),fit(fAlert,msg,l.content.w));
      }
    } else {
    auto fEmpty = fontOf(false,int(float(H)*0.036f),Grey);
    const std::string msg = tr("No saves yet","Brak zapis\xF3w");
    textC(fEmpty,l.detail.x+(l.detail.w-fEmpty.textSize(msg).w)/2,l.detail.y+l.detail.h/2,msg);
    }

  // the buttons of the screen
  for(auto& b:buttons(l)) {
    if(b.act==Act::Back || b.act==Act::No || b.act==Act::Yes)
      continue;
    const auto& r = b.rect;
    const bool  primary = b.primary;
    if(primary)
      bar(p,r.x,r.y,r.w,r.h,b.enabled ? Gold : GoldDim); else
      frame(p,r,1,Color(0.62f,0.72f,0.82f,0.34f),Color(0.04f,0.09f,0.13f,0.92f));

    auto f = fontOf(false,int(float(H)*0.034f),primary ? Dark : (b.enabled ? White : Grey));
    int tw = f.textSize(b.label).w;
    const int len = b.arrow ? int(float(H)*0.026f) : 0;
    const int gap = b.arrow ? int(float(H)*0.012f) : 0;
    const int x0  = r.x+(r.w-(tw+gap+len))/2;
    f.drawText(p,x0,r.y+r.h/2+f.pixelSize()/2,b.label);
    if(b.arrow)
      arrow(p,float(x0+tw+gap),float(r.y)+float(r.h)*0.5f,float(len),std::max(2.f,float(H)*0.0032f),Dark);
    }

  // a question or the name that is typed
  if(state!=State::Browse) {
    bar(p,0,0,W,H,Color(0.f,0.f,0.f,0.55f));
    const int  cw = int(float(W)*0.46f);
    const int  ch = int(float(H)*(state==State::Confirm ? 0.30f : 0.28f));
    const Rect card((W-cw)/2,state==State::Confirm ? (H-ch)/2 : int(float(H)*0.17f),cw,ch);
    frame(p,card,2,GoldDim,Color(0.03f,0.06f,0.09f,0.98f));

    const int mx = card.x+int(float(cw)*0.06f);
    const int mw = card.w-2*(mx-card.x);
    auto fTitle = fontOf(true,int(float(H)*0.046f),Cream);
    auto fMsg   = fontOf(false,int(float(H)*0.034f),White);

    if(state==State::Confirm) {
      const Row* r = current();
      const std::string what = r!=nullptr && !r->fresh ? (r->e.isQuick() ? quickTitle(r->e) : r->e.hdr.name) : "";
      const std::string head = ask==Ask::Delete ? tr("Delete save","Usu\xF1 zapis") : tr("Overwrite save","Nadpisz zapis");
      const std::string msg  = ask==Ask::Delete ? tr("This save will be gone for good.","Tego zapisu nie b\xEA" "dzie mo\xBF" "na odzyska\xE6.") :
                                                  tr("The old game of this save will be lost.","Stary stan gry w tym zapisie zostanie utracony.");
      fTitle.drawText(p,mx,card.y+int(float(H)*0.065f),mw,0,fit(fTitle,head,mw),AlignHCenter);
      fMsg.drawText(p,mx,card.y+int(float(H)*0.115f),mw,0,fit(fMsg,what,mw),AlignHCenter);
      auto fSm = fontOf(false,small,Grey);
      fSm.drawText(p,mx,card.y+int(float(H)*0.160f),mw,0,msg,AlignHCenter);
      } else {
      fTitle.drawText(p,mx,card.y+int(float(H)*0.065f),mw,0,tr("Name of the save","Nazwa zapisu"),AlignHCenter);
      const Rect field(mx,card.y+int(float(H)*0.085f),mw,int(float(H)*0.07f));
      frame(p,field,1,GoldDim,Color(0.01f,0.03f,0.05f,1.f));
      std::string shown = text;
      const bool caret = (Application::tickCount()/450)%2==0;
      auto fIn = fontOf(false,int(float(H)*0.040f),White);
      while(!shown.empty() && fIn.textSize(shown).w>field.w-int(float(W)*0.02f))
        shown.erase(0,1);
      const int tx = field.x+int(float(W)*0.01f);
      fIn.drawText(p,tx,field.y+field.h/2+fIn.pixelSize()/2,shown);
      if(caret)
        bar(p,tx+fIn.textSize(shown).w+2,field.y+field.h/2-fIn.pixelSize()/2,std::max(2,int(float(H)*0.003f)),fIn.pixelSize(),Gold);
      }

    for(auto& b:buttons(l)) {
      const auto& r = b.rect;
      if(b.primary)
        bar(p,r.x,r.y,r.w,r.h,Gold); else
        frame(p,r,1,Color(0.62f,0.72f,0.82f,0.34f),Color(0.04f,0.09f,0.13f,0.92f));
      auto f = fontOf(false,int(float(H)*0.034f),b.primary ? Dark : White);
      f.drawText(p,r.x+(r.w-f.textSize(b.label).w)/2,r.y+r.h/2+f.pixelSize()/2,b.label);
      }
    }
  }

void SaveScreen::touch(float x, float y, int phase, int w, int h) {
  if(state!=State::Browse) {
    drag = 0;
    return;
    }
  const auto l = layout(w,h);
  if(phase==0) {
    drag   = 0;
    startY = y;
    if(l.list.contains(int(x),int(y))) {
      drag        = 1;
      startScroll = scroll;
      }
    return;
    }
  if(phase>=2 || drag==0) {
    drag = 0;
    return;
    }

  // a finger that only trembles is a tap, not a scroll
  const float dead = float(h)*0.012f;
  float dy = startY-y;
  dy = dy>dead ? dy-dead : dy<-dead ? dy+dead : 0.f;
  scroll = std::clamp(startScroll+dy,0.f,scrollMax);
  }

void SaveScreen::tap(float x, float y, int w, int h) {
  const auto l = layout(w,h);
  for(auto& b:buttons(l)) {
    if(!b.rect.contains(int(x),int(y)))
      continue;
    if(!b.enabled)
      return;
    doAct(b.act);
    return;
    }
  if(state!=State::Browse)
    return;

  if(l.inner.contains(int(x),int(y)) && l.rowH>0) {
    const int at = int((y-float(l.inner.y)+scroll)/float(l.rowH));
    if(at>=0 && size_t(at)<rows.size())
      select(at);
    }
  }

void SaveScreen::step(int dir) {
  if(state!=State::Browse)
    return;
  select(sel+dir);
  ensureVisible(layout(lastW,lastH));
  }

void SaveScreen::confirm() {
  if(state==State::Browse)
    doAct(Act::Primary); else
  if(state==State::Confirm)
    doAct(Act::Yes); else
    acceptRename();
  }

bool SaveScreen::cancel() {
  if(state==State::Browse)
    return false;
  state = State::Browse;
  return true;
  }

SaveScreen::Result SaveScreen::takeResult() {
  const auto r = result;
  result = Result::None;
  return r;
  }

void SaveScreen::doAct(Act a) {
  const Row* r = current();
  switch(a) {
    case Act::None:
      break;
    case Act::Back:
      result = Result::Back;
      break;
    case Act::Delete:
      if(r!=nullptr && !r->fresh) {
        ask   = Ask::Delete;
        state = State::Confirm;
        }
      break;
    case Act::Rename:
      startRename();
      break;
    case Act::Primary:
      if(r==nullptr)
        break;
      if(mode==Load) {
        if(!canLoad())
          break;
        Gothic::inst().load(r->e.file);
        result = Result::Close;
        } else
      if(canSave()) {
        if(r->fresh) {
          doSave(SaveStore::newFile(),freshName);
          } else {
          ask   = Ask::Overwrite;
          state = State::Confirm;
          }
        }
      break;
    case Act::Yes:
      if(state==State::Rename) {
        acceptRename();
        break;
        }
      state = State::Browse;
      if(r!=nullptr && !r->fresh) {
        if(ask==Ask::Delete) {
          SaveStore::remove(r->e);
          const int keep = sel;
          reload("");
          select(keep);
          } else {
          doSave(r->e.file,r->e.hdr.name);
          }
        }
      break;
    case Act::No:
      state = State::Browse;
      break;
    }
  }

void SaveScreen::doSave(const std::string& file, const std::string& name) {
  Gothic::inst().save(file,name);
  result = Result::Close;
  }

void SaveScreen::startRename() {
  const Row* r = current();
  if(r==nullptr)
    return;
  textFresh = r->fresh;
  text      = r->fresh ? freshName : r->e.hdr.name;
  state     = State::Rename;
  }

void SaveScreen::acceptRename() {
  const Row* r = current();
  state = State::Browse;
  if(r==nullptr || text.empty())
    return;
  if(textFresh) {
    freshName = text;
    return;
    }
  const std::string file = r->e.file;
  if(SaveStore::rename(r->e,text))
    reload(file); else
    Tempest::Log::e("unable to rename the save ",file);
  }

void SaveScreen::key(const KeyEvent& e) {
  if(state!=State::Rename)
    return;
  if(e.key==Event::K_ESCAPE) {
    state = State::Browse;
    return;
    }
  if(e.key==Event::K_Return) {
    acceptRename();
    return;
    }
  if(e.key==Event::K_Back) {
    if(!text.empty())
      text.pop_back();
    return;
    }

  uint32_t code = e.code;
  if(code>=32 && code<127) {
    // ok
    } else
  if(polish && polishByte(code)!=0) {
    code = polishByte(code);
    } else {
    return;
    }
  if(text.size()<48)
    text.push_back(char(code));
  }
