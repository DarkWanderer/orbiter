// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// Implementation of class StarSpec

#include <fstream>
#include <stdio.h>
#include "Orbiter.h"
#include "Config.h"
#include "Star.h"
#include "Camera.h"
#include "Log.h"

using namespace std;

extern Orbiter *g_pOrbiter;
extern Camera *g_camera;
extern char DBG_MSG[256];

static int texrefcount = 0;

Star::Star (double _mass, double _mean_radius)
: CelestialBody (_mass, _mean_radius)
{
	psys  = 0;
	upd_t = 0.0;
	Setup ();
}

Star::Star (char *fname)
: CelestialBody (fname)
{
	psys  = 0;
	upd_t = 0.0;
	ifstream ifs (g_pOrbiter->ConfigPath (fname));
	if (!ifs) return;
	bDynamicPosVel = false;
	// read star-specific parameters here
	InitDeviceObjects ();
	Setup ();
}

Star::~Star ()
{
	DestroyDeviceObjects ();
}

void Star::Setup ()
{
	CelestialBody::Setup ();
	ExternPosition(); // try to init from module
}

void Star::Update (bool force)
{
	if (bDynamicPosVel || (module && module->bEphemeris()))
		CelestialBody::Update (force);
	// otherwise don't update: keep star in the origin
	// of the global coordinate system
}

Vector Star::Pos2Barycentre (Vector &pos)
{
	// by definition, the barycentre of stars (root objects) is the origin
	return Vector(0,0,0);
}

void Star::InitDeviceObjects ()
{
}

void Star::DestroyDeviceObjects ()
{
	if (!texrefcount) return; // oops
	if (--texrefcount == 0)   // need to release
		oapiReleaseTexture(tex);
}
