// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#define OAPI_IMPLEMENTATION

#include "Script.h"
#include <dlfcn.h>
const char *path = "Modules";
const char *libname = "LuaInline";

ScriptInterface::ScriptInterface (Orbiter *pOrbiter)
{
	orbiter = pOrbiter;
	hLib = NULL;
}

void *ScriptInterface::LoadInterpreterLib ()
{
	hLib = orbiter->LoadModule (path, libname);
	return hLib;
}

INTERPRETERHANDLE ScriptInterface::NewInterpreter ()
{
	if (!hLib && !LoadInterpreterLib()) return 0;
	INTERPRETERHANDLE(*proc)() = (INTERPRETERHANDLE(*)())oapiModuleGetProcAddress(hLib, "opcNewInterpreter");
	if (proc) {
		INTERPRETERHANDLE hInterp = proc();
		return hInterp;
	}
	return 0;
}

int ScriptInterface::DelInterpreter (INTERPRETERHANDLE hInterp)
{
	if (!hLib && !LoadInterpreterLib()) return 0;
	int(*proc)(INTERPRETERHANDLE) = (int(*)(INTERPRETERHANDLE))oapiModuleGetProcAddress(hLib, "opcDelInterpreter");
	if (proc) return proc(hInterp);
	else return 3;
}

INTERPRETERHANDLE ScriptInterface::RunInterpreter (const char *cmd)
{
	if (!hLib && !LoadInterpreterLib()) return NULL;
	INTERPRETERHANDLE(*proc)(const char*) = (INTERPRETERHANDLE(*)(const char*))oapiModuleGetProcAddress(hLib, "opcRunInterpreter");
	INTERPRETERHANDLE hInterp = NULL;
	if (proc) {
		hInterp = proc(cmd);
	}
	return hInterp;
}

bool ScriptInterface::ExecScriptCmd (INTERPRETERHANDLE hInterp, const char *cmd)
{
	if (!hLib && !LoadInterpreterLib()) return false;
	bool(*proc)(INTERPRETERHANDLE,const char*) = (bool(*)(INTERPRETERHANDLE,const char*))oapiModuleGetProcAddress(hLib, "opcExecScriptCmd");
	if (proc) return proc(hInterp, cmd);
	else      return false;
}

bool ScriptInterface::AsyncScriptCmd (INTERPRETERHANDLE hInterp, const char *cmd)
{
	if (!hLib && !LoadInterpreterLib()) return false;
	bool(*proc)(INTERPRETERHANDLE,const char*) = (bool(*)(INTERPRETERHANDLE,const char*))oapiModuleGetProcAddress(hLib, "opcAsyncScriptCmd");
	if (proc) return proc(hInterp, cmd);
	else      return false;
}

lua_State *ScriptInterface::GetLua (INTERPRETERHANDLE hInterp)
{
	if (!hLib && !LoadInterpreterLib()) return NULL;
	lua_State*(*proc)(INTERPRETERHANDLE)=(lua_State*(*)(INTERPRETERHANDLE))oapiModuleGetProcAddress(hLib, "opcGetLua");
	if (proc) return proc(hInterp);
	else      return NULL;
}
