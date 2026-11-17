// Declarations of the functions ScriptHookRDR2.dll exports (by Alexander Blade, dev-c.com). The DLL itself is installed
// by the player. We link through our own import library (build.sh generates it from these declarations), so calls go
// through the .asi import table — the way Telegraph and other plugins expect to find them.
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>

#define TG_SH_IMPORT __declspec(dllimport)

using TgKeyboardHandler = void (*)(DWORD key, WORD repeats, BYTE scanCode, BOOL isExtended, BOOL isWithAlt,
                                   BOOL wasDownBefore, BOOL isUpNow);

TG_SH_IMPORT void scriptRegister(HMODULE module, void (*scriptMain)());
TG_SH_IMPORT void scriptUnregister(HMODULE module);
TG_SH_IMPORT void scriptWait(DWORD time);
TG_SH_IMPORT void keyboardHandlerRegister(TgKeyboardHandler handler);
TG_SH_IMPORT void keyboardHandlerUnregister(TgKeyboardHandler handler);
TG_SH_IMPORT void nativeInit(UINT64 hash);
TG_SH_IMPORT void nativePush64(UINT64 value);
TG_SH_IMPORT PUINT64 nativeCall();
TG_SH_IMPORT UINT64* getGlobalPtr(int globalId);
TG_SH_IMPORT int worldGetAllPeds(int* arr, int arrSize);
TG_SH_IMPORT int worldGetAllObjects(int* arr, int arrSize);
TG_SH_IMPORT int worldGetAllVehicles(int* arr, int arrSize);
