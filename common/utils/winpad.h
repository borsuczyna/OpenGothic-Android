#pragma once

#include "ui/gamepadmodel.h"

/// A gamepad on Windows for the desktop build of the touch controls (on Android the events of the system are used, see androidinput.cpp).
///
/// XInput first (Xbox pads, and any pad that Steam Input or DS4Windows presents as one), then DirectInput: the DualShock 4 and the DualSense are
/// found there without a driver. Does nothing on other systems.
namespace WinPad {

/// Once per frame: tells the model what the pad does now. The state is let go when the window of the game is not the active one.
void poll(Mobile::Gamepad& pad);

}
