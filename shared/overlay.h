#pragma once
#include <windows.h>

namespace overlay {

bool  Init();
void  Shutdown();

void  BeginFrame();                 // pumps messages + ImGui NewFrame
void  EndFrame();                   // Render + Clear + Present
void  AssertTopmost();              // call every N frames

HWND  Hwnd();
bool  MenuOpen();
void  SetMenuOpen(bool open);       // toggles WS_EX_TRANSPARENT for input passthrough
void  GetSize(int& w, int& h);

} // namespace overlay
