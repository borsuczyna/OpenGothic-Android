#include "winpad.h"

#if defined(_WIN32) && defined(OPENGOTHIC_TOUCH_UI)

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>

using namespace Mobile;

namespace {

// ---- XInput, loaded by name: the program does not need it to start

struct XState {
  DWORD packet;
  WORD  buttons;
  BYTE  lt, rt;
  SHORT lx, ly, rx, ry;
  };

using XGetState = DWORD (WINAPI*)(DWORD, XState*);

XGetState xinputFunction() {
  static const XGetState fn = []() -> XGetState {
    for(const char* name:{"xinput1_4.dll","xinput1_3.dll","xinput9_1_0.dll"}) {
      if(HMODULE m = LoadLibraryA(name)) {
        if(FARPROC f = GetProcAddress(m,"XInputGetState"))
          return reinterpret_cast<XGetState>(reinterpret_cast<void*>(f));
        }
      }
    return nullptr;
    }();
  return fn;
  }

float unitShort(SHORT v) {
  return std::clamp(float(v)/32767.f,-1.f,1.f);
  }

bool pollXInput(Gamepad& pad) {
  auto get = xinputFunction();
  if(get==nullptr)
    return false;
  for(DWORD user=0; user<4; ++user) {
    XState s = {};
    if(get(user,&s)!=ERROR_SUCCESS)
      continue;

    auto button = [&](Pad p, WORD mask) { pad.button(p,(s.buttons & mask)!=0); };
    button(Pad::A,          0x1000);
    button(Pad::B,          0x2000);
    button(Pad::X,          0x4000);
    button(Pad::Y,          0x8000);
    button(Pad::LeftBumper, 0x0100);
    button(Pad::RightBumper,0x0200);
    button(Pad::LeftStick,  0x0040);
    button(Pad::RightStick, 0x0080);
    button(Pad::Start,      0x0010);
    button(Pad::Select,     0x0020);
    button(Pad::DpadUp,     0x0001);
    button(Pad::DpadDown,   0x0002);
    button(Pad::DpadLeft,   0x0004);
    button(Pad::DpadRight,  0x0008);
    // XInput has y upwards, the model has it downwards
    pad.axes(unitShort(s.lx),-unitShort(s.ly),unitShort(s.rx),-unitShort(s.ry),float(s.lt)/255.f,float(s.rt)/255.f);
    return true;
    }
  return false;
  }

// ---- DirectInput: the DualShock 4, the DualSense and the other pads that are not XInput

struct Device {
  IDirectInput8A*       api  = nullptr;
  IDirectInputDevice8A* dev  = nullptr;
  bool                  sony = false;
  ULONGLONG             nextScan = 0;
  };

Device device;

BOOL CALLBACK onDevice(const DIDEVICEINSTANCEA* inst, void* user) {
  auto& d = *static_cast<Device*>(user);
  IDirectInputDevice8A* dev = nullptr;
  if(FAILED(d.api->CreateDevice(inst->guidInstance,&dev,nullptr)))
    return DIENUM_CONTINUE;
  if(FAILED(dev->SetDataFormat(&c_dfDIJoystick2))) {
    dev->Release();
    return DIENUM_CONTINUE;
    }

  HWND wnd = GetActiveWindow();
  if(wnd==nullptr)
    wnd = GetDesktopWindow();
  dev->SetCooperativeLevel(wnd,DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);

  // the axes in -1000..1000, the same for every pad
  for(DWORD offset:{DWORD(DIJOFS_X),DWORD(DIJOFS_Y),DWORD(DIJOFS_Z),DWORD(DIJOFS_RX),DWORD(DIJOFS_RY),DWORD(DIJOFS_RZ)}) {
    DIPROPRANGE range = {};
    range.diph.dwSize       = sizeof(DIPROPRANGE);
    range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    range.diph.dwHow        = DIPH_BYOFFSET;
    range.diph.dwObj        = offset;
    range.lMin = -1000;
    range.lMax = +1000;
    dev->SetProperty(DIPROP_RANGE,&range.diph);
    }
  dev->Acquire();

  d.dev  = dev;
  d.sony = LOWORD(inst->guidProduct.Data1)==0x054C;   // the vendor id of Sony
  return DIENUM_STOP;
  }

void dropDevice() {
  if(device.dev!=nullptr) {
    device.dev->Unacquire();
    device.dev->Release();
    device.dev = nullptr;
    }
  }

float unitAxis(LONG v) {
  return std::clamp(float(v)/1000.f,-1.f,1.f);
  }

bool pollDirectInput(Gamepad& pad) {
  const ULONGLONG now = GetTickCount64();
  if(device.api==nullptr) {
    if(now<device.nextScan)
      return false;
    if(FAILED(DirectInput8Create(GetModuleHandleA(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8A,
                                 reinterpret_cast<void**>(&device.api),nullptr))) {
      device.api      = nullptr;
      device.nextScan = now+5000;
      return false;
      }
    }
  if(device.dev==nullptr) {
    // an empty scan is not for every frame
    if(now<device.nextScan)
      return false;
    device.nextScan = now+1000;
    device.api->EnumDevices(DI8DEVCLASS_GAMECTRL,onDevice,&device,DIEDFL_ATTACHEDONLY);
    if(device.dev==nullptr)
      return false;
    }

  DIJOYSTATE2 st = {};
  HRESULT hr = device.dev->Poll();
  if(FAILED(hr)) {
    hr = device.dev->Acquire();
    if(FAILED(hr)) {
      dropDevice();      // unplugged: the next scan looks for another one
      return false;
      }
    device.dev->Poll();
    }
  if(FAILED(device.dev->GetDeviceState(sizeof(st),&st))) {
    dropDevice();
    return false;
    }

  auto down = [&](int i) { return (st.rgbButtons[i] & 0x80)!=0; };

  // the hat: the angle in hundredths of a degree, the low word 0xFFFF when it is let go
  bool hatUp = false, hatDown = false, hatLeft = false, hatRight = false;
  if(LOWORD(st.rgdwPOV[0])!=0xFFFF) {
    const int a = int(st.rgdwPOV[0]/100);
    hatUp    = a<=50  || a>=310;
    hatRight = a>=40  && a<=140;
    hatDown  = a>=130 && a<=230;
    hatLeft  = a>=220 && a<=320;
    }
  pad.button(Pad::DpadUp,   hatUp);
  pad.button(Pad::DpadDown, hatDown);
  pad.button(Pad::DpadLeft, hatLeft);
  pad.button(Pad::DpadRight,hatRight);

  if(device.sony) {
    // DualShock 4 and DualSense: 0 square, 1 cross, 2 circle, 3 triangle, 4 L1, 5 R1, 6 L2, 7 R2, 8 share, 9 options, 10 L3, 11 R3
    // axes: X, Y the left stick, Z the right stick to the sides, Rz up and down. The triggers are the buttons 6 and 7
    pad.button(Pad::A,          down(1));
    pad.button(Pad::B,          down(2));
    pad.button(Pad::X,          down(0));
    pad.button(Pad::Y,          down(3));
    pad.button(Pad::LeftBumper, down(4));
    pad.button(Pad::RightBumper,down(5));
    pad.button(Pad::Select,     down(8));
    pad.button(Pad::Start,      down(9));
    pad.button(Pad::LeftStick,  down(10));
    pad.button(Pad::RightStick, down(11));
    pad.axes(unitAxis(st.lX),unitAxis(st.lY),unitAxis(st.lZ),unitAxis(st.lRz),down(6) ? 1.f : 0.f,down(7) ? 1.f : 0.f);
    } else {
    // the common layout of the pads of the old kind: 0 A, 1 B, 2 X, 3 Y, 4 LB, 5 RB, 6 back, 7 start, 8 L3, 9 R3;
    // the right stick is Rx, Ry, the triggers are one axis Z (the left one positive)
    pad.button(Pad::A,          down(0));
    pad.button(Pad::B,          down(1));
    pad.button(Pad::X,          down(2));
    pad.button(Pad::Y,          down(3));
    pad.button(Pad::LeftBumper, down(4));
    pad.button(Pad::RightBumper,down(5));
    pad.button(Pad::Select,     down(6));
    pad.button(Pad::Start,      down(7));
    pad.button(Pad::LeftStick,  down(8));
    pad.button(Pad::RightStick, down(9));
    const float z = unitAxis(st.lZ);
    pad.axes(unitAxis(st.lX),unitAxis(st.lY),unitAxis(st.lRx),unitAxis(st.lRy),z>0.4f ? 1.f : 0.f,z<-0.4f ? 1.f : 0.f);
    }
  return true;
  }

}

void WinPad::poll(Gamepad& pad) {
  // a pad is read for the game that has the focus only
  DWORD pid = 0;
  GetWindowThreadProcessId(GetForegroundWindow(),&pid);
  if(pid!=GetCurrentProcessId()) {
    pad.reset();
    return;
    }

  if(pollXInput(pad))
    return;
  if(!pollDirectInput(pad))
    pad.reset();
  }

#else

void WinPad::poll(Mobile::Gamepad&) {
  }

#endif
