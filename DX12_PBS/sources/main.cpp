//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "DX12_PBS_sample.h"

int WINAPI main(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
  // Declare this process to be high DPI aware, and prevent automatic scaling 
  HINSTANCE hUser32 = LoadLibrary(L"user32.dll");
  if (hUser32)
  {
    typedef BOOL(WINAPI* LPSetProcessDPIAware)(void);
    LPSetProcessDPIAware pSetProcessDPIAware = (LPSetProcessDPIAware)GetProcAddress(hUser32, "SetProcessDPIAware");
    if (pSetProcessDPIAware)
    {
      pSetProcessDPIAware();
    }
    FreeLibrary(hUser32);
  }

  DX12PBSSample sample(1280, 720, L"D3D12 PBS sample");
  return Win32Application::Run(&sample, hInstance, nCmdShow);
}
