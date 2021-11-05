// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include <windows.h>

INT WINAPI WinMain (HINSTANCE hInstance, HINSTANCE, LPSTR strCmdLine, INT nCmdShow)
{
	const char *cmd = "Modules\\Server\\Orbiter.exe";
	CreateProcess(cmd, strCmdLine, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}
