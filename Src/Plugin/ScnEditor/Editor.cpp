// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ==============================================================
//              ORBITER MODULE: Scenario Editor
//                  Part of the ORBITER SDK
//
// Editor.cpp
//
// Implementation of ScnEditor class and editor tab subclasses
// derived from ScnEditorTab.
// ==============================================================

#include "Orbitersdk.h"
#include "Editor.h"
#include "font_awesome_5.h"
#include <stdio.h>
#include <imgui.h>
#include <imgui-knobs.h>
#include <filesystem>
namespace fs = std::filesystem;

extern ScnEditor *g_editor;

// ==============================================================
// Local prototypes
// ==============================================================

void OpenDialog (void *context);
void Crt2Pol (VECTOR3 &pos, VECTOR3 &vel);
void Pol2Crt (VECTOR3 &pos, VECTOR3 &vel);

static double lengthscale[4] = {1.0, 1e-3, 1.0/AU, 1.0};
static double anglescale[2] = {DEG, 1.0};

// ==============================================================
// ScnEditor class definition
// ==============================================================

ScnEditor::ScnEditor (): GUIElement("Scenario Editor", "ScnEditor")
{
	hEdLib = nullptr;
	dwCmd = oapiRegisterCustomCmd (
		"Scenario Editor",
		"Create, delete and configure spacecraft",
		::OpenDialog, this);

	m_preview = nullptr;
	m_currentVessel = nullptr;
	oe.frm = FRAME_ECL;
	vecState.frm = 0;
	vecState.crd = 0;
	memset(m_newVesselName, 0 , sizeof(m_newVesselName));
	aRot = {0,0,0};
	aVel = {0,0,0};
	m_customTabs = nullptr;
}

ScnEditor::~ScnEditor ()
{
	CloseDialog();
	oapiUnregisterCustomCmd (dwCmd);
}

void ScnEditor::OpenDialog ()
{
	oapiOpenDialog (this);
}

void ScnEditor::CloseDialog ()
{
	oapiCloseDialog (this);

	if (hEdLib) {
		oapiModuleUnload (hEdLib);
		hEdLib = nullptr;
		m_customTabs = nullptr;
	}
}

void ScnEditor::DrawConfigs(const char *path) {
    std::vector<fs::path> directories;
    for (auto &file : fs::directory_iterator(path)) {
        if(file.path() != "." && file.path() != "..") {
            if (file.is_directory()) {
                directories.push_back(file.path());
            }
        }
    }

    std::sort(directories.begin(), directories.end());//, alphanum_sort);
    for(auto &p: directories) {
        const bool is_selected = m_SelectedDirectory == p;
        ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow;//ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if(is_selected) node_flags |= ImGuiTreeNodeFlags_Selected;
        
        bool clicked = false;
        if(ImGui::TreeNodeEx(p.stem().c_str(), node_flags)) {
            clicked = ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen();
            DrawConfigs(p.c_str());
            ImGui::TreePop();
        } else {
            clicked = ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen();
        }
        if (clicked) {
            m_SelectedConfig.clear();
            m_SelectedDirectory = p;
        }
    }

    std::vector<fs::path> configs;
    for (auto &file : fs::directory_iterator(path)) {
        if(file.path().extension() == ".cfg") {
            if (file.is_regular_file()) {
				bool skip = true;
				FILEHANDLE hFile = oapiOpenFile (file.path().c_str(), FILE_IN);
				if (hFile) {
					bool b;
					skip = (oapiReadItem_bool (hFile, "EditorCreate", b) && !b);
					oapiCloseFile (hFile, FILE_IN);
				}
				if (skip) continue;

                configs.push_back(file.path());
            }
        }
    }

    std::sort(configs.begin(), configs.end());//, alphanum_sort);
    for(auto &p: configs) {
        const bool is_selected = m_SelectedConfig == p;
        ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if(is_selected) node_flags |= ImGuiTreeNodeFlags_Selected;
        char node_text[256];
        sprintf(node_text, ICON_FA_PAPER_PLANE" %s", p.stem().c_str());
        ImGui::TreeNodeEx(node_text, node_flags);
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            m_SelectedConfig = p;
            m_SelectedDirectory.clear();

			FILEHANDLE hFile = oapiOpenFile (p.c_str(), FILE_IN);
			if (hFile) {
				char imagename[256];
				if (oapiReadItem_string (hFile, "ImageBmp", imagename)) {
					if(m_preview) {
						oapiReleaseTexture(m_preview);
						m_preview = nullptr;
					}
					m_preview = oapiLoadTexture(imagename);
				}
				if (oapiReadItem_string (hFile, "ClassName", imagename)) {
					m_classname = imagename;
				} else {
					m_classname = p.stem();
				}
				oapiCloseFile (hFile, FILE_IN);
			}
        }
    }
}

void ScnEditor::CreateVessel() {
	if (oapiGetVesselByName (m_newVesselName)) {
		oapiAddNotification(OAPINOTIF_ERROR, "Cannot create new vessel", "Vessel name already in use");
		return;
	}

	// define an arbitrary status (the user will have to edit this after creation)
	VESSELSTATUS2 vs;
	memset (&vs, 0, sizeof(vs));
	vs.version = 2;
	vs.rbody = oapiGetGbodyByName ("Earth");
	if (!vs.rbody) vs.rbody = oapiGetGbodyByIndex (0);
	double rad = 1.1 * oapiGetSize (vs.rbody);
	double vel = sqrt (GGRAV * oapiGetMass (vs.rbody) / rad);
	vs.rpos = _V(rad,0,0);
	vs.rvel = _V(0,0,vel);
	m_currentVessel = oapiCreateVesselEx (m_newVesselName, m_classname.c_str(), &vs);
	oapiSetFocusObject (m_currentVessel);
//	if (SendDlgItemMessage (hTab, IDC_CAMERA, BM_GETSTATE, 0, 0) == BST_CHECKED)
	oapiCameraAttach (m_currentVessel, 1);
	m_newVesselName[0] = '\0';
}

void ScnEditor::VesselCreatePopup() {
	ImGui::Text("New vessel");
	ImGui::InputText("Name", m_newVesselName, IM_ARRAYSIZE(m_newVesselName));

    ImGui::BeginChild("ChildL", ImVec2(ImGui::GetContentRegionAvail().x / 2, ImGui::GetContentRegionAvail().y - 30), true);
	DrawConfigs("Config/Vessels");
    ImGui::EndChild();
	ImGui::SameLine();
    ImGui::BeginChild("ChildR", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - 30), true);
	if(m_preview) {
        const ImVec2 uv_min = ImVec2(0.0f, 0.0f);                 // Top-left
        const ImVec2 uv_max = ImVec2(1.0f, 1.0f);                 // Lower-right
        const ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
        const ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // 50% opaque white
		int w,h;
        oapiIncrTextureRef(m_preview);
		oapiGetTextureSize(m_preview, &w, &h);
		float ratio = (float)h/(float)w;
        ImGui::ImageButton(m_preview, ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x * ratio), uv_min, uv_max, 0, tint_col, border_col);
	}
    ImGui::EndChild();

	const bool ready = strlen(m_newVesselName) != 0 && !m_SelectedConfig.empty();

	if(!ready)
		ImGui::BeginDisabled();

	if(ImGui::Button("Create")) {
		CreateVessel();
		ImGui::CloseCurrentPopup();
	}
	if(!ready)
		ImGui::EndDisabled();

	ImGui::SameLine();
	if(ImGui::Button("Cancel")) {
		ImGui::CloseCurrentPopup();
	}
}

bool ScnEditor::DrawShipList(OBJHANDLE &selected)
{
	bool ret = false;
	if (ImGui::TreeNodeEx("Vessels", ImGuiTreeNodeFlags_DefaultOpen)) {
		for (int i = 0; i < oapiGetVesselCount(); i++) {
			OBJHANDLE hV = oapiGetVesselByIndex (i);
			VESSEL *vessel = oapiGetVesselInterface (hV);
			const char *name = vessel->GetName();
			const bool is_selected = selected == hV;
			
			ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if(is_selected) node_flags |= ImGuiTreeNodeFlags_Selected;
			ImGui::TreeNodeEx(name, node_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
				selected = hV;
				ReloadVessel();
				ret = true;
				//Don't do this for now because it can create lots of controller profiles :(
				//oapiSetFocusObject (m_currentVessel);
				//oapiCameraAttach (m_currentVessel, 1);
			}
		}
		ImGui::TreePop();
	}
	return ret;
}

void ScnEditor::DrawTabs ()
{
	const char *tabs[] = {
		ICON_FA_SATELLITE" Orbital elements", ICON_FA_LOCATION_ARROW" State vectors", ICON_FA_COMPASS" Rotation",
		ICON_FA_MAP_MARKER_ALT" Location", ICON_FA_LIFE_RING" Docking", ICON_FA_GAS_PUMP" Propellant"
	};

	void (ScnEditor::* func[])() = {
		&ScnEditor::DrawOrbitalElements, &ScnEditor::DrawStateVectors, &ScnEditor::DrawRotation,
		&ScnEditor::DrawLocation, &ScnEditor::DrawDocking, &ScnEditor::DrawPropellant
	};


	ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_FittingPolicyScroll;
	if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
	{
		for(size_t i = 0; i<sizeof(tabs)/sizeof(tabs[0]);i++) {
			if(ImGui::BeginTabItem(tabs[i])) {
				(this->*func[i])();
				ImGui::EndTabItem();
			}
		}
		if(m_customTabs && m_currentVessel)
			m_customTabs(m_currentVessel);

		ImGui::EndTabBar();
	}
}

void ScnEditor::ReloadVessel()
{
	VESSEL *vessel = oapiGetVesselInterface (m_currentVessel);
	OBJHANDLE hRef = vessel->GetGravityRef();// oapiGetGbodyByName (m_selectedReference.c_str());

	char cbuf[64];
	oapiGetObjectName (hRef, cbuf, 64);
	oe.m_selectedReference = cbuf;
	vecState.m_selectedReference = cbuf;
	loc.planet = cbuf;
	vessel->GetElements(hRef, oe.el, &oe.prm, oe.elmjd, oe.frm);
	
	oapiGetRelativePos (m_currentVessel, hRef, &vecState.pos);
	oapiGetRelativeVel (m_currentVessel, hRef, &vecState.vel);

	oapiGetPlanetObliquityMatrix (hRef, &vecState.rotFixed);
	oapiGetRotationMatrix (hRef, &vecState.rotRotating);
	vessel->GetGlobalOrientation (aRot);
	aRot.x*=DEG;
	aRot.y*=DEG;
	aRot.z*=DEG;

	vessel->GetAngularVel (aVel);
	aVel.x*=DEG;
	aVel.y*=DEG;
	aVel.z*=DEG;

	LoadVesselLibrary(vessel);
}

bool ScnEditor::DrawCBodies(std::string &ref, const char *name) {
	bool ret = false;
	std::vector<std::string> bodies;
	for (int n = 0; n < oapiGetGbodyCount(); n++) {
		char cbuf[256];
		oapiGetObjectName (oapiGetGbodyByIndex (n), cbuf, 256);
		bodies.push_back(cbuf);
	}
	if(bodies.empty()) return false;
	std::sort(bodies.begin(), bodies.end());
	if(ref.empty()) ref = bodies[0];

	ImGui::PushItemWidth(100);
	if(ImGui::BeginCombo(name, ref.c_str())) {
		for (auto &body: bodies) {
			if (ImGui::Selectable(body.c_str(), body == ref)) {
				ref = body;
				ret = true;
			}
		}
        ImGui::EndCombo();
	}
	ImGui::PopItemWidth();
	return ret;
}

void ScnEditor::DrawBases(OBJHANDLE hPlanet, std::string &ref) {
	std::vector<std::string> bases;
	for (int n = 0; n < oapiGetBaseCount(hPlanet); n++) {
		char cbuf[256];
		oapiGetObjectName (oapiGetBaseByIndex (hPlanet, n), cbuf, 256);
		bases.push_back(cbuf);
	}
	if(bases.empty()) return;
	std::sort(bases.begin(), bases.end());
	if(ref.empty()) ref = bases[0];

	ImGui::BeginGroupPanel("Surface bases");
	ImGui::PushItemWidth(150);
	if(ImGui::BeginCombo("##surfacebase", ref.c_str())) {
		for (auto &body: bases) {
			if (ImGui::Selectable(body.c_str(), body == ref)) {
				ref = body;
				loc.pad.clear();
			}
		}
        ImGui::EndCombo();
	}
	ImGui::PopItemWidth();
	if(!loc.base.empty()) {
		OBJHANDLE hBase = oapiGetBaseByName(hPlanet, loc.base.c_str());
		if(hBase) {
			DrawPads(hBase, loc.pad);
		}
	}

	ImGui::SameLine();
	if(ImGui::Button("Preset position")) {
		if(!loc.base.empty()) {
			OBJHANDLE hPlanet = oapiGetObjectByName(loc.planet.c_str());
			OBJHANDLE hBase = oapiGetBaseByName(hPlanet, loc.base.c_str());
			if (!loc.pad.empty())
				oapiGetBasePadEquPos (hBase, std::stoi(loc.pad)-1, &loc.longitude, &loc.latitude);
			else
				oapiGetBaseEquPos (hBase, &loc.longitude, &loc.latitude);
			loc.longitude *= DEG;
			loc.latitude *= DEG;
		}
	}

	ImGui::EndGroupPanel();
}

bool ScnEditor::DrawPads(OBJHANDLE hBase, std::string &ref) {
	bool ret = false;
	int n, npad = oapiGetBasePadCount (hBase);
	if(ref.empty()) ref = "1";
	if(npad) {
		ImGui::SameLine();
		ImGui::Text("Pad : ");
		ImGui::SameLine();
		ImGui::PushItemWidth(100);
		if(ImGui::BeginCombo("##Pads", ref.c_str())) {
			char cbuf[16];
			for (n = 1; n <= npad; n++) {
				sprintf (cbuf, "%d", n);
				if (ImGui::Selectable(cbuf, cbuf == ref)) {
					ref = cbuf;
					ret = true;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
	}
	return ret;
}


void ScnEditor::DrawOrbitalElements()
{
	if(!m_currentVessel) return;
	ImGui::BeginGroupPanel("Reference");
		ImGui::BeginGroupPanel("Orbit", ImVec2(ImGui::GetContentRegionAvail().x * 0.33f, 0));
		DrawCBodies(oe.m_selectedReference, "##Orbit");
		ImGui::EndGroupPanel();
		ImGui::SameLine();
		ImGui::BeginGroupPanel("Frame", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0));
		bool frmChanged = ImGui::RadioButton("Ecliptic", &oe.frm, 0); ImGui::SameLine();
		frmChanged     |= ImGui::RadioButton("Equatorial", &oe.frm, 1);
		if(frmChanged) ReloadVessel();
		ImGui::EndGroupPanel();
		ImGui::SameLine();
		ImGui::BeginGroupPanel("Epoch");
		ImGui::PushItemWidth(100);
		ImGui::InputDouble("MJD", &oe.elmjd, 0.0, 0.0, "%g");
		ImGui::PopItemWidth();
		ImGui::EndGroupPanel();

	ImGui::EndGroupPanel();

	static int smaUnit;
	VESSEL *vessel = oapiGetVesselInterface (m_currentVessel);
	OBJHANDLE hRef = vessel->GetGravityRef();
	lengthscale[3] = 1.0/oapiGetSize (hRef);

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
	ImGui::BeginGroupPanel("Osculating elements", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0));
		ImGui::BeginGroupPanel("Unit");
		ImGui::RadioButton("m", &smaUnit,0); ImGui::SameLine();
		ImGui::RadioButton("km", &smaUnit,1); ImGui::SameLine();
		ImGui::RadioButton("AU", &smaUnit,2); ImGui::SameLine();
		ImGui::RadioButton("Planet radius", &smaUnit,3);
		ImGui::EndGroupPanel();

		ImGui::PushItemWidth(100);
		oe.el.a*=lengthscale[smaUnit];
		ImGui::InputDouble("Semi-major Axis [SMa]", &oe.el.a, 0.0, 0.0, "%g");
		oe.el.a/=lengthscale[smaUnit];
		ImGui::InputDouble("Eccentricity [Ecc]", &oe.el.e, 0.0, 0.0, "%g");
		oe.el.i*=DEG;
		ImGui::InputDouble("Inclination [Inc]°", &oe.el.i, 0.0, 0.0, "%g");
		oe.el.i/=DEG;
		oe.el.theta*=DEG;
		ImGui::InputDouble("Longitude of ascending node [LAN]°", &oe.el.theta, 0.0, 0.0, "%g");
		oe.el.theta/=DEG;
		oe.el.omegab*=DEG;
		ImGui::InputDouble("Longitude of periapsis [LPe]°", &oe.el.omegab, 0.0, 0.0, "%g");
		oe.el.omegab/=DEG;
		oe.el.L*=DEG;
		ImGui::InputDouble("Mean longitude at epoch [eps]°", &oe.el.L, 0.0, 0.0, "%g");
		oe.el.L/=DEG;
		ImGui::PopItemWidth();
	ImGui::EndGroupPanel();
	ImGui::SameLine();
	ImGui::BeginGroupPanel("Secondary parameters", ImVec2(ImGui::GetContentRegionAvail().x, 0));
		bool closed = (oe.el.e < 1.0); // closed orbit?

		ImGui::Text ("Periapsis : %g m", oe.prm.PeD);
		ImGui::Text ("PeT : %g s", oe.prm.PeT);
		ImGui::Text ("MnA : %0.3f °", oe.prm.MnA*DEG);
		ImGui::Text ("TrA : %0.3f °", oe.prm.TrA*DEG);
		ImGui::Text ("MnL : %0.3f °", oe.prm.MnL*DEG);
		ImGui::Text ("TrL : %0.3f °", oe.prm.TrL*DEG);
		if (closed) {
			ImGui::Text ("Period : %g s", oe.prm.T);
			ImGui::Text ("Apoapsis : %g m", oe.prm.ApD);
			ImGui::Text ("ApT : %g s", oe.prm.ApT);
		}
	ImGui::EndGroupPanel();

	if(ImGui::Button("Apply")) {
		OBJHANDLE hRef = oapiGetGbodyByName (oe.m_selectedReference.c_str());
		if (hRef) {
			oe.el.a = fabs (oe.el.a);
			if (oe.el.e > 1.0) oe.el.a = -oe.el.a;
			VESSEL *vessel = oapiGetVesselInterface (m_currentVessel);

			if (!vessel->SetElements (hRef, oe.el, &oe.prm, oe.elmjd, oe.frm)) {
				oapiAddNotification(OAPINOTIF_ERROR, "Failed to set orbital elements", "Trajectory is inside the body");
			}
		}
	}
}
void ScnEditor::DrawStateVectors()
{
	if(!m_currentVessel) return;
	ImGui::BeginGroupPanel("Reference");
		ImGui::BeginGroupPanel("Orbit", ImVec2(ImGui::GetContentRegionAvail().x * 0.25f, 0));
		DrawCBodies(vecState.m_selectedReference, "##Orbit");
		ImGui::EndGroupPanel();

		ImGui::SameLine();
		ImGui::BeginGroupPanel("Frame", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0));
		ImGui::RadioButton("Ecliptic", &vecState.frm ,0);
		ImGui::SameLine();
		ImGui::RadioButton("Equator (fixed)", &vecState.frm ,1);
		ImGui::SameLine();
		ImGui::RadioButton("Equator (rotating)", &vecState.frm ,2);
		ImGui::EndGroupPanel();

		ImGui::SameLine();
		ImGui::BeginGroupPanel("Coordinates");	
		ImGui::RadioButton("Cartesian", &vecState.crd, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Polar", &vecState.crd, 1);
		ImGui::EndGroupPanel();
	ImGui::EndGroupPanel();

	VESSEL *vessel = oapiGetVesselInterface (m_currentVessel);
	OBJHANDLE hRef = oapiGetObjectByName(vecState.m_selectedReference.c_str());

	VECTOR3 pos = vecState.pos, vel = vecState.vel;
	if (vecState.frm) {
		MATRIX3 rot;
		if (vecState.frm == 1) rot = vecState.rotFixed;
		else          rot = vecState.rotRotating;
		pos = tmul (rot, pos);
		vel = tmul (rot, vel);
	}
	// map cartesian -> polar coordinates
	if (vecState.crd) {
		Crt2Pol (pos, vel);
		pos.data[1] *= DEG; pos.data[2] *= DEG;
		vel.data[1] *= DEG; vel.data[2] *= DEG;
	}
	// in the rotating reference frame we need to subtract the angular
	// velocity of the planet
	if (vecState.frm == 2) {
		double T = oapiGetPlanetPeriod (hRef);
		if (vecState.crd) {
			vel.data[1] -= 360.0/T;
		} else { // map back to cartesian
			double r   = std::hypot (pos.x, pos.z);
			double phi = atan2 (pos.z, pos.x);
			double v   = 2.0*PI*r/T;
			vel.x     += v*sin(phi);
			vel.z     -= v*cos(phi);
		}
	}
	if(vecState.crd) {
		//Polar
		ImGui::BeginGroupPanel("Position", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0));
		ImGui::PushItemWidth(100);
		ImGui::InputDouble("Radius (m)", &pos.x, 0.0, 0.0, "%g");
		ImGui::InputDouble("Longitude (°)", &pos.y, 0.0, 0.0, "%g");
		ImGui::InputDouble("Latitude (°)", &pos.z, 0.0, 0.0, "%g");
		ImGui::PopItemWidth();
		ImGui::EndGroupPanel();
		ImGui::SameLine();
		ImGui::BeginGroupPanel("Velocity");
		ImGui::PushItemWidth(100);
		ImGui::InputDouble("dRadius/dt (m/s)", &vel.x, 0.0, 0.0, "%g");
		ImGui::InputDouble("dLongitude/dt (°/s)", &vel.y, 0.0, 0.0, "%g");
		ImGui::InputDouble("dLatitude/dt (°/s)", &vel.z, 0.0, 0.0, "%g");
		ImGui::PopItemWidth();
		ImGui::EndGroupPanel();
	} else {
		//Cartesian
		ImGui::BeginGroupPanel("Position", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0));
		ImGui::PushItemWidth(100);
		ImGui::InputDouble("x (m)", &pos.x, 0.0, 0.0, "%g");
		ImGui::InputDouble("y (m)", &pos.y, 0.0, 0.0, "%g");
		ImGui::InputDouble("z (m)", &pos.z, 0.0, 0.0, "%g");
		ImGui::PopItemWidth();
		ImGui::EndGroupPanel();
		ImGui::SameLine();
		ImGui::BeginGroupPanel("Velocity");
		ImGui::PushItemWidth(100);
		ImGui::InputDouble("dx/dt (m/s)", &vel.x, 0.0, 0.0, "%g");
		ImGui::InputDouble("dy/dt (m/s)", &vel.y, 0.0, 0.0, "%g");
		ImGui::InputDouble("dz/dt (m/s)", &vel.z, 0.0, 0.0, "%g");
		ImGui::PopItemWidth();
		ImGui::EndGroupPanel();
	}

	// in the rotating reference frame we need to add the angular
	// velocity of the planet
	MATRIX3 rot;
	VECTOR3 refpos, refvel;
	VESSELSTATUS vs;
	if (vecState.frm == 2) {
		double T = oapiGetPlanetPeriod (hRef);
		if (vecState.crd) {
			vel.data[1] += 360.0/T;
		} else { // map back to cartesian
			double r   = std::hypot (pos.x, pos.z);
			double phi = atan2 (pos.z, pos.x);
			double v   = 2.0*PI*r/T;
			vel.x     -= v*sin(phi);
			vel.z     += v*cos(phi);
		}
	}
	// map polar -> cartesian coordinates
	if (vecState.crd) {
		pos.data[1] *= RAD, pos.data[2] *= RAD;
		vel.data[1] *= RAD, vel.data[2] *= RAD;
		Pol2Crt (pos, vel);
	}
	// map from celestial/equatorial frame of reference
	if (vecState.frm) {
		if (vecState.frm == 1) rot = vecState.rotFixed;
		else          rot = vecState.rotRotating;
		pos = mul (rot, pos);
		vel = mul (rot, vel);
	}
	vecState.pos = pos;
	vecState.vel = vel;
	if(ImGui::Button("Apply")) {
		// change reference in case the selected reference object is
		// not the same as the VESSELSTATUS reference
		vessel->GetStatus (vs);
		oapiGetGlobalPos (hRef, &refpos);     pos += refpos;
		oapiGetGlobalVel (hRef, &refvel);     vel += refvel;
		oapiGetGlobalPos (vs.rbody, &refpos); pos -= refpos;
		oapiGetGlobalVel (vs.rbody, &refvel); vel -= refvel;
		if (vs.status != 0) { // enforce freeflight mode
			vs.status = 0;
			vessel->GetRotationMatrix (rot);
			vs.arot.x = atan2(rot.m23, rot.m33);
			vs.arot.y = -asin(rot.m13);
			vs.arot.z = atan2(rot.m12, rot.m11);
			vessel->GetAngularVel(vs.vrot);
		}
		veccpy (vs.rpos, pos);
		veccpy (vs.rvel, vel);
		vessel->DefSetState (&vs);
	}
	ImGui::SameLine();
	if(ImGui::Button("Set from...")) {
		ImGui::OpenPopup("CopyVesselSV");
	}
	if (ImGui::BeginPopup("CopyVesselSV"))
	{
		OBJHANDLE h;
		if(DrawShipList(h)) {
			VESSELSTATUS2 vsSrc;
			VESSELSTATUS2 vsDst;
			memset(&vsDst, 0, sizeof(vsDst));
			memset(&vsSrc, 0, sizeof(vsSrc));
			vsSrc.version = 2;
			vsDst.version = 2;
			VESSEL *vesselSrc = oapiGetVesselInterface (h);
			vesselSrc->GetStatusEx(&vsSrc);
			vessel->GetStatusEx(&vsDst);

			vsDst.arot = vsSrc.arot;
			vsDst.rbody = vsSrc.rbody;
			vsDst.rpos = vsSrc.rpos;
			vsDst.rvel = vsSrc.rvel;
			vsDst.status = vsSrc.status;
			vsDst.surf_hdg = vsSrc.surf_hdg;
			vsDst.surf_lat = vsSrc.surf_lat;
			vsDst.surf_lng = vsSrc.surf_lng;
			vsDst.vrot = vsSrc.vrot;

			vessel->DefSetStateEx (&vsDst);
			ReloadVessel();

			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}
/*
void ScnEditor::DrawOrientation()
{
	if(!m_currentVessel) return;
	ImGui::BeginGroupPanel("Euler angles");
	ImGui::InputDouble("alpha", &aRot.x, 0.0, 0.0, "%g");
	ImGui::InputDouble("beta", &aRot.y, 0.0, 0.0, "%g");
	ImGui::InputDouble("gamma", &aRot.z, 0.0, 0.0, "%g");
	ImGui::EndGroupPanel();

	if(ImGui::Button("Apply")) {
		VESSEL *vessel = oapiGetVesselInterface (m_currentVessel);
		VECTOR3 arot = aRot;
		arot.x*=RAD;
		arot.y*=RAD;
		arot.z*=RAD;
		vessel->SetGlobalOrientation (arot);
	}
}*/
void ScnEditor::DrawRotation()
{
	if(!m_currentVessel) return;
	ImGui::PushItemWidth(100);
	ImGui::BeginGroupPanel("Euler angles", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0));
	ImGui::InputDouble("alpha", &aRot.x, 0.0, 0.0, "%g");
	ImGui::InputDouble("beta", &aRot.y, 0.0, 0.0, "%g");
	ImGui::InputDouble("gamma", &aRot.z, 0.0, 0.0, "%g");
	ImGui::EndGroupPanel();
	ImGui::SameLine();
	ImGui::BeginGroupPanel("Angular velocity");
	ImGui::InputDouble("Pitch (°/s)", &aVel.x, 0.0, 0.0, "%g");
	ImGui::InputDouble("Yaw (°/s)", &aVel.y, 0.0, 0.0, "%g");
	ImGui::InputDouble("Bank (°/s)", &aVel.z, 0.0, 0.0, "%g");
	ImGui::EndGroupPanel();
	ImGui::PopItemWidth();

	if(ImGui::Button("Apply")) {
		VESSEL *vessel = oapiGetVesselInterface (m_currentVessel);
		VECTOR3 avel = aVel;
		VECTOR3 arot = aRot;
		avel.x*=RAD;
		avel.y*=RAD;
		avel.z*=RAD;
		vessel->SetAngularVel (avel);

		arot.x*=RAD;
		arot.y*=RAD;
		arot.z*=RAD;
		vessel->SetGlobalOrientation (arot);
	}
	ImGui::SameLine();
	if(ImGui::Button("Kill rotation")) {
		VECTOR3 avel{0,0,0};
		VESSEL *vessel = oapiGetVesselInterface (m_currentVessel);
		vessel->SetAngularVel (avel);
	}
}
void ScnEditor::DrawLocation()
{
	ImGui::BeginGroupPanel("Celestial body");
	if(DrawCBodies(loc.planet, "##Celestial body")) {
		loc.base.clear();
	}
	ImGui::EndGroupPanel();
	if(!loc.planet.empty()) {
		OBJHANDLE hPlanet = oapiGetObjectByName(loc.planet.c_str());
		DrawBases(hPlanet, loc.base);
	}

	ImGui::BeginGroupPanel("Position");
	ImGui::InputDouble("Longitude", &loc.longitude, 0.0, 0.0, "%f");
	ImGui::InputDouble("Latitude", &loc.latitude, 0.0, 0.0, "%f");
	ImGui::InputDouble("Heading (°)", &loc.heading, 0.0, 0.0, "%f");
	ImGui::EndGroupPanel();
	if(ImGui::Button("Apply")) {
		char cbuf[256];
		VESSEL *vessel = oapiGetVesselInterface (m_currentVessel);
		OBJHANDLE hRef = oapiGetGbodyByName (loc.planet.c_str());
		if (!hRef) return;
		VESSELSTATUS2 vs;
		memset (&vs, 0, sizeof(vs));
		vs.version = 2;
		vs.rbody = hRef;
		vs.status = 1; // landed
		vs.arot.x = 10; // use default touchdown orientation
		vs.surf_lng = loc.longitude * RAD;
		vs.surf_lat = loc.latitude * RAD;
		vs.surf_hdg = loc.heading * RAD;
		vessel->DefSetStateEx (&vs);
	}
}
void ScnEditor::DrawDocking()
{
	if(!m_currentVessel) return;
	VESSEL *vessel = oapiGetVesselInterface (m_currentVessel);
	int ndock = vessel->DockCount();
	if(ndock == 0) {
		ImGui::TextUnformatted("Vessel has no docking ports");
		return;
	}

	static int mode = 1;
	ImGui::RadioButton("Teleport the target vessel", &mode, 1);
	ImGui::SameLine();
	ImGui::RadioButton("Teleport the current vessel", &mode, 2);

	if (ImGui::BeginTable("Docking Ports", 5, ImGuiTableFlags_Borders))
	{
		ImGui::TableSetupColumn("Port");
		ImGui::TableSetupColumn("IDS enabled");
		ImGui::TableSetupColumn("Frequency");
		ImGui::TableSetupColumn("Status");
		ImGui::TableSetupColumn("Action");
		ImGui::TableHeadersRow();
		for(int i = 0; i < ndock; i++) {
			ImGui::PushID(i);
			DOCKHANDLE hDock = vessel->GetDockHandle (i);
			NAVHANDLE hIDS = vessel->GetIDS (hDock);
			float freq = -1;
			if (hIDS) {
				freq = oapiGetNavFreq (hIDS);
			}
			bool b = freq != -1;
			ImGui::TableNextColumn(); ImGui::Text("%d", i + 1);
			ImGui::TableNextColumn(); 
			if(ImGui::Checkbox("##IDS", &b)) {
				vessel->EnableIDS(hDock, b);
			}
			if (hIDS) {
				ImGui::TableNextColumn();
				if(ImGui::InputFloat("##Frequency", &freq, 0, 0, "%.2f")) {
					int ch = (int)((freq-108.0)*20.0+0.5);
					int dch  = 0;
					ch = std::max(0, std::min (639, ch+dch));

					vessel->SetIDSChannel(hDock, ch);
				}
			} else {
				ImGui::TableNextColumn(); ImGui::Text("<none>");
			}

			OBJHANDLE hMate = vessel->GetDockStatus (hDock);
			if (hMate) { // dock is engaged
				char cbuf[128];
				oapiGetObjectName (hMate, cbuf, sizeof(cbuf));
				ImGui::TableNextColumn(); ImGui::Text("Docked with %s", cbuf);
				ImGui::TableNextColumn();
				if(ImGui::Button(ICON_FA_UNLINK" Undock")) {
					vessel->Undock (i);
				}
			} else {
				ImGui::TableNextColumn(); ImGui::Text("Free");
				ImGui::TableNextColumn();
				if(ImGui::Button(ICON_FA_LINK" Dock with...")) {
					ImGui::OpenPopup("DockWith");
				}
				if (ImGui::BeginPopup("DockWith"))
				{
					if (ImGui::TreeNodeEx("Vessels", ImGuiTreeNodeFlags_DefaultOpen)) {
						for (int v = 0; v < oapiGetVesselCount(); v++) {
							OBJHANDLE hV = oapiGetVesselByIndex (v);
							if(hV == m_currentVessel) continue;
							VESSEL *vesselTgt = oapiGetVesselInterface (hV);
							int ndock = vesselTgt->DockCount();
							if(ndock == 0) continue;
							const char *name = vesselTgt->GetName();

							if(ImGui::TreeNodeEx(name)) {
								for(int d = 0; d < ndock ; d++) {
									char cbuf[16];
									sprintf(cbuf,"Port %d",d+1);
									ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
									DOCKHANDLE hDock = vesselTgt->GetDockHandle (d);
									OBJHANDLE hMate = vesselTgt->GetDockStatus (hDock);
									if(hMate) ImGui::BeginDisabled();
									ImGui::TreeNodeEx(cbuf, node_flags);
									if(hMate) ImGui::EndDisabled();
									if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
										int res = vessel->Dock (hV, i, d, mode);
										switch(res) {
											case 1:
												oapiAddNotification(OAPINOTIF_ERROR, "Docking failed","Docking port on the vessel already in use");
												break;
											case 2:
												oapiAddNotification(OAPINOTIF_ERROR, "Docking failed","Docking port on the target vessel already in use");
												break;
											case 3:
												oapiAddNotification(OAPINOTIF_ERROR, "Docking failed","Target vessel already part of the vessel's superstructure");
												break;
											default:
												break;
										}
										ImGui::CloseCurrentPopup();
									}
								}
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}
					ImGui::EndPopup();
				}
			}
			ImGui::TableNextRow();
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
}
void ScnEditor::DrawPropellant()
{
	if(!m_currentVessel) return;
	VESSEL *vessel = oapiGetVesselInterface (m_currentVessel);
	int ntank = ntank = vessel->GetPropellantCount();
	if(ntank == 0) {
		ImGui::TextUnformatted("Vessel has no fuel tanks");
		return;
	}
	if (ImGui::BeginTable("Fuel tanks", ntank, ImGuiTableFlags_Borders, ImVec2(0.0f, 0.0f), 60.0f))
	{
		for(int tank = 0; tank < ntank; tank++) {
			PROPELLANT_HANDLE hP = vessel->GetPropellantHandleByIndex (tank);
			double m0 = vessel->GetPropellantMaxMass (hP);
			double m  = vessel->GetPropellantMass (hP);

			ImGui::TableNextColumn();
			if(m0 == 0.0) continue;

			float ratio = m / m0 * 100.0f;

			char cbuf[32];
			sprintf(cbuf, "Tank %d", tank + 1);

			if (ImGuiKnobs::Knob(cbuf, &ratio, 0.0f, 100.0f, 1.0f, "%.2f%%", ImGuiKnobVariant_WiperOnly, 50.0, ImGuiKnobFlags_DragHorizontal)) {
				// value was changed
				m = m0 * ratio / 100.0;
				vessel->SetPropellantMass(hP, m);
			}
		}
		ImGui::TableNextRow();
		for(int tank = 0; tank < ntank; tank++) {
			ImGui::TableNextColumn();
			PROPELLANT_HANDLE hP = vessel->GetPropellantHandleByIndex (tank);
			double m0 = vessel->GetPropellantMaxMass (hP);
			if(m0 == 0.0) continue;
			double m  = vessel->GetPropellantMass (hP);

			ImGui::SetNextItemWidth(60);
			ImGui::PushID(tank);
			if(ImGui::InputDouble("##fuelmass", &m, 0.0, 0.0, "%.1f")) {
				m = std::clamp(m, 0.0, m0);
				vessel->SetPropellantMass(hP, m);
			}
			ImGui::PopID();
			ImGui::SameLine();
			ImGui::Text("/%.1fkg", m0);
		}
		ImGui::EndTable();
	}
	/*
void EditorTab_Propellant::SetLevel (double level, bool setall)
{
	char cbuf[256];
	int i, j;
	DWORD n, k, k0, k1;
	double m0;
	VESSEL *vessel = oapiGetVesselInterface (ed->hVessel);
	ntank = vessel->GetPropellantCount();
	if (!ntank) return;
	GetWindowText (GetDlgItem (hTab, IDC_EDIT1), cbuf, 256);
	i = sscanf (cbuf, "%d", &n);
	if (!i || --n >= ntank) return;
	if (setall) k0 = 0, k1 = ntank;
	else        k0 = n, k1 = n+1;
	for (k = k0; k < k1; k++) {
		PROPELLANT_HANDLE hP = vessel->GetPropellantHandleByIndex (k);
		m0 = vessel->GetPropellantMaxMass (hP);
		vessel->SetPropellantMass (hP, level*m0);
		if (k == n) {
			sprintf (cbuf, "%f", level);
			SetWindowText (GetDlgItem (hTab, IDC_EDIT2), cbuf);
			sprintf (cbuf, "%0.2f", level*m0);
			SetWindowText (GetDlgItem (hTab, IDC_EDIT3), cbuf);
			i = oapiGetGaugePos (GetDlgItem (hTab, IDC_PROPLEVEL));
			j = (int)(level*100.0+0.5);
			if (i != j) oapiSetGaugePos (GetDlgItem (hTab, IDC_PROPLEVEL), j);
		}
	}
	RefreshTotals();
}
	*/
}

void ScnEditor::Show ()
{
    if(!show) return;
	if(ImGui::Begin("Scenario Editor", &show)) {
	    ImGui::BeginChild("ChildL", ImVec2(ImGui::GetContentRegionAvail().x / 3, ImGui::GetContentRegionAvail().y), true);
		if(ImGui::Button("New vessel")) {
			ImGui::OpenPopup("NewVessel");
		}
		ImGui::SameLine();
		const bool disabled = !m_currentVessel;
		if(disabled) ImGui::BeginDisabled();
		if(ImGui::Button("Delete selected")) {
			if(oapiGetVesselCount()>1) {
				oapiDeleteVessel (m_currentVessel);
				m_currentVessel = nullptr;
			} else {
				oapiAddNotification(OAPINOTIF_ERROR, "Cannot delete vessel", "One vessel at least must exists");
			}
		}
		ImGui::SameLine();
		if(ImGui::Button("Focus")) {
			oapiSetFocusObject (m_currentVessel);
			oapiCameraAttach (m_currentVessel, 1);
		}
		ImGui::SameLine();
		if(ImGui::Button("Refresh")) {
			ReloadVessel();
		}
		if(disabled) ImGui::EndDisabled();
		if(ImGui::Button("Date")) {
			
		}
		ImGui::SetNextWindowSize({500,500});
		if (ImGui::BeginPopup("NewVessel"))
		{
			VesselCreatePopup();
			ImGui::EndPopup();
		}
		ImGui::Separator();

		DrawShipList(m_currentVessel);
	    ImGui::EndChild();
		ImGui::SameLine();
	    ImGui::BeginChild("ChildR", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y), true);
		DrawTabs();
	    ImGui::EndChild();
	}
	ImGui::End();
}

void ScnEditor::VesselDeleted (OBJHANDLE hV)
{
	// update editor after vessel has been deleted
	if (m_currentVessel == hV) { // vessel is currently being edited
		m_currentVessel = nullptr;
	}
}

/*
void ScnEditor::ScanCBodyList (HWND hDlg, int hList, OBJHANDLE hSelect)
{
	// populate a list of celestial bodies
	char cbuf[256];
	SendDlgItemMessage (hDlg, hList, CB_RESETCONTENT, 0, 0);
	for (DWORD n = 0; n < oapiGetGbodyCount(); n++) {
		oapiGetObjectName (oapiGetGbodyByIndex (n), cbuf, 256);
		SendDlgItemMessage (hDlg, hList, CB_ADDSTRING, 0, (LPARAM)cbuf);
	}
	// select the requested body
	oapiGetObjectName (hSelect, cbuf, 256);
	SendDlgItemMessage (hDlg, hList, CB_SELECTSTRING, -1, (LPARAM)cbuf);
}

void ScnEditor::ScanPadList (HWND hDlg, int hList, OBJHANDLE hBase)
{
	SendDlgItemMessage (hDlg, hList, CB_RESETCONTENT, 0, 0);
	if (hBase) {
		DWORD n, npad = oapiGetBasePadCount (hBase);
		char cbuf[16];
		for (n = 1; n <= npad; n++) {
			sprintf (cbuf, "%d", n);
			SendDlgItemMessage (hDlg, hList, CB_ADDSTRING, 0, (LPARAM)cbuf);
		}
		SendDlgItemMessage (hDlg, hList, CB_SETCURSEL, 0, 0);
	}
}

void ScnEditor::SelectBase (HWND hDlg, int hList, OBJHANDLE hRef, OBJHANDLE hBase)
{
	char cbuf[256];
	GetWindowText (GetDlgItem (hDlg, hList), cbuf, 256);
	OBJHANDLE hOldBase = oapiGetBaseByName (hRef, cbuf);
	if (hBase) {
		oapiGetObjectName (hBase, cbuf, 256);
		SendDlgItemMessage (hDlg, hList, CB_SELECTSTRING, -1, (LPARAM)cbuf);
	}
	if (hOldBase != hBase) ScanPadList (hDlg, IDC_PAD, hBase);
}

void ScnEditor::SetBasePosition (HWND hDlg)
{
	char cbuf[256];
	DWORD pad;
	double lng, lat;
	GetWindowText (GetDlgItem (hDlg, IDC_REF), cbuf, 256);
	OBJHANDLE hRef = oapiGetGbodyByName (cbuf);
	if (!hRef) return;
	GetWindowText (GetDlgItem (hDlg, IDC_BASE), cbuf, 256);
	OBJHANDLE hBase = oapiGetBaseByName (hRef, cbuf);
	if (!hBase) return;
	GetWindowText (GetDlgItem (hDlg, IDC_PAD), cbuf, 256);
	if (sscanf (cbuf, "%d", &pad) && pad >= 1 && pad <= oapiGetBasePadCount (hBase))
		oapiGetBasePadEquPos (hBase, pad-1, &lng, &lat);
	else
		oapiGetBaseEquPos (hBase, &lng, &lat);
	sprintf (cbuf, "%lf", lng * DEG);
	SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf);
	sprintf (cbuf, "%lf", lat * DEG);
	SetWindowText (GetDlgItem (hDlg, IDC_EDIT2), cbuf);
}

bool ScnEditor::SaveScenario (HWND hDlg)
{
	char fname[256], desc[4096];
	GetWindowText (GetDlgItem (hDlg, IDC_EDIT1), fname, 256);
	GetWindowText (GetDlgItem (hDlg, IDC_EDIT2), desc, 4096);
	return oapiSaveScenario (fname, desc);
}

void ScnEditor::InitDialog (HWND _hDlg)
{
	hDlg = _hDlg;
	AddTab (new EditorTab_Vessel (this));
	AddTab (new EditorTab_New (this));
	AddTab (new EditorTab_Save (this));
	AddTab (new EditorTab_Edit (this));
	AddTab (new EditorTab_Landed (this));
	AddTab (new EditorTab_Propellant (this));
	AddTab (new EditorTab_Docking (this));
	AddTab (new EditorTab_Date (this));
	nTab0 = nTab;
	oapiAddTitleButton (IDPAUSE, g_hPause, DLG_CB_TWOSTATE);
	Pause (oapiGetPause());
	ShowTab (0);
}

DWORD ScnEditor::AddTab (ScnEditorTab *newTab)
{
	if (newTab->TabHandle()) {
		ScnEditorTab **tmpTab = new ScnEditorTab*[nTab+1];
		if (nTab) {
			memcpy (tmpTab, pTab, nTab*sizeof(ScnEditorTab*));
			delete []pTab;
		}
		pTab = tmpTab;
		pTab[nTab] = newTab;
		return nTab++;
	} else return (DWORD)-1;
}

void ScnEditor::DelCustomTabs ()
{
	if (!nTab || nTab == nTab0) return; // nothing to do
	for (DWORD i = nTab0; i < nTab; i++) delete pTab[i];
	ScnEditorTab **tmpTab = new ScnEditorTab*[nTab0];
	memcpy (tmpTab, pTab, nTab0*sizeof(ScnEditorTab*));
	delete []pTab;
	pTab = tmpTab;
	nTab = nTab0;
}

void ScnEditor::ShowTab (DWORD t)
{
	if (t < nTab) {
		cTab = pTab[t];
		cTab->Show();
	}
}

INT_PTR ScnEditor::MsgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		InitDialog (hDlg);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDCANCEL:
			CloseDialog();
			return TRUE;
		case IDHELP:
			if (cTab) cTab->OpenHelp();
			return TRUE;
		case IDPAUSE:
			oapiSetPause (HIWORD(wParam) != 0);
			return TRUE;
		}
		break;
	}
	return oapiDefDialogProc (hDlg, uMsg, wParam, lParam);
}

bool ScnEditor::CreateVessel (char *name, char *classname)
{
	// define an arbitrary status (the user will have to edit this after creation)
	VESSELSTATUS2 vs;
	memset (&vs, 0, sizeof(vs));
	vs.version = 2;
	vs.rbody = oapiGetGbodyByName ("Earth");
	if (!vs.rbody) vs.rbody = oapiGetGbodyByIndex (0);
	double rad = 1.1 * oapiGetSize (vs.rbody);
	double vel = sqrt (GGRAV * oapiGetMass (vs.rbody) / rad);
	vs.rpos = _V(rad,0,0);
	vs.rvel = _V(0,0,vel);
	// create the vessel
	OBJHANDLE newvessel = oapiCreateVesselEx (name, classname, &vs);
	if (!newvessel) return false; // failure
	hVessel = newvessel;
	return true;
}

void ScnEditor::VesselDeleted (OBJHANDLE hV)
{
	if (!hDlg) return; // no action required

	// update editor after vessel has been deleted
	if (hVessel == hV) { // vessel is currently being edited
		for (DWORD i = 0; i < nTab; i++)
			pTab[i]->Hide();
		ShowTab (0);
	}
	((EditorTab_Vessel*)pTab[0])->VesselDeleted (hV);
}

char *ScnEditor::ExtractVesselName (char *str)
{
	int i;
	for (i = 0; str[i] && str[i] != '\t'; i++);
	str[i] = '\0';
	return str;
}
*/
void ScnEditor::Pause (bool pause)
{
//	if (hDlg) oapiSetTitleButtonState (hDlg, IDPAUSE, pause ? 1:0);
}

MODULEHANDLE ScnEditor::LoadVesselLibrary (const VESSEL *vessel)
{
	// load vessel-specific editor extensions
	char cbuf[256];
	if (hEdLib) {
		oapiModuleUnload (hEdLib); // remove previous library
		m_customTabs = nullptr;
	}
	if (vessel->GetEditorModule (cbuf)) {
		char path[256];
		sprintf (path, "lib%s.so", cbuf);

		hEdLib = oapiModuleLoad (path);
		// now load vessel-specific interface
		if (hEdLib) {
			typedef ScnDrawCustomTabs (*SEC_Init)(void);
			SEC_Init secInit = (SEC_Init)oapiModuleGetProcAddress(hEdLib, "secInit");
			if (secInit) {
				m_customTabs = secInit ();
			}
		}
	}
	else hEdLib = nullptr;
	return hEdLib;
}
/*
// ==============================================================
// nonmember functions
*/
void OpenDialog (void *context)
{
	((ScnEditor*)context)->OpenDialog();
}

void Crt2Pol (VECTOR3 &pos, VECTOR3 &vel)
{
	// position in polar coordinates
	double r      = sqrt  (pos.x*pos.x + pos.y*pos.y + pos.z*pos.z);
	double lng    = atan2 (pos.z, pos.x);
	double lat    = asin  (pos.y/r);
	// derivatives in polar coordinates
	double drdt   = (vel.x*pos.x + vel.y*pos.y + vel.z*pos.z) / r;
	double dlngdt = (vel.z*pos.x - pos.z*vel.x) / (pos.x*pos.x + pos.z*pos.z);
	double dlatdt = (vel.y*r - pos.y*drdt) / (r*sqrt(r*r - pos.y*pos.y));
	pos.data[0] = r;
	pos.data[1] = lng;
	pos.data[2] = lat;
	vel.data[0] = drdt;
	vel.data[1] = dlngdt;
	vel.data[2] = dlatdt;
}

void Pol2Crt (VECTOR3 &pos, VECTOR3 &vel)
{
	double r   = pos.data[0];
	double lng = pos.data[1], clng = cos(lng), slng = sin(lng);
	double lat = pos.data[2], clat = cos(lat), slat = sin(lat);
	// position in cartesian coordinates
	double x   = r * cos(lat) * cos(lng);
	double z   = r * cos(lat) * sin(lng);
	double y   = r * sin(lat);
	// derivatives in cartesian coordinates
	double dxdt = vel.data[0]*clat*clng - vel.data[1]*r*clat*slng - vel.data[2]*r*slat*clng;
	double dzdt = vel.data[0]*clat*slng + vel.data[1]*r*clat*clng - vel.data[2]*r*slat*slng;
	double dydt = vel.data[0]*slat + vel.data[2]*r*clat;
	pos.data[0] = x;
	pos.data[1] = y;
	pos.data[2] = z;
	vel.data[0] = dxdt;
	vel.data[1] = dydt;
	vel.data[2] = dzdt;
}
/*
INT_PTR CALLBACK EditorProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return g_editor->MsgProc (hDlg, uMsg, wParam, lParam);
}

// ==============================================================
// ScnEditorTab class definition
// ==============================================================

ScnEditorTab::ScnEditorTab (ScnEditor *editor)
{
	ed = editor;
	HWND hDlg = ed->DlgHandle();
	hTab = 0;
}

ScnEditorTab::~ScnEditorTab ()
{
	DestroyTab ();
}

HWND ScnEditorTab::CreateTab (HINSTANCE hInst, WORD ResId,  DLGPROC TabProc)
{
	hTab = CreateDialogParam (hInst, MAKEINTRESOURCE(ResId), ed->DlgHandle(), TabProc, (LPARAM)this);
	SetWindowLongPtr (hTab, DWLP_USER, (LONG_PTR)this);
	return hTab;
}

HWND ScnEditorTab::CreateTab (WORD ResId, DLGPROC TabProc)
{
	return CreateTab (ed->InstHandle(), ResId, TabProc);
}

void ScnEditorTab::DestroyTab ()
{
	if (hTab) {
		DestroyWindow (hTab);
		hTab = 0;
	}
}

void ScnEditorTab::SwitchTab (int newtab)
{
	ShowWindow (hTab, SW_HIDE);
	ed->ShowTab (newtab);
}

void ScnEditorTab::Show ()
{
	ShowWindow (hTab, SW_SHOW);
	g_hc.topic = HelpTopic();
	InitTab ();
}

void ScnEditorTab::Hide ()
{
	ShowWindow (hTab, SW_HIDE);
}

char *ScnEditorTab::HelpTopic ()
{
	return "/ScnEditor.htm";
}

void ScnEditorTab::OpenHelp ()
{
	static HELPCONTEXT hc = {
		"html/plugin/ScnEditor.chm",
		0,
		"html/plugin/ScnEditor.chm::/ScnEditor.hhc",
		"html/plugin/ScnEditor.chm::/ScnEditor.hhk"
	};
	hc.topic = HelpTopic();
	oapiOpenHelp (&hc);
}

INT_PTR ScnEditorTab::TabProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDHELP:
			OpenHelp();
			return TRUE;
		}
		break;
	}
	return FALSE;
}

ScnEditorTab *ScnEditorTab::TabPointer (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_INITDIALOG) return (ScnEditorTab*)lParam;
	else return (ScnEditorTab*)GetWindowLongPtr (hDlg, DWLP_USER);
}

void ScnEditorTab::ScanVesselList (int ResId, bool detail, OBJHANDLE hExclude)
{
	char cbuf[256], *cp;

	// populate vessel list
	SendDlgItemMessage (hTab, ResId, LB_RESETCONTENT, 0, 0);
	for (DWORD i = 0; i < oapiGetVesselCount(); i++) {
		OBJHANDLE hR, hV = oapiGetVesselByIndex (i);
		if (hV == hExclude) continue;
		VESSEL *vessel = oapiGetVesselInterface (hV);
		strcpy (cbuf, vessel->GetName());
		if (detail) {
			if (cp = vessel->GetClassName())
				sprintf (cbuf+strlen(cbuf), "\t(%s)", cp);
			else strcat (cbuf, "\t");
			if (hR = vessel->GetGravityRef()) {
				char rname[256];
				oapiGetObjectName (hR, rname, 256);
				sprintf (cbuf+strlen(cbuf), "\t%s", rname);
			}
		}
		SendDlgItemMessage (hTab, ResId, LB_ADDSTRING, 0, (LPARAM)cbuf);
	}
}

OBJHANDLE ScnEditorTab::GetVesselFromList (int ResId)
{
	char cbuf[256];
	int idx = SendDlgItemMessage (hTab, ResId, LB_GETCURSEL, 0, 0);
	SendDlgItemMessage (hTab, ResId, LB_GETTEXT, idx, (LPARAM)cbuf);
	OBJHANDLE hV = oapiGetVesselByName (ed->ExtractVesselName (cbuf));
	return hV;
}


// ==============================================================
// EditorTab_Vessel class definition
// ==============================================================

EditorTab_Vessel::EditorTab_Vessel (ScnEditor *editor) : ScnEditorTab (editor)
{
	CreateTab (IDD_TAB_VESSEL, EditorTab_Vessel::DlgProc);
}

void EditorTab_Vessel::InitTab ()
{
	UINT tbs[1] = {70};
	SendDlgItemMessage (hTab, IDC_LIST1, LB_SETTABSTOPS, 1, (LPARAM)tbs);
	ScanVesselList ();
}

void EditorTab_Vessel::ScanVesselList ()
{
	ScnEditorTab::ScanVesselList (IDC_LIST1, true);
	// update vessel list
	SelectVessel (0);
}

char *EditorTab_Vessel::HelpTopic ()
{
	return "/ScnEditor.htm";
}

INT_PTR EditorTab_Vessel::TabProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int i;
	char cbuf[256];

	switch (uMsg) {
	case WM_INITDIALOG:
		SendDlgItemMessage (hDlg, IDC_TRACK, BM_SETCHECK, BST_CHECKED, 0);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDCANCEL:
			ed->CloseDialog();
			return TRUE;
		case IDC_VESSELNEW:
			SwitchTab (1);
			return TRUE;
		case IDC_VESSELEDIT:
			i = SendDlgItemMessage (hTab, IDC_LIST1, LB_GETCURSEL, 0, 0);
			SendDlgItemMessage (hTab, IDC_LIST1, LB_GETTEXT, i, (LPARAM)cbuf);
			ed->hVessel = oapiGetVesselByName (ed->ExtractVesselName(cbuf));
			if (ed->hVessel) SwitchTab (3);
			return TRUE;
		case IDC_VESSELDEL:
			DeleteVessel();
			return TRUE;
		case IDC_SAVE:
			SwitchTab (2);
			return TRUE;
		case IDC_DATE:
			SwitchTab (11);
			return TRUE;
		case IDC_LIST1:
			if (HIWORD (wParam) == LBN_SELCHANGE) {
				VesselSelected ();
				return TRUE;
			}
			break;
		}
		break;
	}
	return ScnEditorTab::TabProc (hDlg, uMsg, wParam, lParam);
}

void EditorTab_Vessel::SelectVessel (OBJHANDLE hV)
{
	char cbuf[256];
	if (!hV) hV = oapiCameraTarget();
	oapiGetObjectName (hV, cbuf, 256);
	int idx = SendDlgItemMessage (hTab, IDC_LIST1, LB_FINDSTRING, -1, (LPARAM)cbuf);
	if (idx == LB_ERR) { // last resort: focus object
		hV = oapiGetFocusObject();
		oapiGetObjectName (hV, cbuf, 256);
		idx = SendDlgItemMessage (hTab, IDC_LIST1, LB_FINDSTRING, -1, (LPARAM)cbuf);
	}
	if (idx != LB_ERR) {
		SendDlgItemMessage (hTab, IDC_LIST1, LB_SETCURSEL, idx, 0);
		ed->hVessel = hV;
	}
}

void EditorTab_Vessel::VesselSelected ()
{
	OBJHANDLE hV = GetVesselFromList (IDC_LIST1);
	if (!hV) return;
	bool track = (SendDlgItemMessage (hTab, IDC_TRACK, BM_GETCHECK, 0, 0) == BST_CHECKED);
	if (track) oapiCameraAttach (hV, 1);
}

void EditorTab_Vessel::VesselDeleted (OBJHANDLE hV)
{
	char cbuf[256];
	oapiGetObjectName (hV, cbuf, 256);
	int idx = SendDlgItemMessage (hTab, IDC_LIST1, LB_FINDSTRING, -1, (LPARAM)cbuf);
	if (idx == LB_ERR) return;
	SendDlgItemMessage (hTab, IDC_LIST1, LB_DELETESTRING, idx, 0);
	idx = SendDlgItemMessage (hTab, IDC_LIST1, LB_GETCURSEL, 0, 0);
	if (idx == LB_ERR) { // deleted current selection
		SendDlgItemMessage (hTab, IDC_LIST1, LB_SETCURSEL, 0, 0);
		VesselSelected ();
	}
}

bool EditorTab_Vessel::DeleteVessel ()
{
	char cbuf[256];
	int idx = SendDlgItemMessage (hTab, IDC_LIST1, LB_GETCURSEL, 0, 0);
	if (idx == LB_ERR) return false;
	SendDlgItemMessage (hTab, IDC_LIST1, LB_GETTEXT, idx, (LPARAM)cbuf);
	OBJHANDLE hV = oapiGetVesselByName (ed->ExtractVesselName (cbuf));
	if (!hV) return false;
	//ed->hVessel = hV;
	oapiDeleteVessel (hV);
	return true;
}

INT_PTR EditorTab_Vessel::DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EditorTab_Vessel *pTab = (EditorTab_Vessel*)TabPointer (hDlg, uMsg, wParam, lParam);
	if (!pTab) return FALSE;
	else return pTab->TabProc (hDlg, uMsg, wParam, lParam);
}

// ==============================================================
// EditorTab_New class definition
// ==============================================================

EditorTab_New::EditorTab_New (ScnEditor *editor) : ScnEditorTab (editor)
{
	hVesselBmp = NULL;
	CreateTab (IDD_TAB_NEW, EditorTab_New::DlgProc);
}

EditorTab_New::~EditorTab_New ()
{
	if (hVesselBmp) DeleteObject (hVesselBmp);
}

void EditorTab_New::InitTab ()
{
	SendDlgItemMessage (hTab, IDC_CAMERA, BM_SETCHECK, BST_CHECKED, 0);

	// populate vessel type list
	RefreshVesselTpList ();
	SendDlgItemMessage (hTab, IDC_VESSELTP, TVM_SETIMAGELIST, (WPARAM)TVSIL_NORMAL, (LPARAM)ed->imglist);

	RECT r;
	GetClientRect (GetDlgItem (hTab, IDC_VESSELBMP), &r);
	imghmax = r.bottom;
}

char *EditorTab_New::HelpTopic ()
{
	return "/NewVessel.htm";
}

INT_PTR EditorTab_New::TabProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	NM_TREEVIEW *pnmtv;

	switch (uMsg) {
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDCANCEL:
			SwitchTab (0);
			return TRUE;
		case IDC_CREATE:
			if (CreateVessel ()) SwitchTab (3); // switch to editor page
			return TRUE;
		}
		break;
	case WM_NOTIFY:
		switch (LOWORD(wParam)) {
		case IDC_VESSELTP:
			pnmtv = (NM_TREEVIEW FAR *)lParam;
			switch (pnmtv->hdr.code) {
			case TVN_SELCHANGED:
				VesselTpChanged ();
				return TRUE;
			case NM_DBLCLK:
				PostMessage (hDlg, WM_COMMAND, IDC_CREATE, 0);
				return TRUE;
			}
			break;
		}
		break;
	case WM_PAINT:
		DrawVesselBmp ();
		break;
	}
	return ScnEditorTab::TabProc (hDlg, uMsg, wParam, lParam);
}

bool EditorTab_New::CreateVessel ()
{
	char name[256], classname[256];

	GetWindowText (GetDlgItem (hTab, IDC_NAME), name, 256);
	if (name[0] == '\0') return false;            // no name provided
	if (oapiGetVesselByName (name)) return false; // vessel name already in use

	if (GetSelVesselTp (classname, 256) != 1) return false; // no type selected
	if (!ed->CreateVessel (name, classname)) return false; // creation failed
	if (SendDlgItemMessage (hTab, IDC_FOCUS, BM_GETCHECK, 0, 0) == BST_CHECKED)
		oapiSetFocusObject (ed->hVessel);
	if (SendDlgItemMessage (hTab, IDC_CAMERA, BM_GETSTATE, 0, 0) == BST_CHECKED)
		oapiCameraAttach (ed->hVessel, 1);
	//	ed->VesselSelection();
	return true;
}

int EditorTab_New::GetSelVesselTp (char *name, int len)
{
	TV_ITEM tvi;
	char cbuf[256];
	int type;

	tvi.mask = TVIF_HANDLE | TVIF_TEXT | TVIF_CHILDREN;
	tvi.hItem = TreeView_GetSelection (GetDlgItem (hTab, IDC_VESSELTP));
	tvi.pszText = name;
	tvi.cchTextMax = len;

	if (!TreeView_GetItem (GetDlgItem (hTab, IDC_VESSELTP), &tvi)) return 0;
	type = (tvi.cChildren ? 2 : 1);

	// build path
	tvi.pszText = cbuf;
	tvi.cchTextMax = 256;
	while (tvi.hItem = TreeView_GetParent (GetDlgItem (hTab, IDC_VESSELTP), tvi.hItem)) {
		if (TreeView_GetItem (GetDlgItem (hTab, IDC_VESSELTP), &tvi)) {
			strcat (cbuf, "\\");
			strcat (cbuf, name);
			strcpy (name, cbuf);
		}
	}
	return type;
}

void EditorTab_New::VesselTpChanged ()
{
	UpdateVesselBmp();
}

bool EditorTab_New::UpdateVesselBmp ()
{
	char classname[256], pathname[256], imagename[256];

	if (hVesselBmp) {
		DeleteObject (hVesselBmp);
		hVesselBmp = NULL;
	}

	if (GetSelVesselTp (classname, 256) == 1) {
		sprintf (pathname, "Vessels\\%s.cfg", classname);
		FILEHANDLE hFile = oapiOpenFile (pathname, FILE_IN, CONFIG);
		if (!hFile) return false;
		if (oapiReadItem_string (hFile, "ImageBmp", imagename)) {
			hVesselBmp = (HBITMAP)LoadImage (ed->InstHandle(), imagename, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		}
		oapiCloseFile (hFile, FILE_IN);
	}
	DrawVesselBmp();
	return (hVesselBmp != NULL);
}

// ==============================================================
// EditorTab_Save class definition
// ==============================================================

EditorTab_Save::EditorTab_Save (ScnEditor *editor) : ScnEditorTab (editor)
{
	CreateTab (IDD_TAB_SAVE, EditorTab_Save::DlgProc);
}

char *EditorTab_Save::HelpTopic ()
{
	return "/SaveScenario.htm";
}

INT_PTR EditorTab_Save::TabProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDC_BACK:
			SwitchTab (0);
			return TRUE;
		case IDOK:
			if (ed->SaveScenario (hTab))
				SwitchTab (0);
			return TRUE;
		}
		break;
	}
	return ScnEditorTab::TabProc (hDlg, uMsg, wParam, lParam);
}

INT_PTR EditorTab_Save::DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EditorTab_Save *pTab = (EditorTab_Save*)TabPointer (hDlg, uMsg, wParam, lParam);
	if (!pTab) return FALSE;
	else return pTab->TabProc (hDlg, uMsg, wParam, lParam);
}


// ==============================================================
// EditorTab_Date class definition
// ==============================================================

EditorTab_Date::EditorTab_Date (ScnEditor *editor) : ScnEditorTab (editor)
{
	CreateTab (IDD_TAB_DATE, EditorTab_Date::DlgProc);
}

void EditorTab_Date::InitTab ()
{
	Refresh();
}

char *EditorTab_Date::HelpTopic ()
{
	return "/Date.htm";
}

void EditorTab_Date::Apply ()
{
	int OrbitalMode[3] = {PROP_ORBITAL_FIXEDSTATE, PROP_ORBITAL_FIXEDSURF, PROP_ORBITAL_ELEMENTS};
	int SOrbitalMode[4] = {PROP_SORBITAL_FIXEDSTATE, PROP_SORBITAL_FIXEDSURF, PROP_SORBITAL_ELEMENTS, PROP_SORBITAL_DESTROY};
	int omode = OrbitalMode[SendDlgItemMessage (hTab, IDC_PROP_ORBITAL, CB_GETCURSEL, 0, 0)];
	int smode = SOrbitalMode[SendDlgItemMessage (hTab, IDC_PROP_SORBITAL, CB_GETCURSEL, 0, 0)];
	oapiSetSimMJD (mjd, omode | smode);
}

void EditorTab_Date::Refresh ()
{
	SetMJD (oapiGetSimMJD (), true);
}

void EditorTab_Date::UpdateDateTime ()
{
	char cbuf[256];

	sprintf (cbuf, "%02d", date.tm_mday);
	bIgnore = true;
	SetWindowText (GetDlgItem (hTab, IDC_UT_DAY), cbuf);
	bIgnore = false;

	sprintf (cbuf, "%02d", date.tm_mon);
	bIgnore = true;
	SetWindowText (GetDlgItem (hTab, IDC_UT_MONTH), cbuf);
	bIgnore = false;

	sprintf (cbuf, "%04d", date.tm_year+1900);
	bIgnore = true;
	SetWindowText (GetDlgItem (hTab, IDC_UT_YEAR), cbuf);
	bIgnore = false;

	sprintf (cbuf, "%02d", date.tm_hour);
	bIgnore = true;
	SetWindowText (GetDlgItem (hTab, IDC_UT_HOUR), cbuf);
	bIgnore = false;

	sprintf (cbuf, "%02d", date.tm_min);
	bIgnore = true;
	SetWindowText (GetDlgItem (hTab, IDC_UT_MIN), cbuf);
	bIgnore = false;

	sprintf (cbuf, "%02d", date.tm_sec);
	bIgnore = true;
	SetWindowText (GetDlgItem (hTab, IDC_UT_SEC), cbuf);
	bIgnore = false;
}

void EditorTab_Date::UpdateMJD (void)
{
	char cbuf[256];
	sprintf (cbuf, "%0.6f", mjd);
	bIgnore = true;
	SetWindowText (GetDlgItem (hTab, IDC_MJD), cbuf);
	bIgnore = false;
}

void EditorTab_Date::UpdateJD (void)
{
	char cbuf[256];
	sprintf (cbuf, "%0.6f", mjd + 2400000.5);
	bIgnore = true;
	SetWindowText (GetDlgItem (hTab, IDC_JD), cbuf);
	bIgnore = false;
}

void EditorTab_Date::UpdateJC (void)
{
	char cbuf[256];
	sprintf (cbuf, "%0.10f", MJD2JC(mjd));
	bIgnore = true;
	SetWindowText (GetDlgItem (hTab, IDC_JC2000), cbuf);
	bIgnore = false;
}

void EditorTab_Date::UpdateEpoch (void)
{
	char cbuf[256];
	sprintf (cbuf, "%0.8f", MJD2Jepoch (mjd));
	bIgnore = true;
	SetWindowText (GetDlgItem (hTab, IDC_EPOCH), cbuf);
	bIgnore = false;
}

void EditorTab_Date::SetUT (struct tm *new_date, bool reset_ut)
{
	mjd = date2mjd (new_date);
	UpdateMJD();
	UpdateJD();
	UpdateJC();
	UpdateEpoch();
	if (reset_ut) UpdateDateTime();
}

void EditorTab_Date::SetMJD (double new_mjd, bool reset_mjd)
{
	mjd = new_mjd;
	memcpy (&date, mjddate(mjd), sizeof (date));

	UpdateDateTime();
	UpdateJD();
	UpdateJC();
	UpdateEpoch();
	if (reset_mjd) UpdateMJD();
}

void EditorTab_Date::SetJD (double new_jd, bool reset_jd)
{
	mjd = new_jd - 2400000.5;
	memcpy (&date, mjddate(mjd), sizeof (date));

	UpdateDateTime();
	UpdateMJD();
	UpdateJC();
	UpdateEpoch();
	if (reset_jd) UpdateJD();
}

void EditorTab_Date::SetJC (double new_jc, bool reset_jc)
{
	mjd = JC2MJD (new_jc);
	memcpy (&date, mjddate(mjd), sizeof (date));

	UpdateDateTime();
	UpdateMJD();
	UpdateJD();
	UpdateEpoch();
	if (reset_jc) UpdateJC();
}

void EditorTab_Date::SetEpoch (double new_epoch, bool reset_epoch)
{
	mjd = Jepoch2MJD (new_epoch);
	memcpy (&date, mjddate(mjd), sizeof (date));

	UpdateDateTime();
	UpdateMJD();
	UpdateJD();
	UpdateJC();
	if (reset_epoch) UpdateEpoch();
}

void EditorTab_Date::OnChangeDateTime() 
{
	if (bIgnore) return;
	char cbuf[256];
	int day, month, year, hour, min, sec;

	GetWindowText (GetDlgItem (hTab, IDC_UT_DAY), cbuf, 256);
	if (sscanf (cbuf, "%d", &day) != 1 || day < 1 || day > 31) return;
	GetWindowText (GetDlgItem (hTab, IDC_UT_MONTH), cbuf, 256);
	if (sscanf (cbuf, "%d", &month) != 1 || month < 1 || month > 12) return;
	GetWindowText (GetDlgItem (hTab, IDC_UT_YEAR), cbuf, 256);
	if (sscanf (cbuf, "%d", &year) != 1) return;
	year -= 1900;
	GetWindowText (GetDlgItem (hTab, IDC_UT_HOUR), cbuf, 256);
	if (sscanf (cbuf, "%d", &hour) != 1) return;
	GetWindowText (GetDlgItem (hTab, IDC_UT_MIN), cbuf, 256);
	if (sscanf (cbuf, "%d", &min) != 1) return;
	GetWindowText (GetDlgItem (hTab, IDC_UT_SEC), cbuf, 256);
	if (sscanf (cbuf, "%d", &sec) != 1) return;

	if (day == date.tm_mday && month == date.tm_mon && year == date.tm_year &&
		hour == date.tm_hour && min == date.tm_min && sec == date.tm_sec) return;

	date.tm_mday = day;
	date.tm_mon  = month;
	date.tm_year = year;
	date.tm_hour = hour;
	date.tm_min  = min;
	date.tm_sec  = sec;
	SetUT (&date);
}

void EditorTab_Date::OnChangeMjd() 
{
	if (bIgnore) return;
	char cbuf[256];
	double new_mjd;
	GetWindowText (GetDlgItem (hTab, IDC_MJD), cbuf, 256);
	if (sscanf (cbuf, "%lf", &new_mjd) == 1 && fabs (new_mjd-mjd) > 1e-6)
		SetMJD (new_mjd);
}

void EditorTab_Date::OnChangeJd() 
{
	if (bIgnore) return;
	char cbuf[256];
	double new_jd;
	GetWindowText (GetDlgItem (hTab, IDC_JD), cbuf, 256);
	if (sscanf (cbuf, "%lf", &new_jd) == 1)
		SetJD (new_jd);
}

void EditorTab_Date::OnChangeJc() 
{
	if (bIgnore) return;
	char cbuf[256];
	double new_jc;
	GetWindowText (GetDlgItem (hTab, IDC_JC2000), cbuf, 256);
	if (sscanf (cbuf, "%lf", &new_jc) == 1)
		SetJC (new_jc);
}

void EditorTab_Date::OnChangeEpoch() 
{
	if (bIgnore) return;
	char cbuf[256];
	double new_epoch;
	GetWindowText (GetDlgItem (hTab, IDC_EPOCH), cbuf, 256);
	if (sscanf (cbuf, "%lf", &new_epoch) == 1)
		SetEpoch (new_epoch);
}

INT_PTR EditorTab_Date::TabProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG: {
		int i;
		SendDlgItemMessage (hDlg, IDC_PROP_ORBITAL, CB_RESETCONTENT, 0, 0);
		for (i = 0; i < 3; i++) {
			char cbuf[128];
			LoadString (ed->InstHandle(), IDS_PROP1+i, cbuf, 128);
			SendDlgItemMessage (hDlg, IDC_PROP_ORBITAL, CB_ADDSTRING, 0, (LPARAM)cbuf);
		}
		SendDlgItemMessage (hDlg, IDC_PROP_ORBITAL, CB_SETCURSEL, 2, 0);
		SendDlgItemMessage (hDlg, IDC_PROP_SORBITAL, CB_RESETCONTENT, 0, 0);
		for (i = 0; i < 4; i++) {
			char cbuf[128];
			LoadString (ed->InstHandle(), IDS_PROP1+i, cbuf, 128);
			SendDlgItemMessage (hDlg, IDC_PROP_SORBITAL, CB_ADDSTRING, 0, (LPARAM)cbuf);
		}
		SendDlgItemMessage (hDlg, IDC_PROP_SORBITAL, CB_SETCURSEL, 1, 0);
		} return TRUE;
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDC_BACK:
			SwitchTab (0);
			return TRUE;
		case IDC_REFRESH:
			Refresh();
			return TRUE;
		case IDC_NOW:
			SetMJD (oapiGetSysMJD(), true);
			// fall through
		case IDC_APPLY:
			Apply();
			return TRUE;
		case IDC_UT_DAY:
		case IDC_UT_MONTH:
		case IDC_UT_YEAR:
		case IDC_UT_HOUR:
		case IDC_UT_MIN:
		case IDC_UT_SEC:
			OnChangeDateTime ();
			return TRUE;
		case IDC_MJD:
			OnChangeMjd ();
			return TRUE;
		case IDC_JD:
			OnChangeJd ();
			return TRUE;
		case IDC_JC2000:
			OnChangeJc ();
			return TRUE;
		case IDC_EPOCH:
			OnChangeEpoch ();
			return TRUE;
		}
		break;
	case WM_NOTIFY:
		if (((NMHDR*)lParam)->code == UDN_DELTAPOS) {
			NMUPDOWN *nmud = (NMUPDOWN*)lParam;
			double dmjd = 0;
			bool dut = false;
			switch (((NMHDR*)lParam)->idFrom) {
			case IDC_SPIN_DAY:
				dmjd = -nmud->iDelta;
				break;
			case IDC_SPIN_HOUR:
				dmjd = -nmud->iDelta/24.0;
				break;
			case IDC_SPIN_MINUTE:
				dmjd = -nmud->iDelta/(24.0*60.0);
				break;
			case IDC_SPIN_SECOND:
				dmjd = -nmud->iDelta/(24.0*3600.0);
				break;
			case IDC_SPIN_MONTH:
				if (nmud->iDelta > 0) {
					date.tm_mon--;
					if (date.tm_mon < 1) date.tm_year--, date.tm_mon = 12;
				} else {
					date.tm_mon++;
					if (date.tm_mon > 12) date.tm_year++, date.tm_mon = 1;
				}
				dut = true;
				break;
			case IDC_SPIN_YEAR:
				date.tm_year -= nmud->iDelta;
				dut = true;
				break;
			}
			if (dmjd || dut) {
				if (dmjd) SetMJD (mjd+dmjd, true);
				else SetUT (&date, true);
				Apply();
			}
		}
		break;
	}
	return ScnEditorTab::TabProc (hDlg, uMsg, wParam, lParam);
}

INT_PTR EditorTab_Date::DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EditorTab_Date *pTab = (EditorTab_Date*)TabPointer (hDlg, uMsg, wParam, lParam);
	if (!pTab) return FALSE;
	else return pTab->TabProc (hDlg, uMsg, wParam, lParam);
}


// ==============================================================
// EditorTab_Edit class definition
// ==============================================================

EditorTab_Edit::EditorTab_Edit (ScnEditor *editor) : ScnEditorTab (editor)
{
	hVessel = 0;
	CreateTab (IDD_TAB_EDIT1, EditorTab_Edit::DlgProc);
}

void EditorTab_Edit::InitTab ()
{
	VESSEL *vessel = Vessel();

	if (hVessel != ed->hVessel) {
		hVessel = ed->hVessel;

		// fill vessel and class name boxes
		SetWindowText (GetDlgItem (hTab, IDC_EDIT1), vessel->GetName());
		SetWindowText (GetDlgItem (hTab, IDC_EDIT2), vessel->GetClassName());
		SetWindowText (GetDlgItem (hTab, IDC_STATIC1), vessel->GetClassName());

		BOOL bFuel = (vessel->GetPropellantCount() > 0 && vessel->GetMaxFuelMass () > 0.0);
		EnableWindow (GetDlgItem (hTab, IDC_PROPELLANT), bFuel);

		BOOL bDocking = (vessel->DockCount() > 0);
		EnableWindow (GetDlgItem (hTab, IDC_DOCKING), bDocking);

		// disable custom buttons by default
		nCustom = 0;
		ed->DelCustomTabs();
		ShowWindow (GetDlgItem (hTab, IDC_STATIC1), SW_HIDE);
		for (int i = 0; i < 6; i++) {
			CustomPage[i] = 0;
			ShowWindow (GetDlgItem (hTab, IDC_EXTRA1+i), SW_HIDE);
		}

		// now load vessel-specific interface
		HINSTANCE hLib = ed->LoadVesselLibrary (vessel);
		if (hLib) {
			typedef void (*SEC_Init)(HWND,OBJHANDLE);
			SEC_Init secInit = (SEC_Init)GetProcAddress (hLib, "secInit");
			if (secInit) secInit (hTab, hVessel);
		}
	}
	SetWindowText (GetDlgItem (hTab, IDC_EDIT3),
		vessel->GetFlightStatus() & 1 ? "Inactive (Landed)":"Active (Flight)");
}

char *EditorTab_Edit::HelpTopic ()
{
	return "/EditVessel.htm";
}

BOOL EditorTab_Edit::AddFuncButton (EditorFuncSpec *efs)
{
	if (nCustom == 6) return FALSE;
	ShowWindow (GetDlgItem (hTab, IDC_STATIC1), SW_SHOW);
	SetWindowText (GetDlgItem (hTab, IDC_EXTRA1+nCustom), efs->btnlabel);
	ShowWindow (GetDlgItem (hTab, IDC_EXTRA1+nCustom), SW_SHOW);
	funcCustom[nCustom++] = efs->func;
	return TRUE;

}

BOOL EditorTab_Edit::AddPageButton (EditorPageSpec *eps)
{
	if (nCustom == 6) return FALSE;
	ShowWindow (GetDlgItem (hTab, IDC_STATIC1), SW_SHOW);
	SetWindowText (GetDlgItem (hTab, IDC_EXTRA1+nCustom), eps->btnlabel);
	ShowWindow (GetDlgItem (hTab, IDC_EXTRA1+nCustom), SW_SHOW);
	CustomPage[nCustom++] = ed->AddTab (new EditorTab_Custom (ed, eps->hDLL, eps->ResId, eps->TabProc));
	return TRUE;
}

INT_PTR EditorTab_Edit::TabProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDC_GROUND:
			SwitchTab (6);
			return TRUE;
		case IDC_ORIENT:
			SwitchTab (7);
			return TRUE;
		case IDC_ANGVEL:
			SwitchTab (8);
			return TRUE;
		case IDC_PROPELLANT:
			SwitchTab (9);
			return TRUE;
		case IDC_DOCKING:
			SwitchTab (10);
			return TRUE;
		case IDC_EXTRA1:
		case IDC_EXTRA2:
		case IDC_EXTRA3:
		case IDC_EXTRA4:
		case IDC_EXTRA5:
		case IDC_EXTRA6: {
			int id = LOWORD(wParam)-IDC_EXTRA1;
			if (CustomPage[id])
				SwitchTab (CustomPage[id]);
			else
				funcCustom[id](ed->hVessel);
			} return TRUE;
		}
		break;
	case WM_SCNEDITOR:
		switch (LOWORD (wParam)) {
		case SE_ADDFUNCBUTTON:
			return AddFuncButton ((EditorFuncSpec*)lParam);
		case SE_ADDPAGEBUTTON:
			return AddPageButton ((EditorPageSpec*)lParam);
		}
		break;
	}
	return ScnEditorTab::TabProc (hDlg, uMsg, wParam, lParam);
}

INT_PTR EditorTab_Edit::DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EditorTab_Edit *pTab = (EditorTab_Edit*)TabPointer (hDlg, uMsg, wParam, lParam);
	if (!pTab) return FALSE;
	else return pTab->TabProc (hDlg, uMsg, wParam, lParam);
}

// ==============================================================
// EditorTab_Statevec class definition
// ==============================================================

EditorTab_Statevec::EditorTab_Statevec (ScnEditor *editor) : ScnEditorTab (editor)
{
	CreateTab (IDD_TAB_EDIT3, EditorTab_Statevec::DlgProc);
}

void EditorTab_Statevec::InitTab ()
{
	VESSEL *vessel = oapiGetVesselInterface (ed->hVessel);
	OBJHANDLE hRef = vessel->GetGravityRef();

	SendDlgItemMessage (hTab, IDC_FRM, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage (hTab, IDC_FRM, CB_ADDSTRING, 0, (LPARAM)"ecliptic");
	SendDlgItemMessage (hTab, IDC_FRM, CB_ADDSTRING, 0, (LPARAM)"ref. equator (fixed)");
	SendDlgItemMessage (hTab, IDC_FRM, CB_ADDSTRING, 0, (LPARAM)"ref. equator (rotating)");
	SendDlgItemMessage (hTab, IDC_FRM, CB_SETCURSEL, 0, 0);

	SendDlgItemMessage (hTab, IDC_CRD, CB_RESETCONTENT, 0, 0);
	SendDlgItemMessage (hTab, IDC_CRD, CB_ADDSTRING, 0, (LPARAM)"cartesian");
	SendDlgItemMessage (hTab, IDC_CRD, CB_ADDSTRING, 0, (LPARAM)"polar");
	SendDlgItemMessage (hTab, IDC_CRD, CB_SETCURSEL, 0, 0);

	DlgLabels ();
	ed->ScanCBodyList (hTab, IDC_REF, hRef);
	ScanVesselList ();
	Refresh ();
}

void EditorTab_Statevec::ScanVesselList ()
{
	char cbuf[256];

	// populate vessel list
	SendDlgItemMessage (hTab, IDC_STATECPY, LB_RESETCONTENT, 0, 0);
	for (DWORD i = 0; i < oapiGetVesselCount(); i++) {
		OBJHANDLE hV = oapiGetVesselByIndex (i);
		if (hV == ed->hVessel) continue;                  // skip myself
		VESSEL *vessel = oapiGetVesselInterface (hV);
		if ((vessel->GetFlightStatus() & 1) == 1) continue; // skip landed vessels
		strcpy (cbuf, vessel->GetName());
		SendDlgItemMessage (hTab, IDC_STATECPY, LB_ADDSTRING, 0, (LPARAM)cbuf);
	}
}

char *EditorTab_Statevec::HelpTopic ()
{
	return "/Statevec.htm";
}

void EditorTab_Statevec::DlgLabels ()
{
	int crd = SendDlgItemMessage (hTab, IDC_CRD, CB_GETCURSEL, 0, 0);
	SetWindowText (GetDlgItem (hTab, IDC_STATIC1A), crd ? "radius" : "x");
	SetWindowText (GetDlgItem (hTab, IDC_STATIC2A), crd ? "longitude" : "y");
	SetWindowText (GetDlgItem (hTab, IDC_STATIC3A), crd ? "latitude" : "z");
	SetWindowText (GetDlgItem (hTab, IDC_STATIC4A), crd ? "d radius / dt" : "dx / dt");
	SetWindowText (GetDlgItem (hTab, IDC_STATIC5A), crd ? "d longitude / dt" : "dy / dt");
	SetWindowText (GetDlgItem (hTab, IDC_STATIC6A), crd ? "d latitude / dt" : "dz / dt");
	SetWindowText (GetDlgItem (hTab, IDC_STATIC2), crd ? "deg" : "m");
	SetWindowText (GetDlgItem (hTab, IDC_STATIC3), crd ? "deg" : "m");
	SetWindowText (GetDlgItem (hTab, IDC_STATIC5), crd ? "deg/s" : "m/s");
	SetWindowText (GetDlgItem (hTab, IDC_STATIC6), crd ? "deg/s" : "m/s");
}

INT_PTR EditorTab_Statevec::TabProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDC_BACK:
			SwitchTab (3);
			return TRUE;
		case IDC_APPLY:
			Apply ();
			return TRUE;
		case IDC_REFRESH:
			Refresh ();
			return TRUE;
		case IDC_REF:
			if (HIWORD (wParam) == CBN_SELCHANGE || HIWORD (wParam) == CBN_EDITCHANGE) {
				PostMessage (hDlg, WM_COMMAND, IDC_REFRESH, 0);
				return TRUE;
			}
			break;
		case IDC_FRM:
			if (HIWORD (wParam) == CBN_SELCHANGE) {
				PostMessage (hDlg, WM_COMMAND, IDC_REFRESH, 0);
				return TRUE;
			}
			break;
		case IDC_CRD:
			if (HIWORD (wParam) == CBN_SELCHANGE) {
				DlgLabels ();
				PostMessage (hDlg, WM_COMMAND, IDC_REFRESH, 0);
				return TRUE;
			}
			break;
		case IDC_STATECPY: {
			OBJHANDLE hV = GetVesselFromList (IDC_STATECPY);
			switch (HIWORD(wParam)) {
			case LBN_SELCHANGE:
				Refresh (hV);
				break;
			case LBN_DBLCLK:
				Refresh (hV);
				Apply ();
				break;
			}
			} break;
		}
		break;
	case WM_NOTIFY:
		if (((NMHDR*)lParam)->code == UDN_DELTAPOS) {
			char cbuf[256];
			int crd = SendDlgItemMessage (hTab, IDC_CRD, CB_GETCURSEL, 0, 0);
			int prec, idx = 0;
			double val, dv;
			NMUPDOWN *nmud = (NMUPDOWN*)lParam;
			switch (((NMHDR*)lParam)->idFrom) {
			case IDC_SPIN1:
				idx = IDC_EDIT1; prec = 1; dv = -nmud->iDelta*1.0;
				break;
			case IDC_SPIN1A:
				idx = IDC_EDIT1; prec = 1; dv = -nmud->iDelta*1000.0;
				break;
			case IDC_SPIN2:
				idx = IDC_EDIT2; prec = (crd?6:1); dv = -nmud->iDelta*(crd?0.0001:1.0);
				break;
			case IDC_SPIN2A:
				idx = IDC_EDIT2; prec = (crd?6:1); dv = -nmud->iDelta*(crd?0.1:1000.0);
				break;
			case IDC_SPIN3:
				idx = IDC_EDIT3; prec = (crd?6:1); dv = -nmud->iDelta*(crd?0.0001:1.0);
				break;
			case IDC_SPIN3A:
				idx = IDC_EDIT3; prec = (crd?6:1); dv = -nmud->iDelta*(crd?0.1:1000.0);
				break;
			case IDC_SPIN4:
				idx = IDC_EDIT4; prec = 2; dv = -nmud->iDelta*0.1;
				break;
			case IDC_SPIN4A:
				idx = IDC_EDIT4; prec = 2; dv = -nmud->iDelta*100.0;
				break;
			case IDC_SPIN5:
				idx = IDC_EDIT5; prec = (crd?7:2); dv = -nmud->iDelta*(crd?1e-5:0.1);
				break;
			case IDC_SPIN5A:
				idx = IDC_EDIT5; prec = (crd?7:2); dv = -nmud->iDelta*(crd?1e-2:100.0);
				break;
			case IDC_SPIN6:
				idx = IDC_EDIT6; prec = (crd?7:2); dv = -nmud->iDelta*(crd?1e-5:0.1);
				break;
			case IDC_SPIN6A:
				idx = IDC_EDIT6; prec = (crd?7:2); dv = -nmud->iDelta*(crd?1e-2:100.0);
				break;
			}
			if (idx) {
				GetWindowText (GetDlgItem (hDlg, idx), cbuf, 256);
				sscanf (cbuf, "%lf", &val);
				val += dv;
				sprintf (cbuf, "%0.*f", prec, val);
				SetWindowText (GetDlgItem (hDlg, idx), cbuf);
				Apply ();
				return TRUE;
			}
		}
		break;
	}
	return ScnEditorTab::TabProc (hDlg, uMsg, wParam, lParam);
}

INT_PTR EditorTab_Statevec::DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EditorTab_Statevec *pTab = (EditorTab_Statevec*)TabPointer (hDlg, uMsg, wParam, lParam);
	if (!pTab) return FALSE;
	else return pTab->TabProc (hDlg, uMsg, wParam, lParam);
}

// ==============================================================
// EditorTab_Landed class definition
// ==============================================================

EditorTab_Landed::EditorTab_Landed (ScnEditor *editor) : ScnEditorTab (editor)
{
	CreateTab (IDD_TAB_EDIT4, EditorTab_Landed::DlgProc);
}

void EditorTab_Landed::InitTab ()
{
	VESSEL *vessel = oapiGetVesselInterface (ed->hVessel);
	OBJHANDLE hRef = vessel->GetGravityRef();

	ScanCBodyList (hTab, IDC_REF, hRef);
	ScanBaseList (hTab, IDC_BASE, hRef);
	ScanVesselList ();
	Refresh ();
}

void EditorTab_Landed::ScanVesselList ()
{
	char cbuf[256];

	// populate vessel list
	SendDlgItemMessage (hTab, IDC_STATECPY, LB_RESETCONTENT, 0, 0);
	for (DWORD i = 0; i < oapiGetVesselCount(); i++) {
		OBJHANDLE hV = oapiGetVesselByIndex (i);
		if (hV == ed->hVessel) continue;                  // skip myself
		VESSEL *vessel = oapiGetVesselInterface (hV);
		if ((vessel->GetFlightStatus() & 1) == 0) continue; // skip vessels in flight
		strcpy (cbuf, vessel->GetName());
		SendDlgItemMessage (hTab, IDC_STATECPY, LB_ADDSTRING, 0, (LPARAM)cbuf);
	}
}

char *EditorTab_Landed::HelpTopic ()
{
	return "/Location.htm";
}

INT_PTR EditorTab_Landed::TabProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDC_BACK:
			SwitchTab (3);
			return TRUE;
		case IDC_APPLY:
			Apply ();
			return TRUE;
		case IDC_REFRESH:
			if (lParam == 1) { // rescan bases
				char cbuf[256];
				GetWindowText (GetDlgItem (hDlg, IDC_REF), cbuf, 256);
				OBJHANDLE hRef = oapiGetGbodyByName (cbuf);
				ScanBaseList (hDlg, IDC_BASE, hRef);
			}
			Refresh ();
			return TRUE;
		case IDC_REF:
			if (HIWORD (wParam) == CBN_SELCHANGE || HIWORD (wParam) == CBN_EDITCHANGE) {
				PostMessage (hDlg, WM_USER+0, 0, 0);
				return TRUE;
			}
		case IDC_BASE:
			if (HIWORD (wParam) == CBN_SELCHANGE || HIWORD (wParam) == CBN_EDITCHANGE) {
				PostMessage (hDlg, WM_USER+1, 0, 0);
				return TRUE;
			}
			break;
		case IDC_PAD:
			if (HIWORD (wParam) == CBN_SELCHANGE || HIWORD (wParam) == CBN_EDITCHANGE) {
				ed->SetBasePosition (hDlg);
				PostMessage (hDlg, WM_COMMAND, IDC_APPLY, 0);
				return TRUE;
			}
			break;
		case IDC_STATECPY: {
			OBJHANDLE hV = GetVesselFromList (IDC_STATECPY);
			switch (HIWORD(wParam)) {
			case LBN_SELCHANGE:
				Refresh (hV);
				break;
			case LBN_DBLCLK:
				Refresh (hV);
				Apply ();
				break;
			}
			} break;
		}
		break;
	case WM_USER+0: { // reference body changed
		char cbuf[256];
		GetWindowText (GetDlgItem (hDlg, IDC_REF), cbuf, 256);
		OBJHANDLE hRef = oapiGetGbodyByName (cbuf);
		if (!hRef) break;
		ScanBaseList (hDlg, IDC_BASE, hRef);
		PostMessage (hDlg, WM_USER+1, 0, 0);
		} return TRUE;
	case WM_USER+1: { // base changed
		char cbuf[256];
		GetWindowText (GetDlgItem (hDlg, IDC_REF), cbuf, 256);
		OBJHANDLE hRef = oapiGetGbodyByName (cbuf);
		if (!hRef) break;
		GetWindowText (GetDlgItem (hDlg, IDC_BASE), cbuf, 256);
		OBJHANDLE hBase = oapiGetBaseByName (hRef, cbuf);
		ed->ScanPadList (hDlg, IDC_PAD, hBase);
		ed->SetBasePosition (hDlg);
		PostMessage (hDlg, WM_COMMAND, IDC_APPLY, 0);
		} return TRUE;
	case WM_NOTIFY:
		if (((NMHDR*)lParam)->code == UDN_DELTAPOS) {
			NMUPDOWN *nmud = (NMUPDOWN*)lParam;
			char cbuf[256];
			double val;
			int id = ((NMHDR*)lParam)->idFrom;
			switch (id) {
			case IDC_SPIN1:
			case IDC_SPIN1A:
				GetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf, 256);
				sscanf (cbuf, "%lf", &val);
				val -= nmud->iDelta * (id == IDC_SPIN1 ? 0.00001 : 0.001);
				if      (val < -180.0) val += 360.0;
				else if (val > +180.0) val -= 360.0;
				sprintf (cbuf, "%lf", val);
				SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf);
				Apply ();
				return TRUE;
			case IDC_SPIN2:
			case IDC_SPIN2A:
				GetWindowText (GetDlgItem (hDlg, IDC_EDIT2), cbuf, 256);
				sscanf (cbuf, "%lf", &val);
				val -= nmud->iDelta * (id == IDC_SPIN2 ? 0.00001 : 0.001);
				val = min (90, max (-90, val));
				sprintf (cbuf, "%lf", val);
				SetWindowText (GetDlgItem (hDlg, IDC_EDIT2), cbuf);
				Apply ();
				return TRUE;
			case IDC_SPIN3:
			case IDC_SPIN3A:
				GetWindowText (GetDlgItem (hDlg, IDC_EDIT3), cbuf, 256);
				sscanf (cbuf, "%lf", &val);
				val -= nmud->iDelta * (id == IDC_SPIN3 ? 0.01 : 1.0);
				if      (val <   0.0) val += 360.0;
				else if (val > 360.0) val -= 360.0;
				sprintf (cbuf, "%lf", val);
				SetWindowText (GetDlgItem (hDlg, IDC_EDIT3), cbuf);
				Apply ();
				return TRUE;
			}
		}
		break;
	}
	return ScnEditorTab::TabProc (hDlg, uMsg, wParam, lParam);
}

void EditorTab_Landed::Refresh (OBJHANDLE hV)
{
	char cbuf[256], lngstr[64], latstr[64];
	bool scancbody = ((hV != NULL) && (hV != ed->hVessel));
	if (!hV) hV = ed->hVessel;
	VESSEL *vessel = oapiGetVesselInterface (hV);
	if (scancbody) SelectCBody (vessel->GetSurfaceRef());
	GetWindowText (GetDlgItem (hTab, IDC_REF), cbuf, 256);
	OBJHANDLE hRef = oapiGetGbodyByName (cbuf);
	if (!hRef) return;

	VESSELSTATUS2 vs;
	memset (&vs, 0, sizeof(vs)); vs.version = 2;
	vessel->GetStatusEx (&vs);
	if (vs.rbody == hRef) {
		sprintf (lngstr, "%lf", vs.surf_lng * DEG);
		sprintf (latstr, "%lf", vs.surf_lat * DEG);
		ed->SelectBase (hTab, IDC_BASE, hRef, vs.base);
	} else {
		// calculate ground position
		VECTOR3 pos;
		MATRIX3 rot;
		oapiGetRelativePos (ed->hVessel, hRef, &pos);
		oapiGetRotationMatrix (hRef, &rot);
		pos = tmul (rot, pos);
		Crt2Pol (pos, _V(0,0,0));
		sprintf (lngstr, "%lf", pos.data[1] * DEG);
		sprintf (latstr, "%lf", pos.data[2] * DEG);
	}
	SetWindowText (GetDlgItem (hTab, IDC_EDIT1), lngstr);
	SetWindowText (GetDlgItem (hTab, IDC_EDIT2), latstr);

	sprintf (cbuf, "%lf", vs.surf_hdg * DEG);
	SetWindowText (GetDlgItem (hTab, IDC_EDIT3), cbuf);
}

void EditorTab_Landed::Apply ()
{
	char cbuf[256];
	GetWindowText (GetDlgItem (hTab, IDC_REF), cbuf, 256);
	VESSEL *vessel = oapiGetVesselInterface (ed->hVessel);
	OBJHANDLE hRef = oapiGetGbodyByName (cbuf);
	if (!hRef) return;
	VESSELSTATUS2 vs;
	memset (&vs, 0, sizeof(vs));
	vs.version = 2;
	vs.rbody = hRef;
	vs.status = 1; // landed
	vs.arot.x = 10; // use default touchdown orientation
	GetWindowText (GetDlgItem (hTab, IDC_EDIT1), cbuf, 256);
	sscanf (cbuf, "%lf", &vs.surf_lng); vs.surf_lng *= RAD;
	GetWindowText (GetDlgItem (hTab, IDC_EDIT2), cbuf, 256);
	sscanf (cbuf, "%lf", &vs.surf_lat); vs.surf_lat *= RAD;
	GetWindowText (GetDlgItem (hTab, IDC_EDIT3), cbuf, 256);
	sscanf (cbuf, "%lf", &vs.surf_hdg); vs.surf_hdg *= RAD;
	vessel->DefSetStateEx (&vs);
}

void EditorTab_Landed::ScanCBodyList (HWND hDlg, int hList, OBJHANDLE hSelect)
{
	// populate a list of celestial bodies
	char cbuf[256];
	SendDlgItemMessage (hDlg, hList, CB_RESETCONTENT, 0, 0);
	for (DWORD n = 0; n < oapiGetGbodyCount(); n++) {
		OBJHANDLE hBody = oapiGetGbodyByIndex (n);
		if (oapiGetObjectType (hBody) == OBJTP_STAR) continue; // skip stars
		oapiGetObjectName (hBody, cbuf, 256);
		SendDlgItemMessage (hDlg, hList, CB_ADDSTRING, 0, (LPARAM)cbuf);
	}
	// select the requested body
	oapiGetObjectName (hSelect, cbuf, 256);
	SendDlgItemMessage (hDlg, hList, CB_SELECTSTRING, -1, (LPARAM)cbuf);
}

void EditorTab_Landed::ScanBaseList (HWND hDlg, int hList, OBJHANDLE hRef)
{
	char cbuf[256];
	DWORD n;

	SendDlgItemMessage (hDlg, hList, CB_RESETCONTENT, 0, 0);
	for (n = 0; n < oapiGetBaseCount (hRef); n++) {
		oapiGetObjectName (oapiGetBaseByIndex (hRef, n), cbuf, 256);
		SendDlgItemMessage (hDlg, hList, CB_ADDSTRING, 0, (LPARAM)cbuf);
	}
}

void EditorTab_Landed::SelectCBody (OBJHANDLE hBody)
{
	char cbuf[256];
	oapiGetObjectName (hBody, cbuf, 256);
	int idx = SendDlgItemMessage (hTab, IDC_REF, CB_FINDSTRING, -1, (LPARAM)cbuf);
	if (idx != LB_ERR) {
		SendDlgItemMessage (hTab, IDC_REF, CB_SETCURSEL, idx, 0);
		PostMessage (hTab, WM_USER+0, 0, 0);
	}
}

INT_PTR EditorTab_Landed::DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EditorTab_Landed *pTab = (EditorTab_Landed*)TabPointer (hDlg, uMsg, wParam, lParam);
	if (!pTab) return FALSE;
	else return pTab->TabProc (hDlg, uMsg, wParam, lParam);
}


// ==============================================================
// EditorTab_Orientation class definition
// ==============================================================

EditorTab_Orientation::EditorTab_Orientation (ScnEditor *editor) : ScnEditorTab (editor)
{
	CreateTab (IDD_TAB_EDIT5, EditorTab_Orientation::DlgProc);
}

void EditorTab_Orientation::InitTab ()
{
	Refresh();
}

char *EditorTab_Orientation::HelpTopic ()
{
	return "/Orientation.htm";
}

INT_PTR EditorTab_Orientation::TabProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDC_BACK:
			SwitchTab (3);
			return TRUE;
		case IDC_REFRESH:
			Refresh ();
			return TRUE;
		case IDC_APPLY:
			Apply ();
			return TRUE;
		}
		break;
	case WM_NOTIFY:
		if (((NMHDR*)lParam)->code == UDN_DELTAPOS) {
			NMUPDOWN *nmud = (NMUPDOWN*)lParam;
			char cbuf[256];
			double val;
			int id = ((NMHDR*)lParam)->idFrom;
			switch (id) {
			case IDC_SPIN1:
			case IDC_SPIN1A:
				GetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf, 256);
				sscanf (cbuf, "%lf", &val);
				val -= nmud->iDelta * (id == IDC_SPIN1 ? 0.001 : 0.1);
				if      (val < -180.0) val += 360.0;
				else if (val > +180.0) val -= 360.0;
				sprintf (cbuf, "%lf", val);
				SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf);
				Apply ();
				return TRUE;
			case IDC_SPIN2:
			case IDC_SPIN2A:
				GetWindowText (GetDlgItem (hDlg, IDC_EDIT2), cbuf, 256);
				sscanf (cbuf, "%lf", &val);
				val -= nmud->iDelta * (id == IDC_SPIN2 ? 0.001 : 0.1);
				val = min (90, max (-90, val));
				sprintf (cbuf, "%lf", val);
				SetWindowText (GetDlgItem (hDlg, IDC_EDIT2), cbuf);
				Apply ();
				return TRUE;
			case IDC_SPIN3:
			case IDC_SPIN3A:
				GetWindowText (GetDlgItem (hDlg, IDC_EDIT3), cbuf, 256);
				sscanf (cbuf, "%lf", &val);
				val -= nmud->iDelta * (id == IDC_SPIN3 ? 0.001 : 0.1);
				if      (val <   0.0) val += 360.0;
				else if (val > 360.0) val -= 360.0;
				sprintf (cbuf, "%lf", val);
				SetWindowText (GetDlgItem (hDlg, IDC_EDIT3), cbuf);
				Apply ();
				return TRUE;
			case IDC_SPIN4:
			case IDC_SPIN5:
			case IDC_SPIN6:
				Rotate (id-IDC_SPIN4, nmud->iDelta * -0.005);
				return TRUE;
			}
		}
		break;
	}
	return ScnEditorTab::TabProc (hDlg, uMsg, wParam, lParam);
}

void EditorTab_Orientation::Refresh ()
{
	int i;
	char cbuf[256];
	VESSEL *vessel = oapiGetVesselInterface (ed->hVessel);
	VECTOR3 arot;
	vessel->GetGlobalOrientation (arot);
	for (i = 0; i < 3; i++) {
		sprintf (cbuf, "%lf", arot.data[i] * DEG);
		SetWindowText (GetDlgItem (hTab, IDC_EDIT1+i), cbuf);
	}
}

void EditorTab_Orientation::Apply ()
{
	int i;
	char cbuf[256];
	VESSEL *vessel = oapiGetVesselInterface (ed->hVessel);
	VECTOR3 arot;
	for (i = 0; i < 3; i++) {
		GetWindowText (GetDlgItem (hTab, IDC_EDIT1+i), cbuf, 256);
		sscanf (cbuf, "%lf", &arot.data[i]);
		arot.data[i] *= RAD;
	}
	vessel->SetGlobalOrientation (arot);
}


void EditorTab_Orientation::Rotate (int axis, double da)
{
	MATRIX3 R, R2;
	VESSEL *vessel = oapiGetVesselInterface (ed->hVessel);
	vessel->GetRotationMatrix (R);
	double sina = sin(da), cosa = cos(da);
	switch (axis) {
	case 0: // pitch
		R2 = _M(1,0,0,  0,cosa,sina,  0,-sina,cosa);
		break;
	case 1: // yaw
		R2 = _M(cosa,0,sina,  0,1,0,  -sina,0,cosa);
		break;
	case 2: // bank
		R2 = _M(cosa,sina,0,  -sina,cosa,0,  0,0,1);
		break;
	}
	vessel->SetRotationMatrix (mul (R,R2));
	Refresh ();
}

INT_PTR EditorTab_Orientation::DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EditorTab_Orientation *pTab = (EditorTab_Orientation*)TabPointer (hDlg, uMsg, wParam, lParam);
	if (!pTab) return FALSE;
	else return pTab->TabProc (hDlg, uMsg, wParam, lParam);
}

// ==============================================================
// EditorTab_AngularVel class definition
// ==============================================================

EditorTab_AngularVel::EditorTab_AngularVel (ScnEditor *editor) : ScnEditorTab (editor)
{
	CreateTab (IDD_TAB_EDIT6, EditorTab_AngularVel::DlgProc);
}

void EditorTab_AngularVel::InitTab ()
{
	Refresh();
}

char *EditorTab_AngularVel::HelpTopic ()
{
	return "/AngularVel.htm";
}

INT_PTR EditorTab_AngularVel::TabProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDC_BACK:
			SwitchTab (3);
			return TRUE;
		case IDC_REFRESH:
			Refresh ();
			return TRUE;
		case IDC_APPLY:
			Apply ();
			return TRUE;
		case IDC_KILLROT:
			Killrot ();
			return TRUE;
		}
		break;
	case WM_NOTIFY:
		if (((NMHDR*)lParam)->code == UDN_DELTAPOS) {
			NMUPDOWN *nmud = (NMUPDOWN*)lParam;
			char cbuf[256];
			double val;
			int id = ((NMHDR*)lParam)->idFrom;
			switch (id) {
			case IDC_SPIN1:
			case IDC_SPIN1A:
				GetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf, 256);
				sscanf (cbuf, "%lf", &val);
				val -= nmud->iDelta * (id == IDC_SPIN4 ? 0.001 : 0.1);
				sprintf (cbuf, "%lf", val);
				SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf);
				Apply ();
				return TRUE;
			case IDC_SPIN2:
			case IDC_SPIN2A:
				GetWindowText (GetDlgItem (hDlg, IDC_EDIT2), cbuf, 256);
				sscanf (cbuf, "%lf", &val);
				val -= nmud->iDelta * (id == IDC_SPIN2 ? 0.001 : 0.1);
				sprintf (cbuf, "%lf", val);
				SetWindowText (GetDlgItem (hDlg, IDC_EDIT2), cbuf);
				Apply ();
				return TRUE;
			case IDC_SPIN3:
			case IDC_SPIN3A:
				GetWindowText (GetDlgItem (hDlg, IDC_EDIT3), cbuf, 256);
				sscanf (cbuf, "%lf", &val);
				val -= nmud->iDelta * (id == IDC_SPIN3 ? 0.001 : 0.1);
				sprintf (cbuf, "%lf", val);
				SetWindowText (GetDlgItem (hDlg, IDC_EDIT3), cbuf);
				Apply ();
				return TRUE;
			}
		}
		break;
	}
	return ScnEditorTab::TabProc (hDlg, uMsg, wParam, lParam);
}

void EditorTab_AngularVel::Refresh ()
{
	char cbuf[256];
	VESSEL *vessel = oapiGetVesselInterface (ed->hVessel);
	VECTOR3 avel;
	vessel->GetAngularVel (avel);
	for (int i = 0; i < 3; i++) {
		sprintf (cbuf, "%lf", avel.data[i] * DEG);
		SetWindowText (GetDlgItem (hTab, IDC_EDIT1+i), cbuf);
	}
}

void EditorTab_AngularVel::Apply ()
{
	char cbuf[256];
	VESSEL *vessel = oapiGetVesselInterface (ed->hVessel);
	VECTOR3 avel;
	for (int i = 0; i < 3; i++) {
		GetWindowText (GetDlgItem (hTab, IDC_EDIT1+i), cbuf, 256);
		sscanf (cbuf, "%lf", &avel.data[i]);
		avel.data[i] *= RAD;
	}
	vessel->SetAngularVel (avel);
}

void EditorTab_AngularVel::Killrot ()
{
	VESSEL *vessel = oapiGetVesselInterface (ed->hVessel);
	VECTOR3 avel;
	for (int i = 0; i < 3; i++) {
		avel.data[i] = 0.0;
		SetWindowText (GetDlgItem (hTab, IDC_EDIT1+i), "0");
	}
	vessel->SetAngularVel (avel);

}

INT_PTR EditorTab_AngularVel::DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EditorTab_AngularVel *pTab = (EditorTab_AngularVel*)TabPointer (hDlg, uMsg, wParam, lParam);
	if (!pTab) return FALSE;
	else return pTab->TabProc (hDlg, uMsg, wParam, lParam);
}


// ==============================================================
// EditorTab_Propellant class definition
// ==============================================================

EditorTab_Propellant::EditorTab_Propellant (ScnEditor *editor) : ScnEditorTab (editor)
{
	CreateTab (IDD_TAB_EDIT7, EditorTab_Propellant::DlgProc);
}

void EditorTab_Propellant::InitTab ()
{
	GAUGEPARAM gp = { 0, 100, GAUGEPARAM::LEFT, GAUGEPARAM::BLACK };
	oapiSetGaugeParams (GetDlgItem (hTab, IDC_PROPLEVEL), &gp);
	lastedit = IDC_EDIT2;
	Refresh();
}

char *EditorTab_Propellant::HelpTopic ()
{
	return "/Propellant.htm";
}

INT_PTR EditorTab_Propellant::TabProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDC_BACK:
			SwitchTab (3);
			return TRUE;
		case IDC_REFRESH:
			Refresh ();
			return TRUE;
		case IDC_APPLY:
			Apply ();
			return TRUE;
		case IDC_EMPTY:
			SetLevel (0);
			return TRUE;
		case IDC_FULL:
			SetLevel (1);
			return TRUE;
		case IDC_EMPTYALL:
			SetLevel (0, true);
			return TRUE;
		case IDC_FULLALL:
			SetLevel (1, true);
			return TRUE;
		case IDC_EDIT2:
		case IDC_EDIT3:
			if (HIWORD(wParam) == EN_CHANGE)
				lastedit = LOWORD(wParam);
			return TRUE;
		}
		break;
	case WM_NOTIFY:
		if (((NMHDR*)lParam)->code == UDN_DELTAPOS) {
			NMUPDOWN *nmud = (NMUPDOWN*)lParam;
			char cbuf[256];
			DWORD i, n;
			int id = ((NMHDR*)lParam)->idFrom;
			switch (id) {
			case IDC_SPIN1:
				GetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf, 256);
				i = sscanf (cbuf, "%d", &n);
				if (!i || !n || n > ntank) n = 1;
				n += (nmud->iDelta < 0 ? 1 : -1);
				n = max (1, min (ntank, n));
				sprintf (cbuf, "%d", n);
				SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf);
				Refresh ();
				return TRUE;
			}
		}
		break;
	case WM_HSCROLL:
		switch (GetDlgCtrlID ((HWND)lParam)) {
		case IDC_PROPLEVEL:
			switch (LOWORD (wParam)) {
			case SB_THUMBTRACK:
			case SB_LINELEFT:
			case SB_LINERIGHT:
				SetLevel (HIWORD(wParam)*0.01);
				return TRUE;
			}
			break;
		}
		break;
	}
	return ScnEditorTab::TabProc (hDlg, uMsg, wParam, lParam);
}

void EditorTab_Propellant::Refresh ()
{
	int i;
	DWORD n;
	double m, m0;
	char cbuf[256];
	VESSEL *vessel = oapiGetVesselInterface (ed->hVessel);
	ntank = vessel->GetPropellantCount();
	sprintf (cbuf, "of %d", ntank);
	SetWindowText (GetDlgItem (hTab, IDC_STATIC1), cbuf);
	GetWindowText (GetDlgItem (hTab, IDC_EDIT1), cbuf, 256);
	i = sscanf (cbuf, "%d", &n);
	if (i != 1 || n > ntank) {
		SetWindowText (GetDlgItem (hTab, IDC_EDIT1), "1");
		n = 0;
	} else n--;
	PROPELLANT_HANDLE hP = vessel->GetPropellantHandleByIndex (n);
	if (!hP) return;
	m0 = vessel->GetPropellantMaxMass (hP);
	m  = vessel->GetPropellantMass (hP);
	sprintf (cbuf, "Mass (0-%0.2f kg)", m0);
	SetWindowText (GetDlgItem (hTab, IDC_STATIC2), cbuf);
	sprintf (cbuf, "%0.4f", m/m0);
	SetWindowText (GetDlgItem (hTab, IDC_EDIT2), cbuf);
	sprintf (cbuf, "%0.2f", m);
	SetWindowText (GetDlgItem (hTab, IDC_EDIT3), cbuf);
	oapiSetGaugePos (GetDlgItem (hTab, IDC_PROPLEVEL), (int)(m/m0*100+0.5));
	RefreshTotals();
}

void EditorTab_Propellant::RefreshTotals ()
{
	double m;
	char cbuf[256];
	VESSEL *vessel = oapiGetVesselInterface (ed->hVessel);
	m = vessel->GetTotalPropellantMass ();
	sprintf (cbuf, "%0.2f kg", m);
	SetWindowText (GetDlgItem (hTab, IDC_STATIC3), cbuf);
	m = vessel->GetMass ();
	sprintf (cbuf, "%0.2f kg", m);
	SetWindowText (GetDlgItem (hTab, IDC_STATIC4), cbuf);
}

void EditorTab_Propellant::Apply ()
{
	char cbuf[256];
	double level;
	VESSEL *vessel = oapiGetVesselInterface (ed->hVessel);
	if (lastedit == IDC_EDIT2) {
		GetWindowText (GetDlgItem (hTab, IDC_EDIT2), cbuf, 256);
		sscanf (cbuf, "%lf", &level);
		level = max (0, min (1, level));
	} else {
		VESSEL *vessel = oapiGetVesselInterface (ed->hVessel);
		GetWindowText (GetDlgItem (hTab, IDC_EDIT1), cbuf, 256);
		DWORD n, i = sscanf (cbuf, "%d", &n);
		if (!i || --n >= ntank) return;
		PROPELLANT_HANDLE hP = vessel->GetPropellantHandleByIndex (n);
		double m, m0 = vessel->GetPropellantMaxMass (hP);
		GetWindowText (GetDlgItem (hTab, IDC_EDIT3), cbuf, 256);
		sscanf (cbuf, "%lf", &m);
		level = max (0, min (1, m/m0));
	}
	SetLevel (level);
}

void EditorTab_Propellant::SetLevel (double level, bool setall)
{
	char cbuf[256];
	int i, j;
	DWORD n, k, k0, k1;
	double m0;
	VESSEL *vessel = oapiGetVesselInterface (ed->hVessel);
	ntank = vessel->GetPropellantCount();
	if (!ntank) return;
	GetWindowText (GetDlgItem (hTab, IDC_EDIT1), cbuf, 256);
	i = sscanf (cbuf, "%d", &n);
	if (!i || --n >= ntank) return;
	if (setall) k0 = 0, k1 = ntank;
	else        k0 = n, k1 = n+1;
	for (k = k0; k < k1; k++) {
		PROPELLANT_HANDLE hP = vessel->GetPropellantHandleByIndex (k);
		m0 = vessel->GetPropellantMaxMass (hP);
		vessel->SetPropellantMass (hP, level*m0);
		if (k == n) {
			sprintf (cbuf, "%f", level);
			SetWindowText (GetDlgItem (hTab, IDC_EDIT2), cbuf);
			sprintf (cbuf, "%0.2f", level*m0);
			SetWindowText (GetDlgItem (hTab, IDC_EDIT3), cbuf);
			i = oapiGetGaugePos (GetDlgItem (hTab, IDC_PROPLEVEL));
			j = (int)(level*100.0+0.5);
			if (i != j) oapiSetGaugePos (GetDlgItem (hTab, IDC_PROPLEVEL), j);
		}
	}
	RefreshTotals();
}

INT_PTR EditorTab_Propellant::DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EditorTab_Propellant *pTab = (EditorTab_Propellant*)TabPointer (hDlg, uMsg, wParam, lParam);
	if (!pTab) return FALSE;
	else return pTab->TabProc (hDlg, uMsg, wParam, lParam);
}


// ==============================================================
// EditorTab_Docking class definition
// ==============================================================

EditorTab_Docking::EditorTab_Docking (ScnEditor *editor) : ScnEditorTab (editor)
{
	CreateTab (IDD_TAB_EDIT8, EditorTab_Docking::DlgProc);
	SendDlgItemMessage (hTab, IDC_RADIO1, BM_SETCHECK, BST_CHECKED, 0);
	SendDlgItemMessage (hTab, IDC_RADIO2, BM_SETCHECK, BST_UNCHECKED, 0);
}

void EditorTab_Docking::InitTab ()
{
	SetWindowText (GetDlgItem (hTab, IDC_EDIT1), "1");
	ScanTargetList();
	Refresh ();
}

char *EditorTab_Docking::HelpTopic ()
{
	return "/Docking.htm";
}

INT_PTR EditorTab_Docking::TabProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDC_BACK:
			SwitchTab (3);
			return TRUE;
		case IDC_REFRESH:
			Refresh ();
			return TRUE;
		case IDC_DOCK:
			Dock ();
			return TRUE;
		case IDC_UNDOCK:
			Undock ();
			Refresh ();
			return TRUE;
		case IDC_IDS:
			ToggleIDS();
			Refresh ();
			return TRUE;
		case IDC_EDIT1:
			if (HIWORD (wParam) == EN_CHANGE)
				if (DockNo()) Refresh();
			return TRUE;
		case IDC_EDIT2:
			if (HIWORD (wParam) == EN_CHANGE)
				IncIDSChannel (0);
			return TRUE;
		case IDC_COMBO1:
			if (HIWORD (wParam) == CBN_SELCHANGE) {
				SetTargetDock (1);
			}
			return TRUE;
		}
		break;
	case WM_NOTIFY:
		if (((NMHDR*)lParam)->code == UDN_DELTAPOS) {
			NMUPDOWN *nmud = (NMUPDOWN*)lParam;
			char cbuf[256];
			DWORD n;
			switch (((NMHDR*)lParam)->idFrom) {
			case IDC_SPIN1:
				GetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf, 256);
				if (!sscanf (cbuf, "%d", &n)) n = 1;
				sprintf (cbuf, "%d", n + (nmud->iDelta < 0 ? 1 : -1));
				SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), cbuf);
				Refresh ();
				return TRUE;
			case IDC_SPIN2:
				IncIDSChannel (nmud->iDelta < 0 ? 1 : -1);
				Refresh ();
				return TRUE;
			case IDC_SPIN2A:
				IncIDSChannel (nmud->iDelta < 0 ? 20 : -20);
				Refresh ();
				return TRUE;
			case IDC_SPIN3:
				GetWindowText (GetDlgItem (hDlg, IDC_EDIT4), cbuf, 256);
				if (!sscanf (cbuf, "%d", &n)) n = 1;
				SetTargetDock (n + (nmud->iDelta < 0 ? 1 : -1));
				return TRUE;
			}
		}
		break;
	}
	return ScnEditorTab::TabProc (hDlg, uMsg, wParam, lParam);
}

UINT EditorTab_Docking::DockNo ()
{
	// returns the id of the dock currently being processed
	// (>= 1, or 0 if invalid)
	char cbuf[256];
	UINT dock;
	GetWindowText (GetDlgItem (hTab, IDC_EDIT1), cbuf, 256);
	int res = sscanf (cbuf, "%d", &dock);
	if (!res || dock > Vessel()->DockCount()) dock = 0;
	return dock;
}

void EditorTab_Docking::ScanTargetList ()
{
	// populate docking target list
	SendDlgItemMessage (hTab, IDC_COMBO1, CB_RESETCONTENT, 0, 0);
	for (DWORD i = 0; i < oapiGetVesselCount(); i++) {
		OBJHANDLE hV = oapiGetVesselByIndex (i);
		VESSEL *v = oapiGetVesselInterface (hV);
		if (v != Vessel() && v->DockCount() > 0) { // only vessels with docking ports make sense here
			SendDlgItemMessage (hTab, IDC_COMBO1, CB_ADDSTRING, 0, (LPARAM)v->GetName());
		}
	}
}

void EditorTab_Docking::ToggleIDS ()
{
	UINT dock = DockNo();
	if (!dock) return;
	DOCKHANDLE hDock = Vessel()->GetDockHandle (dock-1);
	bool enable = (SendDlgItemMessage (hTab, IDC_IDS, BM_GETCHECK, 0, 0) == BST_CHECKED);
	Vessel()->EnableIDS (hDock, enable);
}

void EditorTab_Docking::IncIDSChannel (int dch)
{
	char cbuf[256];
	double freq;
	UINT dock = DockNo();
	if (!dock) return;
	DOCKHANDLE hDock = Vessel()->GetDockHandle (dock-1);
	GetWindowText (GetDlgItem (hTab, IDC_EDIT2), cbuf, 256);
	if (sscanf (cbuf, "%lf", &freq)) {
		int ch = (int)((freq-108.0)*20.0+0.5);
		ch = max(0, min (639, ch+dch));
		Vessel()->SetIDSChannel (hDock, ch);
	}
}

void EditorTab_Docking::Dock ()
{
	char cbuf[256];
	DWORD n, ntgt, mode;
	VESSEL *vessel = Vessel();

	GetWindowText (GetDlgItem (hTab, IDC_COMBO1), cbuf, 256);
	OBJHANDLE hTarget = oapiGetVesselByName (cbuf);
	if (!hTarget) return;
	VESSEL *target = oapiGetVesselInterface (hTarget);
	n = DockNo();
	if (!n) return;
	GetWindowText (GetDlgItem (hTab, IDC_EDIT4), cbuf, 256);
	if (!sscanf (cbuf, "%d", &ntgt) || ntgt < 1 || ntgt > target->DockCount()) {
		DisplayErrorMsg (IDS_ERR4);
		return;
	}
	mode = (SendDlgItemMessage (hTab, IDC_RADIO1, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1:2);
	int res = vessel->Dock (hTarget, n-1, ntgt-1, mode);
	Refresh ();
	if (res) {
		static UINT errmsgid[3] = {IDS_ERR1, IDS_ERR2, IDS_ERR3};
		DisplayErrorMsg (errmsgid[res-1]);
	}
}

void EditorTab_Docking::Undock ()
{
	UINT dock = DockNo();
	if (!dock) return;
	Vessel()->Undock (dock-1);
}

void EditorTab_Docking::Refresh ()
{
	static const int dockitem[7] = {IDC_DOCK, IDC_COMBO1, IDC_EDIT4, IDC_STATIC3, IDC_SPIN3, IDC_RADIO1, IDC_RADIO2};
	static const int undockitem[2] = {IDC_UNDOCK, IDC_EDIT3};

	VESSEL *vessel = Vessel();
	char cbuf[256];
	int i;
	DWORD n, ndock = vessel->DockCount();
	SetWindowText (GetDlgItem (hTab, IDC_ERRMSG), "");

	sprintf (cbuf, "of %d", vessel->DockCount());
	SetWindowText (GetDlgItem (hTab, IDC_STATIC1), cbuf);

	GetWindowText (GetDlgItem (hTab, IDC_EDIT1), cbuf, 256);
	if (!sscanf (cbuf, "%d", &n)) n = 0;
	if (n < 1 || n > ndock) {
		n = max (1, min (ndock, n));
		sprintf (cbuf, "%d", n);
		SetWindowText (GetDlgItem (hTab, IDC_EDIT1), cbuf);
	}
	n--; // zero-based

	DOCKHANDLE hDock = vessel->GetDockHandle (n);
	NAVHANDLE hIDS = vessel->GetIDS (hDock);
	if (hIDS) sprintf (cbuf, "%0.2f", oapiGetNavFreq (hIDS));
	else      strcpy (cbuf, "<none>");
	SetWindowText (GetDlgItem (hTab, IDC_EDIT2), cbuf);
	SendDlgItemMessage (hTab, IDC_IDS, BM_SETCHECK, hIDS ? BST_CHECKED : BST_UNCHECKED, 0);
	EnableWindow (GetDlgItem (hTab, IDC_EDIT2), hIDS ? TRUE:FALSE);
	EnableWindow (GetDlgItem (hTab, IDC_SPIN2), hIDS ? TRUE:FALSE);

	OBJHANDLE hMate = vessel->GetDockStatus (hDock);
	if (hMate) { // dock is engaged
		SetWindowText (GetDlgItem (hTab, IDC_STATIC2), "Currently docked to");
		for (i = 0; i < 7; i++) ShowWindow (GetDlgItem (hTab, dockitem[i]), SW_HIDE);
		for (i = 0; i < 2; i++) ShowWindow (GetDlgItem (hTab, undockitem[i]), SW_SHOW);
		oapiGetObjectName (hMate, cbuf, 256);
		SetWindowText (GetDlgItem (hTab, IDC_EDIT3), cbuf);
	} else { // dock is free
		SetWindowText (GetDlgItem (hTab, IDC_STATIC2), "Establish docking connection with");
		for (i = 0; i < 2; i++) ShowWindow (GetDlgItem (hTab, undockitem[i]), SW_HIDE);
		for (i = 0; i < 7; i++) ShowWindow (GetDlgItem (hTab, dockitem[i]), SW_SHOW);
	}
}

void EditorTab_Docking::SetTargetDock (DWORD dock)
{
	char cbuf[256];
	DWORD n = 0;
	GetWindowText (GetDlgItem (hTab, IDC_COMBO1), cbuf, 256);
	OBJHANDLE hTarget = oapiGetVesselByName (cbuf);
	if (hTarget) {
		VESSEL *v = oapiGetVesselInterface (hTarget);
		DWORD ndock = v->DockCount();
		if (ndock) n = max (1, min (ndock, dock));
	}
	if (n) sprintf (cbuf, "%d", n);
	else   cbuf[0] = '\0';
	SetWindowText (GetDlgItem (hTab, IDC_EDIT4), cbuf);
}

void EditorTab_Docking::DisplayErrorMsg (UINT err)
{
	char cbuf[256] = "Error: ";
	LoadString (ed->InstHandle(), err, cbuf+7, 249);
	SetWindowText (GetDlgItem (hTab, IDC_ERRMSG), cbuf);
}

INT_PTR EditorTab_Docking::DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EditorTab_Docking *pTab = (EditorTab_Docking*)TabPointer (hDlg, uMsg, wParam, lParam);
	if (!pTab) return FALSE;
	else return pTab->TabProc (hDlg, uMsg, wParam, lParam);
}


// ==============================================================
// EditorTab_Custom class definition
// ==============================================================

EditorTab_Custom::EditorTab_Custom (ScnEditor *editor, HINSTANCE hInst, WORD ResId, DLGPROC UserProc) : ScnEditorTab (editor)
{
	usrProc = UserProc;
	CreateTab (hInst, ResId, EditorTab_Custom::DlgProc);
}

void EditorTab_Custom::OpenHelp ()
{
	if (!usrProc (hTab, WM_COMMAND, IDHELP, 0))
		ScnEditorTab::OpenHelp();
}

INT_PTR EditorTab_Custom::TabProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		return usrProc (hDlg, uMsg, wParam, (LPARAM)ed->hVessel);
		break;
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDC_BACK:
			SwitchTab (3);
			return TRUE;
		}
		break;
	case WM_SCNEDITOR:
		switch (LOWORD (wParam)) {
		case SE_GETVESSEL:
			*(OBJHANDLE*)lParam = ed->hVessel;
			return TRUE;
		}
		break;
	}
	return usrProc (hDlg, uMsg, wParam, lParam);
}

INT_PTR EditorTab_Custom::DlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	EditorTab_Custom *pTab = (EditorTab_Custom*)TabPointer (hDlg, uMsg, wParam, lParam);
	if (!pTab) return FALSE;
	else return pTab->TabProc (hDlg, uMsg, wParam, lParam);
}

*/
