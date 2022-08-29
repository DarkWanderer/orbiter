#define ORBITER_MODULE
#include "glad.h"
#include "OGLClient.h"
#include "Scene.h"
#include "VObject.h"
#include "VStar.h"
#include "Texture.h"
#include "tilemgr2.h"
#include "TileMgr.h"
#include "HazeMgr.h"
#include <glm/gtc/matrix_transform.hpp>
#include <fontconfig/fontconfig.h>
#include <imgui.h>
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#include "Particle.h"
#include "VVessel.h"
#include "OGLPad.h"
#include "OGLCamera.h"
#include <fontconfig/fontconfig.h>

//extern Orbiter *g_pOrbiter;
/*
const uint32_t SPEC_DEFAULT = (uint32_t)(-1); // "default" material/texture flag
const uint32_t SPEC_INHERIT = (uint32_t)(-2); // "inherit" material/texture flag
*/
static void CheckError(const char *s) {
	GLenum err;
	while((err = glGetError()) != GL_NO_ERROR)
	{
	// Process/log the error.
		printf("GLError: %s - 0x%04X\n", s, err);
		abort();
		exit(-1);
	}
}

GLboolean glEnableEx(GLenum e) {
	GLboolean ret;
	glGetBooleanv(e, &ret);
	glEnable(e);
	return ret; 
}
GLboolean glDisableEx(GLenum e) {
	GLboolean ret;
	glGetBooleanv(e, &ret);
	glDisable(e);
	return ret; 
}

void glRestoreEx(GLenum e, GLboolean v) {
	if(v)
		glEnable(e);
	else
		glDisable(e);
}

// ==============================================================
// API interface
// ==============================================================

using namespace oapi;

OGLClient *g_client = 0;

// ==============================================================
// Initialise module
DLLCLBK void InitModule (MODULEHANDLE hDLL)
{
	FcInit();
	g_client = new OGLClient (hDLL);
	if (!oapiRegisterGraphicsClient (g_client)) {
		delete g_client;
		g_client = 0;
	}
}

// ==============================================================
// Clean up module

DLLCLBK void ExitModule (MODULEHANDLE hDLL)
{
	FcFini();
	if (g_client) {
		oapiUnregisterGraphicsClient (g_client);
		delete g_client;
		g_client = 0;
	}
	FcFini();
}

OGLClient::OGLClient (MODULEHANDLE hInstance):GraphicsClient(hInstance)
{
}

OGLClient::~OGLClient ()
{
}

void OGLClient::Style()
{
	ImGuiStyle & style = ImGui::GetStyle();
	ImVec4 * colors = style.Colors;
	
	/// 0 = FLAT APPEARENCE
	/// 1 = MORE "3D" LOOK
	int is3D = 1;

	colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled]           = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
	colors[ImGuiCol_ChildBg]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_WindowBg]               = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_PopupBg]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_Border]                 = ImVec4(0.12f, 0.12f, 0.12f, 0.71f);
	colors[ImGuiCol_BorderShadow]           = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_FrameBg]                = ImVec4(0.42f, 0.42f, 0.42f, 0.54f);
	colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.42f, 0.42f, 0.42f, 0.40f);
	colors[ImGuiCol_FrameBgActive]          = ImVec4(0.56f, 0.56f, 0.56f, 0.67f);
	colors[ImGuiCol_TitleBg]                = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
	colors[ImGuiCol_TitleBgActive]          = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.17f, 0.17f, 0.17f, 0.90f);
	colors[ImGuiCol_MenuBarBg]              = ImVec4(0.335f, 0.335f, 0.335f, 1.000f);
	colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.24f, 0.24f, 0.24f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
	colors[ImGuiCol_CheckMark]              = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
	colors[ImGuiCol_SliderGrab]             = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
	colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.64f, 0.64f, 0.64f, 1.00f);
	colors[ImGuiCol_Button]                 = ImVec4(0.54f, 0.54f, 0.54f, 0.35f);
	colors[ImGuiCol_ButtonHovered]          = ImVec4(0.52f, 0.52f, 0.52f, 0.59f);
	colors[ImGuiCol_ButtonActive]           = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
	colors[ImGuiCol_Header]                 = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	colors[ImGuiCol_HeaderHovered]          = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
	colors[ImGuiCol_HeaderActive]           = ImVec4(0.76f, 0.76f, 0.76f, 0.77f);
	colors[ImGuiCol_Separator]              = ImVec4(0.000f, 0.000f, 0.000f, 0.137f);
	colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.700f, 0.671f, 0.600f, 0.290f);
	colors[ImGuiCol_SeparatorActive]        = ImVec4(0.702f, 0.671f, 0.600f, 0.674f);
	colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
	colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.73f, 0.73f, 0.73f, 0.35f);
	colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);

	style.PopupRounding = 3;

	style.WindowPadding = ImVec2(4, 4);
	style.FramePadding  = ImVec2(6, 4);
	style.ItemSpacing   = ImVec2(6, 2);

	style.ScrollbarSize = 18;

	style.WindowBorderSize = 1;
	style.ChildBorderSize  = 1;
	style.PopupBorderSize  = 1;
	style.FrameBorderSize  = is3D; 

	style.WindowRounding    = 3;
	style.ChildRounding     = 3;
	style.FrameRounding     = 3;
	style.ScrollbarRounding = 2;
	style.GrabRounding      = 3;

	#ifdef IMGUI_HAS_DOCK 
		style.TabBorderSize = is3D; 
		style.TabRounding   = 3;

		colors[ImGuiCol_DockingEmptyBg]     = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
		colors[ImGuiCol_Tab]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
		colors[ImGuiCol_TabHovered]         = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
		colors[ImGuiCol_TabActive]          = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
		colors[ImGuiCol_TabUnfocused]       = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
		colors[ImGuiCol_DockingPreview]     = ImVec4(0.85f, 0.85f, 0.85f, 0.28f);

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->AddFontFromFileTTF("Roboto-Medium.ttf", 12);

	#endif
}

/**
 * \brief Perform any one-time setup tasks.
 *
 * This includes enumerating drivers, graphics modes, etc.
 * Derived classes should also call the base class method to allow
 * default setup.
 * \default Initialises the VideoData structure from the Orbiter.cfg
 *   file
 * \par Calling sequence:
 *   Called during processing of oapiRegisterGraphicsClient, after the
 *   Launchpad Video tab has been inserted (if clbkUseLaunchpadVideoTab
 *   returns true).
 */
bool OGLClient::clbkInitialise()
{
    if (!GraphicsClient::clbkInitialise ()) return false;
    return true;
}

/**
 * \brief Message callback for a visual object.
 * \param hObj handle of the object that created the message
 * \param vis client-supplied identifier for the visual
 * \param msg event identifier
 * \param context message context
 * \return Function should return 1 if it processes the message, 0 otherwise.
 * \default None, returns 0.
 * \note Messages are generated by Orbiter for objects that have been
 *   registered with \ref RegisterVisObject by the client, until they are
 *   un-registered with \ref UnregisterVisObject.
 * \note Currently only vessel objects create visual messages.
 * \note For currently supported event types, see \ref visevent.
 * \note The \e vis pointer passed to this function is the same as that provided
 *   by RegisterVisObject. It can be used by the client to identify the visual
 *   object for which the message was created.
 * \sa RegisterVisObject, UnregisterVisObject, visevent
 */
int OGLClient::clbkVisEvent (OBJHANDLE hObj, VISHANDLE vis, visevent msg, visevent_data context)
{
    vObject *vo = (vObject*)vis;
	vo->clbkEvent (msg, context);
	return 1;
}

ParticleStream *OGLClient::clbkCreateParticleStream (PARTICLESTREAMSPEC *pss)
{
    return NULL;
}

ParticleStream *OGLClient::clbkCreateExhaustStream (PARTICLESTREAMSPEC *pss,
		OBJHANDLE hVessel, const double *lvl, const VECTOR3 *ref, const VECTOR3 *dir)
{
	ExhaustStream *es = new ExhaustStream (this, hVessel, lvl, ref, dir, pss);
	g_client->GetScene()->AddParticleStream (es);
	return es;
}
ParticleStream *OGLClient::clbkCreateExhaustStream (PARTICLESTREAMSPEC *pss,
		OBJHANDLE hVessel, const double *lvl, const VECTOR3 &ref, const VECTOR3 &dir)
{
	ExhaustStream *es = new ExhaustStream (this, hVessel, lvl, ref, dir, pss);
	g_client->GetScene()->AddParticleStream (es);
	return es;
}

ParticleStream *OGLClient::clbkCreateReentryStream (PARTICLESTREAMSPEC *pss,
		OBJHANDLE hVessel)
{
	ReentryStream *rs = new ReentryStream (this, hVessel, pss);
	g_client->GetScene()->AddParticleStream (rs);
	return rs;
}

/**
 * \brief Fullscreen mode flag
 * \return true if the client is set up for running in fullscreen
 *   mode, false for windowed mode.
 */
bool OGLClient::clbkFullscreenMode () const
{
	return false;
}

/**
 * \brief Returns the dimensions of the render viewport
 * \param width render viewport width [pixel]
 * \param height render viewport height [pixel]
 * \note This function is called by orbiter after the render window or
 *   fullscreen renderer has been created (see \ref clbkCreateRenderWindow).
 * \note This should normally return the screen resolution in fullscreen
 *   mode, and the size of the render window client area in windowed mode,
 *   clients can also return smaller values if they only use part of the
 *   screen area for scene rendering.
 */
void OGLClient::clbkGetViewportSize (int *w, int *h) const
{
	*w = m_width, *h = m_height;
}

/**
 * \brief Returns a specific render parameter
 * \param[in] prm parameter identifier (see \sa renderprm)
 * \param[out] value value of the queried parameter
 * \return true if the specified parameter is supported by the client,
 *    false if not.
 */
bool OGLClient::clbkGetRenderParam (int prm, int *value) const
{
	switch (prm) {
	case RP_COLOURDEPTH:
		*value = 32;
		return true;
	case RP_ZBUFFERDEPTH:
		*value = 24;
		return true;
	case RP_STENCILDEPTH:
		*value = 8;
		return true;
	case RP_MAXLIGHTS:
		*value = 8;
		return true;
	case RP_ISTLDEVICE:
		*value = 1;
		return true;
	case RP_REQUIRETEXPOW2:
		*value = 0;
		return true;
	}
	return false;
}

/**
 * \brief Render an instrument panel in cockpit view as a 2D billboard.
 * \param hSurf array of texture handles for the panel surface
 * \param hMesh billboard mesh handle
 * \param T transformation matrix for panel mesh vertices (2D)
 * \param additive If true, panel should be rendered additive (transparent)
 * \default None.
 * \note The texture index of each group in the mesh is interpreted as index into the
 *   hSurf array. Special indices are TEXIDX_MFD0 and above, which specify the
 *   surfaces representing the MFD displays. These are obtained separately and
 *   don't need to be present in the hSurf list.
 * \note The \e additive flag is used when rendering the default "glass
 *   cockpit" if the user requested. "transparent MFDs". The renderer can
 *   then use e.g. additive blending for rendering the panel.
 */
void OGLClient::clbkRender2DPanel (SURFHANDLE *hSurf, MESHHANDLE hMesh, MATRIX3 *T, bool transparent)
{
	static int nHVTX = 1;
	static struct HVTX {
		float tu, tv;
		float x, y;
	} *hvtx = new HVTX[1];

	GLboolean oldDepth = glDisableEx(GL_DEPTH_TEST);
	GLboolean oldBlend = glEnableEx(GL_BLEND);
	GLboolean oldCull = glDisableEx(GL_CULL_FACE);

	glm::mat4 ortho_proj = glm::ortho(0.0f, (float)g_client->GetScene()->GetCamera()->GetWidth(), (float)g_client->GetScene()->GetCamera()->GetHeight(), 0.0f);
	static Shader s("Overlay.vs","Overlay.fs");
//	static glm::vec3 vecTextColor = glm::vec3(1,1,1);

	static GLuint m_Buffer, m_VAO, IBO;
	static bool init = false;
	if(!init) {
		init=true;
		glGenVertexArrays(1, &m_VAO);
		CheckError("clbkRender2DPanel glGenVertexArrays");
		glBindVertexArray(m_VAO);
		CheckError("clbkRender2DPanel glBindVertexArray");

		glGenBuffers(1, &m_Buffer);
		CheckError("clbkRender2DPanel glGenBuffers");
		glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
		CheckError("clbkRender2DPanel glBindBuffer");
		glBufferData(GL_ARRAY_BUFFER, 2048 * 4 * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
		CheckError("clbkRender2DPanel glBufferData");
		glGenBuffers(1, &IBO);
		CheckError("clbkRender2DPanel glGenBuffers");

		glBindVertexArray(0);
	}
	s.Bind();
	s.SetMat4("projection", ortho_proj);
	s.SetFloat("color_keyed", 0.0);

	//s.SetVec3("font_color", vecTextColor);

/*
	int vtxFmt = D3DFVF_XYZRHW | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0);
	int dAlpha;
	pd3dDevice->GetRenderState (D3DRENDERSTATE_ALPHABLENDENABLE, &dAlpha);
	pd3dDevice->SetRenderState (D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
	pd3dDevice->SetTextureStageState (0, D3DTSS_ADDRESS, D3DTADDRESS_CLAMP);
	if (transparent)
		pd3dDevice->SetRenderState (D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE);
	float rhw = 1;
	*/

	//if (transparent)
	//	glBlendFunc(GL_DST_ALPHA, GL_ONE);

	int i, j, nvtx, ngrp = oapiMeshGroupCount (hMesh);
	SURFHANDLE surf = 0, newsurf = 0;

	float scalex = (float)(T->m11),  dx = (float)(T->m13);
	float scaley = (float)(T->m22),  dy = (float)(T->m23);

	for (i = 0; i < ngrp; i++) {
		MESHGROUP *grp = oapiMeshGroup (hMesh, i);
		if (grp->UsrFlag & 2) continue; // skip this group

		if (grp->TexIdx == SPEC_DEFAULT) {//SPEC_DEFAULT) {
			newsurf = 0;
		} else if (grp->TexIdx == SPEC_INHERIT) {//SPEC_INHERIT) {
			// nothing to do
		} else if ((unsigned)grp->TexIdx >= (unsigned)TEXIDX_MFD0) {
			int mfdidx = grp->TexIdx-TEXIDX_MFD0;
			newsurf = GetMFDSurface (mfdidx);
			if (!newsurf) continue;
			//OGLTexture *ttt = (OGLTexture *)newsurf;
			//printf("MFD surface w=%d h=%d\n", ttt->m_Width, ttt->m_Height);
		} else if (hSurf) {
			newsurf = hSurf[grp->TexIdx];
		} else {
			newsurf = oapiGetTextureHandle (hMesh, grp->TexIdx+1);
		}
		if (newsurf != surf) {
//			pd3dDevice->SetTexture (0, (LPDIRECTDRAWSURFACE7)(surf = newsurf));
			surf = newsurf;
			glBindTexture(GL_TEXTURE_2D, ((OGLTexture *)surf)->m_TexId);
		}

		nvtx = grp->nVtx;
		if (nvtx > nHVTX) { // increase buffer size
			delete []hvtx;
			hvtx = new HVTX[nHVTX = nvtx];
		}
		for (j = 0; j < nvtx; j++) {
			HVTX *tgtvtx = hvtx+j;
			NTVERTEX *srcvtx = grp->Vtx+j;
			tgtvtx->x = srcvtx->x*scalex + dx;
			tgtvtx->y = srcvtx->y*scaley + dy;
			tgtvtx->tu = srcvtx->tu;
			tgtvtx->tv = srcvtx->tv;
		}
//		pd3dDevice->DrawIndexedPrimitive (D3DPT_TRIANGLELIST, vtxFmt, hvtx, nvtx, grp->Idx, grp->nIdx, 0);
		glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
		glBindVertexArray(m_VAO);
		CheckError("clbkRender2DPanel glBindBuffer");
		glBufferSubData(GL_ARRAY_BUFFER, 0, nvtx * sizeof(HVTX), hvtx);
		CheckError("clbkRender2DPanel glBufferSubData");
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
		CheckError("clbkRender2DPanel glVertexAttribPointer");
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		CheckError("clbkRender2DPanel glBindBuffer");
		
		glEnableVertexAttribArray(0);
		CheckError("clbkRender2DPanel glEnableVertexAttribArray0");

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
		CheckError("clbkRender2DPanel glBindBuffer");
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, grp->nIdx * 2, grp->Idx, GL_STATIC_DRAW);
		CheckError("clbkRender2DPanel glBufferData");

		glDrawElements(GL_TRIANGLES, grp->nIdx, GL_UNSIGNED_SHORT, 0);
	//		glDrawArrays(GL_TRIANGLES, 0, nvtx);
		CheckError("clbkRender2DPanel glDrawArrays");

		CheckError("clbkRender2DPanel glDrawArrays");
		glBindVertexArray(0);



	}
	s.UnBind();
	glBindTexture(GL_TEXTURE_2D, 0);

	//if (transparent)
	//	glBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA);

	glRestoreEx(GL_DEPTH_TEST, oldDepth);
	glRestoreEx(GL_BLEND, oldBlend);
	glRestoreEx(GL_CULL_FACE, oldCull);
}

/**
 * \brief Render an instrument panel in cockpit view as a 2D billboard.
 * \param hSurf array of texture handles for the panel surface
 * \param hMesh billboard mesh handle
 * \param T transformation matrix for panel mesh vertices (2D)
 * \param alpha opacity value, between 0 (transparent) and 1 (opaque)
 * \param additive If true, panel should be rendered additive (transparent)
 * \default None.
 * \note The texture index of each group in the mesh is interpreted as index into the
 *   hSurf array. Special indices are TEXIDX_MFD0 and above, which specify the
 *   surfaces representing the MFD displays. These are obtained separately and
 *   don't need to be present in the hSurf list.
 * \note The \e additive flag is used when rendering the default "glass
 *   cockpit" if the user requested. "transparent MFDs". The renderer can
 *   then use e.g. additive blending for rendering the panel.
 */
void OGLClient::clbkRender2DPanel (SURFHANDLE *hSurf, MESHHANDLE hMesh, MATRIX3 *T, float alpha, bool additive)
{
	/*
	bool reset = false;
	int alphaop, alphaarg2, tfactor;
	if (alpha < 1.0f) {
		pd3dDevice->GetTextureStageState (0, D3DTSS_ALPHAOP, &alphaop);
		pd3dDevice->GetTextureStageState (0, D3DTSS_ALPHAARG2, &alphaarg2);
		pd3dDevice->GetRenderState (D3DRENDERSTATE_TEXTUREFACTOR, &tfactor);
		pd3dDevice->SetTextureStageState (0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		pd3dDevice->SetTextureStageState (0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
		pd3dDevice->SetRenderState (D3DRENDERSTATE_TEXTUREFACTOR, D3DRGBA(1,1,1,alpha));
		reset = true;
	}*/
	clbkRender2DPanel (hSurf, hMesh, T, additive);
/*
	if (reset) {
		pd3dDevice->SetTextureStageState (0, D3DTSS_ALPHAOP, alphaop);
		pd3dDevice->SetTextureStageState (0, D3DTSS_ALPHAARG2, alphaarg2);
		pd3dDevice->SetRenderState (D3DRENDERSTATE_TEXTUREFACTOR, tfactor);
	}
*/
}

/**
 * \brief Simulation session start notification
 *
 * Called at the beginning of a simulation session to allow the client
 * to create the 3-D rendering window (or to switch into fullscreen
 * mode).
 * \return Should return window handle of the rendering window.
 * \default For windowed mode, opens a window of the size specified by the
 *   VideoData structure (for fullscreen mode, opens a small dummy window)
 *   and returns the window handle.
 * \note For windowed modes, the viewW and viewH parameters should return
 *   the window client area size. For fullscreen mode, they should contain
 *   the screen resolution.
 * \note Derived classes should perform any required per-session
 *   initialisation of the 3D render environment here.
 */

/*
static void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
	g_client->SetSize(width, height);
}*/
//#include "Camera.h"
//extern Camera *g_camera;
void OGLClient::clbkSetViewportSize(int w, int h) {
	m_width = w;
	m_height = h;
	if(mScene) mScene->GetCamera()->SetSize(w, h);
	glViewport(0, 0, m_width, m_height);
//	if(g_camera) g_camera->ResizeViewport(w,h);
}

GLFWwindow *OGLClient::clbkCreateRenderWindow ()
{
	/* Initialize the library */
    if (!glfwInit()) {
		printf("glfwInit failed\n");
		exit(EXIT_FAILURE);
	}

    glfwWindowHint(GLFW_DOUBLEBUFFER , 1);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
	glfwWindowHint(GLFW_FOCUS_ON_SHOW, false);

    glfwWindowHint(
        GLFW_OPENGL_PROFILE,
        GLFW_OPENGL_CORE_PROFILE
        );
	
//	glfwWindowHint(GLFW_SAMPLES, 4);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

//	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    /* Create a windowed mode window and its OpenGL context */
    hRenderWnd = glfwCreateWindow(1280, 800, "xorbiter", NULL, NULL);
	m_width = 1280;
	m_height = 800;
    if (!hRenderWnd)
    {
		printf("!hRenderWnd\n");
        glfwTerminate();
        exit(-1);
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(hRenderWnd);
    glfwSwapInterval(1); // VSync

    if(!gladLoadGL())
    {
        printf("OGLClient: gladLoadGL failed\n");
        exit(-1);
    }

	glViewport(0, 0, m_width, m_height);

//	glfwSetFramebufferSizeCallback(hRenderWnd, framebuffer_size_callback);

	//ValidateRect (hRenderWnd, NULL);
	// avoids white flash after splash screen
	int num_ext = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &num_ext);
	for(int i=0; i<num_ext; i++)
		if(!strcmp((const char *)glGetStringi(GL_EXTENSIONS,i), "GL_ARB_compatibility")) {
			printf("Compatiblity Profile");
			exit(-1);
		}

	mMeshManager = std::make_unique<OGLMeshManager>();
	mScene = std::make_unique<Scene>(m_width, m_height);
	mTextureManager = std::make_unique<TextureManager>();
	TileManager::GlobalInit ();
	HazeManager::GlobalInit();

	TileManager2Base::GlobalInit();
	vStar::GlobalInit ();
	OGLParticleStream::GlobalInit();
	vVessel::GlobalInit();
	OGLPad::OGLPad::GlobalInit();
	mBlitShader = std::make_unique<Shader>("Blit.vs","Blit.fs");
	glEnable( GL_LINE_SMOOTH );
	glHint( GL_LINE_SMOOTH_HINT, GL_NICEST );

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
//	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
//	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
//	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	//ImGui::StyleColorsDark();
	ImGui::StyleColorsClassic();
	Style();

    ImGui_ImplGlfw_InitForOpenGL(hRenderWnd, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	return hRenderWnd;
}

void OGLClient::clbkMakeContextCurrent(bool b) {
	glfwMakeContextCurrent(b ? hRenderWnd : nullptr);
}

/**
 * \brief End of simulation session notification
 *
 * Called before the end of a simulation session. At the point of call,
 * logical objects still exist (OBJHANDLEs valid), and external modules
 * are still loaded.
 * \param fastclose Indicates a "fast shutdown" request (see notes)
 * \default None.
 * \note Derived clients can use this function to perform cleanup operations
 *   for which the simulation objects are still required.
 * \note If fastclose == true, the user has selected one of the fast
 *   shutdown options (terminate Orbiter, or respawn Orbiter process). In
 *   this case, the current process will terminate, and the graphics client
 *   can skip object cleanup and deallocation in order to speed up the
 *   closedown process.
 * \sa clbkDestroyRenderWindow
 */
void OGLClient::clbkDestroyRenderWindow (bool fastclose)
{
	GraphicsClient::clbkDestroyRenderWindow (fastclose);
	//Cleanup3DEnvironment();
	hRenderWnd = NULL;
}

/**
 * \brief Per-frame render notification
 *
 * Called once per frame, after the logical world state has been updated,
 * to allow the client to render the current scene.
 * \note This method is also called continuously while the simulation is
 *   paused, to allow camera panning (although in that case the logical
 *   world state won't change between frames).
 * \note After the 3D scene has been rendered, this function should call
 *   \ref Render2DOverlay to initiate rendering of 2D elements (2D instrument
 *   panel, HUD, etc.)
 */
void OGLClient::clbkRenderScene ()
{
	//scene->Render();
//	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
//	CheckError("glClearColor");
//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//	CheckError("glClear");
	mScene->Render();
}

/**
 * \brief Texture request
 *
 * Load a texture from a file into a device-specific texture object, and
 * return a generic SURFHANDLE for it. Derived classes should overload this
 * method to add texture support.
 * Usually, the client should read Orbiter's default texture files (in
 * DXT? format). However, the client also has the option to load its own
 * texture files stored in a different format, and pass them back via the
 * SUFHANDLE interface.
 * \param fname texture file name with path relative to orbiter
 *   texture folders; can be used as input for OpenTextureFile.
 * \param flags request for texture properties
 * \return Texture handle, cast into generic SURFHANDLE, or NULL if texture
 *   could not be loaded.
 * \default Return NULL.
 * \note If the client loads its own of texture files, they can either be
 *   installed in the default locations, replacing Orbiter's set of
 *   textures, or stored alongside the original textures, using different
 *   names or directory locations. In the latter case, the fname parameter
 *   passed to clbkLoadTexture must be adapted accordingly (for example,
 *   by replacing the dds extension with jpg, or by adding an 'OGL/'
 *   prefix to the path name, etc). Not overwriting the original texture
 *   set has the advantage that other graphics clients relying on the
 *   original textures can still be used.
 * \note The following flags are supported:
 *   - bit 0 set: force creation in system memory
 *   - bit 1 set: decompress, even if format is supported by device
 *   - bit 2 set: don't load mipmaps, even if supported by device
 *   - bit 3 set: load as global resource (can be managed by graphics client)
 * \note If bit 3 of flags is set, orbiter will not try to modify or release
 *   the texture. The client should manage the texture (i.e. keep it in a
 *   repository and release it at destruction). Any further call of
 *   clbkLoadTexture should first scan the repository. If the texture is
 *   already present, the function should just return a pointer to it.
 */
SURFHANDLE OGLClient::clbkLoadTexture (const char *fname, int flags)
{
	if (!mTextureManager) return NULL;
	OGLTexture *tex = NULL;

	if (flags & 8) // load managed
		mTextureManager->GetTexture (fname, &tex, flags);
	else           // load individual
		mTextureManager->LoadTexture (fname, &tex, flags);

	return (SURFHANDLE)tex;
}

/**
 * \brief Texture release request
 *
 * Called by Orbiter when a previously loaded texture can be released
 * from memory. The client can use the appropriate device-specific method
 * to release the texture.
 * \param hTex texture handle
 * \default None.
 */
void OGLClient::clbkReleaseTexture (SURFHANDLE hTex)
{
	// We ignore the request for texture deallocation at this point,
	// and leave it to the texture manager to perform cleanup operations.
	// But it would be a good idea to decrement a reference count at
	// this point.
}

/**
 * \brief Load a surface from file into a surface object, and return a SURFHANDLE for it.
 * \param fname texture file name with path relative to orbiter texture folders
 * \param attrib \ref surfacecaps (see notes)
 * \return A SURFHANDLE for the loaded surface, for example a pointer to the surface object.
 * \note If the request refers to a static surface that has already be loaded, or if the
 *   client buffers the unmodified surfaces after loading, it can simply return a handle to
 *   the existing surface object, instead of reading it again from file.
 * \note The attrib bitflag can contain one of the following main attributes:
 *  - OAPISURFACE_RO: Load the surface to be readable by the GPU pipeline
 *  - OAPISURFACE_RW: Load the surface to be readable and writable by the GPU pipeline
 *  - OAPISURFACE_GDI: Load the surface to be readable and writable by the CPU, and can be blitted into an uncompressed RO or RW surface without alpha channel
 *  - OAPISURFACE_STATIC: Load the surface to be readable by the GPU pipeline
 *  In addition, the flag can contain any of the following auxiliary attributes:
 *  - OAPISURFACE_MIPMAPS: Load the mipmaps for the surface from file, or create them if necessary
 *  - OAPISURFACE_NOMIPMAPS: Don't load mipmaps, even if they are available in the file
 *  - OAPISURFACE_NOALPHA: Load the surface without an alpha channel
 *  - OAPISURFACE_UNCOMPRESS: Uncompress the surface on loading.
 * \sa oapiCreateSurface(int,int,int)
 */
SURFHANDLE OGLClient::clbkLoadSurface (const char *fname, int flags)
{
    printf("OGLClient::clbkLoadSurface %s\n", fname);
	exit(-1);
    return NULL;
}

/**
 * \brief Save the contents of a surface to a formatted image file or to the clipboard
 * \param surf surface handle (0 for primary render surface)
 * \param fname image file path relative to orbiter root directory (excluding file extension), or NULL to save to clipboard
 * \param fmt output file format
 * \param quality quality request if the format supports it (0-1)
 * \return Should return true on success
 * \default Nothing, returns false
 * \note Implementations can make use of the \ref WriteImageDataToFile method to write to
 *   a file in the desired format once a pointer to the image data in 24-bit uncompressed
 *   format has been obtained.
 */
bool OGLClient::clbkSaveSurfaceToImage (SURFHANDLE surf, const char *fname,
		ImageFileFormat fmt, float quality)
{
    return false;
}

/**
 * \brief Replace a texture in a device-specific mesh.
 * \param hMesh device mesh handle
 * \param texidx texture index (>= 0)
 * \param tex texture handle
 * \return Should return \e true if operation successful, \e false otherwise.
 * \default None, returns \e false.
 */
bool OGLClient::clbkSetMeshTexture (DEVMESHHANDLE hMesh, int texidx, SURFHANDLE tex)
{
	return ((OGLMesh*)hMesh)->SetTexture (texidx, tex);
}

/**
 * \brief Replace properties of an existing mesh material.
 * \param hMesh device mesh handle
 * \param matidx material index (>= 0)
 * \param mat pointer to material structure
 * \return Overloaded functions should return an integer error flag, with
 *   the following codes: 0="success", 3="invalid mesh handle", 4="material index out of range"
 * \default None, returns 2 ("client does not support operation").
 */
int OGLClient::clbkSetMeshMaterial (DEVMESHHANDLE hMesh, int matidx, const MATERIAL *mat)
{
	OGLMesh *mesh = (OGLMesh*)hMesh;
	int nmat = mesh->MaterialCount();
	if (matidx >= nmat) return 4; // "index out of range"
	OGLMaterial *meshmat = mesh->GetMaterial (matidx);
	memcpy (meshmat, mat, sizeof(OGLMaterial)); // relies on OGLMaterial and MATERIAL to be equivalent
	return 0;
}

/**
 * \brief Retrieve the properties of one of the mesh materials.
 * \param hMesh device mesh handle
 * \param matidx material index (>= 0)
 * \param mat [out] pointer to MATERIAL structure to be filled by the method.
 * \return true if successful, false on error (index out of range)
 * \default None, returns 2 ("client does not support operation").
 */
int OGLClient::clbkMeshMaterial (DEVMESHHANDLE hMesh, int matidx, MATERIAL *mat)
{
    printf("OGLClient::clbkMeshMaterial\n");
exit(-1);
	return 0;
}

/**
 * \brief Popup window open notification.
 * \note This method is called just before a popup window (e.g. dialog
 *   box) is opened. It allows the client to prepare for subsequent
 *   rendering of the window, if necessary.
 */
void OGLClient::clbkPreOpenPopup ()
{
	//if (clipper) pDD->FlipToGDISurface();
}

void OGLClient::clbkNewVessel (OBJHANDLE hVessel)
{
	mScene->NewVessel (hVessel);
}

// ==============================================================

void OGLClient::clbkDeleteVessel (OBJHANDLE hVessel)
{
	mScene->DeleteVessel (hVessel);
}

/**
 * \brief Return the width and height of a surface
 * \param[in] surf surface handle
 * \param[out] w surface width
 * \param[out] h surface height
 * \return true if surface dimensions could be obtained.
 * \default Sets w and h to 0 and returns false.
 * \sa clbkCreateSurface
 */
bool OGLClient::clbkGetSurfaceSize (SURFHANDLE surf, int *w, int *h)
{
	OGLTexture *tex = (OGLTexture *)surf;
	*w = tex->m_Width;
	*h = tex->m_Height;
	return true;
}

/**
 * \brief Set custom properties for a device-specific mesh.
 * \param hMesh device mesh handle
 * \param property property tag
 * \param value new mesh property value
 * \return The method should return \e true if the property tag was recognised
 *   and the request could be executed, \e false otherwise.
 * \note Currently only a single mesh property request type will be sent, but this may be
 *  extended in future versions:
 * - \c MESHPROPERTY_MODULATEMATALPHA \n \n
 * if value==0 (default) disable material alpha information in textured mesh groups (only use texture alpha channel).\n
 * if value<>0 modulate (mix) material alpha values with texture alpha maps.
 * \default None, returns \e false.
 */
bool OGLClient::clbkSetMeshProperty (DEVMESHHANDLE hMesh, int property, int value)
{
//    printf("OGLClient::clbkSetMeshProperty\n");
	OGLMesh *mesh = (OGLMesh*)hMesh;
	switch (property) {
	case MESHPROPERTY_MODULATEMATALPHA:
		mesh->EnableMatAlpha (value != 0);
		return true;
	}
	return false;
}

/**
 * \brief Return a mesh handle for a visual, defined by its index
 * \param vis visual identifier
 * \param idx mesh index (>= 0)
 * \return Mesh handle (client-specific)
 * \note Derived clients should return a handle that identifies a
 *   mesh for the visual (in client-specific format).
 * \note Orbiter calls this method in response to a \ref VESSEL::GetMesh
 *   call by an vessel module.
 */
MESHHANDLE OGLClient::clbkGetMesh (VISHANDLE vis, unsigned int idx)
{
	return (vis ? ((vObject*)vis)->GetMesh (idx) : NULL);
}

/**
 * \brief Mesh group data retrieval interface for device-specific meshes.
 * \param hMesh device mesh handle
 * \param grpidx mesh group index (>= 0)
 * \param grs data buffers and buffer size information. See \ref oapiGetMeshGroup
 *    for details.
 * \return Should return 0 on success, or error flags > 0.
 * \default None, returns -2.
 */
int OGLClient::clbkGetMeshGroup (DEVMESHHANDLE hMesh, int grpidx, GROUPREQUESTSPEC *grs)
{
	return ((OGLMesh*)hMesh)->GetGroup (grpidx, grs);
}

/**
 * \brief Mesh group editing interface for device-specific meshes.
 * \param hMesh device mesh handle
 * \param grpidx mesh group index (>= 0)
 * \param ges mesh group modification specs
 * \return Should return 0 on success, or error flags > 0.
 * \default None, returns -2.
 * \note Clients should implement this method to allow the modification
 *   of individual groups in a device-specific mesh. Modifications may
 *   include vertex values, index lists, texture and material indices,
 *   and user flags.
 */
int OGLClient::clbkEditMeshGroup (DEVMESHHANDLE hMesh, int grpidx, GROUPEDITSPEC *ges)
{
	return ((OGLMesh*)hMesh)->EditGroup (grpidx, ges);
}

/**
 * \brief Increment the reference counter of a surface.
 * \param surf surface handle
 * \default None.
 * \note Derived classes should keep track on surface references, and
 *   overload this function to increment the reference counter.
 */
void OGLClient::clbkIncrSurfaceRef (SURFHANDLE surf)
{
	((OGLTexture *)surf)->m_RefCnt++;
}

/**
 * \brief Create a pen resource for 2-D drawing.
 * \param style line style (0=invisible, 1=solid, 2=dashed)
 * \param width line width [pixel]
 * \param col line colour (format: 0xBBGGRR)
 * \return Pointer to pen resource
 * \default None, returns NULL.
 * \sa clbkReleasePen, oapi::Pen
 */
Pen *OGLClient::clbkCreatePen (int style, int width, uint32_t col) const
{
	return new OGLPen(style, width, col);
}

void OGLClient::clbkReleasePen (Pen *pen) const
{
	delete (OGLPen *)pen;
}

/**
 * \brief Create a font resource for 2-D drawing.
 * \param height cell or character height [pixel]
 * \param prop proportional/fixed width flag
 * \param face font face name
 * \param style font decoration style
 * \param orientation text orientation [1/10 deg]
 * \return Pointer to font resource
 * \default None, returns NULL.
 * \note For a description of the parameters, see Font constructor
 *   \ref oapi::Font::Font
 * \sa clbkReleaseFont, oapi::Font
 */
Font *OGLClient::clbkCreateFont (int height, bool prop, const char *face, Font::Style style, int orientation) const
{
	OGLFont *font = new OGLFont (height, prop, face, style, orientation);
	return font;
}

/**
 * \brief Create a surface for texturing, as a blitting source, etc.
 * 
 * Surfaces are used for offscreen bitmap and texture manipulation,
 * blitting and rendering.
 * Derived classes should create a device-specific surface, and
 * return a cast to a generic Orbiter SURFHANDLE.
 * \param w surface width [pixels]
 * \param h surface height [pixels]
 * \param attrib \ref surfacecaps (bitflags). See notes.
 * \return Surface handle (in the simplest case, just a pointer to the
 *   surface, cast to a SURFHANDLE). On failure, this method should
 *   return NULL.
 * \default None, returns NULL.
 * \note The attribute flag can contain one of the following main attributes:
 *  - OAPISURFACE_RO: create a surface that can be read by the GPU pipeline, and that can be updated from system memory.
 *  - OAPISURFACE_RW: create a surface that can be read and written by the GPU pipeline, and that can be updated from system memory.
 *  - OAPISURFACE_GDI: create a surface that can be read and written from the CPU, and can be blitted into an uncompressed RO or RW surface without an alpha channel
 *  In addition, the flag can contain any combination of the following auxiliary attributes:
 *  - OAPISURFACE_MIPMAPS: create a full chain of mipmaps for the surface if possible
 *  - OAPISURFACE_NOALPHA: create a surface without an alpha channel
 */
SURFHANDLE OGLClient::clbkCreateSurfaceEx (int w, int h, int attrib)
{
	return mTextureManager->GetTextureForRendering(w, h, attrib);
    /*
	HRESULT hr;
	LPDIRECTDRAWSURFACE7 surf;
	DDSURFACEDESC2 ddsd;
    ZeroMemory (&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.ddpfPixelFormat.dwSize = sizeof (DDPIXELFORMAT);
    ddsd.dwWidth  = w;
	ddsd.dwHeight = h;
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH; 
	if (attrib & OAPISURFACE_TEXTURE)
		ddsd.ddsCaps.dwCaps |= DDSCAPS_TEXTURE;
	else
		ddsd.ddsCaps.dwCaps |= DDSCAPS_OFFSCREENPLAIN;
	if (attrib & OAPISURFACE_SYSMEM || (attrib & OAPISURFACE_GDI || attrib & OAPISURFACE_SKETCHPAD) && !(attrib & OAPISURFACE_TEXTURE)) 
		ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
	else
		ddsd.ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;
	if ((attrib & OAPISURFACE_ALPHA) && !(attrib & (OAPISURFACE_GDI | OAPISURFACE_SKETCHPAD)))
		ddsd.ddpfPixelFormat.dwFlags |=  DDPF_ALPHAPIXELS; // enable alpha channel
	if ((hr = pDD->CreateSurface (&ddsd, &surf, NULL)) != DD_OK) {
		LOGOUT_DDERR (hr);
		return NULL;
	}
	return (SURFHANDLE)surf;
    */
}

/**
 * \brief Create an offscreen surface
 *
 * Surfaces are used for offscreen bitmap and texture manipulation,
 * blitting and rendering.
 * Derived classes should create a device-specific surface, and
 * return a cast to a generic Orbiter SURFHANDLE.
 * \param w surface width [pixels]
 * \param h surface height [pixels]
 * \param hTemplate surface format template
 * \return pointer to surface, cast into a SURFHANDLE, or NULL to
 *   indicate failure.
 * \default None, returns NULL.
 * \note If \e hTemplate is provided, this method should create the new
 *   surface with the same pixel format.
 * \sa clbkCreateTexture, clbkReleaseSurface
 */
SURFHANDLE OGLClient::clbkCreateSurface (int w, int h, SURFHANDLE hTemplate)
{
	return mTextureManager->GetTextureForRendering(w, h, 0); //FIXME: get NOALPHA flag from somewhereS

/*
	HRESULT hr;
	LPDIRECTDRAWSURFACE7 surf;
	DDSURFACEDESC2 ddsd;
    ZeroMemory (&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH; 
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
    ddsd.dwWidth  = w;
	ddsd.dwHeight = h;
	if (hTemplate) { // use pixel format information from template surface
		ddsd.ddpfPixelFormat.dwSize = sizeof (DDPIXELFORMAT);
		ddsd.ddpfPixelFormat.dwFlags = 0;
		((LPDIRECTDRAWSURFACE7)hTemplate)->GetPixelFormat (&ddsd.ddpfPixelFormat);
		ddsd.dwFlags |= DDSD_PIXELFORMAT;
	}
	if ((hr = pDD->CreateSurface (&ddsd, &surf, NULL)) != DD_OK) {
		LOGOUT_DDERR (hr);
		return NULL;
	}
    
	return (SURFHANDLE)surf;
    */
}

/**
 * \brief Create a texture for rendering
 * \param w texture width
 * \param h texture height
 * \return pointer to texture, returned as generic SURFHANDLE. NULL
 *   indicates failure.
 * \note This method is similar to \ref clbkCreateSurface, but the
 *   returned surface handle must be usable as a texture when rendering
 *   the scene. Clients which don't differentiate between offscreen
 *   surfaces and textures may use identical code for both functions.
 * \note Some clients may put restrictions on the texture format (e.g.
 *   require square size (w=h), and/or powers of two (w=2^n). If the
 *   texture cannot be created with the requested size, this method
 *   should return NULL.
 * \sa clbkCreateSurface, clbkReleaseSurface
 */
SURFHANDLE OGLClient::clbkCreateTexture (int w, int h)
{
	return mTextureManager->GetTextureForRendering(w, h, 0);
/*
	LPDIRECTDRAWSURFACE7 surf;
	DDSURFACEDESC2 ddsd;
    ZeroMemory (&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_CAPS ;
	ddsd.dwWidth = w;
	ddsd.dwHeight = h;
	ddsd.ddsCaps.dwCaps = DDSCAPS_TEXTURE;
	ddsd.ddpfPixelFormat.dwSize = sizeof (DDPIXELFORMAT);
	ddsd.ddpfPixelFormat.dwFlags = 0;
	//ddsd.ddckCKSrcBlt.dwColorSpaceLowValue = ddsd.ddckCKSrcBlt.dwColorSpaceLowValue = 0;
	GetFramework()->GetBackBuffer()->GetPixelFormat (&ddsd.ddpfPixelFormat);
	// take pixel format from render surface (should make it compatible)
	ddsd.ddpfPixelFormat.dwFlags &= ~DDPF_ALPHAPIXELS; // turn off alpha data
	pDD->CreateSurface (&ddsd, &surf, NULL);
	return surf;
    */
}

/**
 * \brief Decrement surface reference counter, release surface if counter
 *   reaches 0.
 * \param surf surface handle
 * \return true on success
 * \default None, returns false.
 * \note Derived classes should overload this function to decrement a
 *   surface reference counter and release the surface if required.
 * \sa clbkCreateSurface, clbkIncrSurfaceRef
 */
bool OGLClient::clbkReleaseSurface (SURFHANDLE surf)
{
	((OGLTexture *)surf)->Release();
    return true;
}

/**
 * \brief Release a drawing object.
 * \param sp pointer to drawing object
 * \default None.
 * \sa Sketchpad, clbkGetSketchpad
 */
void OGLClient::clbkReleaseSketchpad (oapi::Sketchpad *sp)
{
	delete (OGLPad *)sp;
}

/**
 * \brief De-allocate a font resource.
 * \param font pointer to font resource
 * \default None.
 * \sa clbkCreateFont, oapi::Font
 */
void OGLClient::clbkReleaseFont (Font *font) const
{
	delete font;
}

/**
 * \brief Simulation startup finalisation
 *
 * Called at the beginning of a simulation session after the scenarion has
 * been parsed and the logical object have been created.
 * \default None
 */
void OGLClient::clbkPostCreation ()
{
//	ExitOutputLoadStatus ();
	if (mScene) mScene->Initialise();

}

/**
 * \brief Set transparency colour key for a surface.
 * \param surf surface handle
 * \param ckey transparency colour key value
 * \default None, returns false.
 * \note Derived classes should overload this method if the renderer
 *   supports colour key transparency for surfaces.
 */
bool OGLClient::clbkSetSurfaceColourKey (SURFHANDLE surf, uint32_t ckey)
{
//	printf("clbkSetSurfaceColourKey %p %u\n", surf, ckey);
	((OGLTexture *)surf)->m_colorkey = ckey;
	return true;
}

/**
 * \brief Create a 2-D drawing object ("sketchpad") associated with a surface.
 * \param surf surface handle
 * \return Pointer to drawing object.
 * \default None, returns NULL.
 * \note Clients should overload this function to provide 2-D drawing
 *   support. This requires an implementation of a class derived from
 *   \ref Sketchpad which provides the drawing context and drawing
 *   primitives.
 * \sa Sketchpad, clbkReleaseSketchpad
 */
oapi::Sketchpad *OGLClient::clbkGetSketchpad (SURFHANDLE surf)
{
	return new OGLPad(surf);
}

/**
 * \brief End of simulation session notification
 *
 * Called before the end of a simulation session. At the point of call,
 * logical objects still exist (OBJHANDLEs valid), and external modules
 * are still loaded.
 * \param fastclose Indicates a "fast shutdown" request (see notes)
 * \default None.
 * \note Derived clients can use this function to perform cleanup operations
 *   for which the simulation objects are still required.
 * \note If fastclose == true, the user has selected one of the fast
 *   shutdown options (terminate Orbiter, or respawn Orbiter process). In
 *   this case, the current process will terminate, and the graphics client
 *   can skip object cleanup and deallocation in order to speed up the
 *   closedown process.
 * \sa clbkDestroyRenderWindow
 */
void OGLClient::clbkCloseSession (bool fastclose)
{
	GraphicsClient::clbkCloseSession (fastclose);
/*
	if (scene) {
		delete scene;
		scene = NULL;
	}
	GlobalExit();
    */
}

/**
 * \brief Display a scene on screen after rendering it.
 *
 * Called after clbkRenderScene to allow the client to display the rendered
 * scene (e.g. by page-flipping, or blitting from background to primary
 * frame buffer. This method can also be used by the client to display any
 * top-level 2-D overlays (e.g. dialogs) on the primary frame buffer.
 * \return Should return true on successful operation, false on failure or
 *   if no operation was performed.
 * \default None, returns false.
 */
bool OGLClient::clbkDisplayFrame ()
{
	CheckError("before clbkDisplayFrame");
	glfwSwapBuffers(hRenderWnd);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	CheckError("glClearColor");
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	CheckError("glClear");

	return true;
}
void OGLClient::clbkRenderGUI ()
{
	CheckError("clbkRenderGUI");
	ImGui_ImplOpenGL3_NewFrame();
	CheckError("ImGui_ImplOpenGL3_NewFrame");
	ImGui_ImplGlfw_NewFrame();
	CheckError("ImGui_ImplGlfw_NewFrame");
	ImGui::NewFrame();
	CheckError("ImGui::NewFrame");

	oapiDrawDialogs();
	CheckError("after oapiDrawDialogs");

	ImGui::Render();
	CheckError("ImGui::Render");
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	CheckError("ImGui_ImplOpenGL3_RenderDrawData");

	ImGuiIO &io = ImGui::GetIO();
	if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		GLFWwindow* backup_current_context = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		CheckError("ImGui::UpdatePlatformWindows");
		ImGui::RenderPlatformWindowsDefault();
		CheckError("ImGui::RenderPlatformWindowsDefault");
		glfwMakeContextCurrent(backup_current_context);
	}
}

/**
 * \brief Display a load status message on the splash screen
 *
 * Called repeatedly while a simulation session is loading, to allow the
 * client to echo load status messages on its splash screen if desired.
 * \param msg Pointer to load status message string
 * \param line message line to be displayed (0 or 1), where 0 indicates
 *   a group or category heading, and 1 indicates an individual action
 *   relating to the most recent group.
 * \return Should return true if it displays the message, false if not.
 * \default None, returns false.
 */
bool OGLClient::clbkSplashLoadMsg (const char *msg, int line)
{
    return false;
}

/**
 * \brief Store a persistent mesh template
 *
 * Called when a plugin loads a mesh with oapiLoadMeshGlobal, to allow the
 * client to store a copy of the mesh in client-specific format. Whenever
 * the mesh is required later, the client can create an instance as a copy
 * of the template, rather than creating it by converting from Orbiter's
 * mesh format.
 * \param hMesh mesh handle
 * \param fname mesh file name
 * \default None.
 * \note Use \ref oapiMeshGroup to to obtain mesh data and convert them to
 *   a suitable format.
 * \note the mesh templates loaded with \ref oapiLoadMeshGlobal are shared between
 *   all vessel instances and should never be edited. Vessels should make
 *   individual copies of the mesh before modifying them (e.g. for animations)
 * \note The file name is provide to allow the client to parse the mesh directly
 *   from file, rather than copying it from the hMesh object, or to use an
 *   alternative mesh file.
 * \note The file name contains a path relative to Orbiter's main mesh
 *   directory.
 */
void OGLClient::clbkStoreMeshPersistent (MESHHANDLE hMesh, const char *fname)
{
	mMeshManager->StoreMesh (hMesh);
}

/**
 * \brief Create a brush resource for 2-D drawing.
 * \param col line colour (format: 0xBBGGRR)
 * \return Pointer to brush resource
 * \default None, returns NULL.
 * \sa clbkReleaseBrush, oapi::Brush
 */
Brush *OGLClient::clbkCreateBrush (uint32_t col) const
{
	return new OGLBrush (col);
}

/**
 * \brief De-allocate a brush resource.
 * \param brush pointer to brush resource
 * \default None.
 * \sa clbkCreateBrush, oapi::Brush
 */
void OGLClient::clbkReleaseBrush (Brush *brush) const
{
	delete (OGLBrush *)brush;
}

/** \brief Launchpad video tab indicator
 *
 * Indicate if the the default video tab in the Orbiter launchpad dialog
 * is to be used for obtaining user video preferences. If a derived
 * class returns false here, the video tab is not shown.
 * \return true if the module wants to use the video tab in the launchpad
 *   dialog, false otherwise.
 * \default Return true.
 */
bool OGLClient::clbkUseLaunchpadVideoTab () const
{
    return false;
}

/**
 * \brief Fill a surface with a uniform colour
 * \param surf surface handle
 * \param col colour value
 * \return true on success, false if the fill operation cannot be performed.
 * \default None, returns false.
 * \note Parameter col is a device-dependent colour value
 *   (see \ref clbkGetDeviceColour).
 * \sa clbkFillSurface(SURFHANDLE,int,int,int,int,uint32_t)
 */
bool OGLClient::clbkFillSurface (SURFHANDLE surf, uint32_t col) const
{
	GLint result;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &result);
	assert(result == 0);
	glBindFramebuffer(GL_FRAMEBUFFER, ((OGLTexture *)surf)->m_FBO);
	float r = (col&0xff)/255.0;
	float g = ((col>>8)&0xff)/255.0;
	float b = ((col>>16)&0xff)/255.0;
	glClearColor(r, g, b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

/**
 * \brief Fill an area in a surface with a uniform colour
 * \param surf surface handle
 * \param tgtx left edge of target rectangle
 * \param tgty top edge of target rectangle
 * \param w width of rectangle
 * \param h height of rectangle
 * \param col colour value
 * \return true on success, false if the fill operation cannot be performed.
 * \default None, returns false.
 * \note Parameter col is a device-dependent colour value
 *   (see \ref clbkGetDeviceColour).
 * \sa clbkFillSurface(SURFHANDLE,uint32_t)
 */
bool OGLClient::clbkFillSurface (SURFHANDLE surf, int tgtx, int tgty, int w, int h, uint32_t col) const
{
	glm::mat4 ortho_proj = glm::ortho(0.0f, (float)g_client->GetScene()->GetCamera()->GetWidth(), (float)g_client->GetScene()->GetCamera()->GetHeight(), 0.0f);
	static Shader s("Untextured.vs","Untextured.fs");

	static GLuint m_Buffer, m_VAO;
	static bool init = false;
	if(!init) {
		init=true;
		glGenVertexArrays(1, &m_VAO);
		CheckError("glGenVertexArrays");
		glBindVertexArray(m_VAO);
		CheckError("glBindVertexArray");

		glGenBuffers(1, &m_Buffer);
		CheckError("glGenBuffers");
		glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
		CheckError("glBindBuffer");
		glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
		CheckError("glBufferSubData");
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
		CheckError("glVertexAttribPointer");
		glEnableVertexAttribArray(0);
		CheckError("glEnableVertexAttribArray");

		glBindVertexArray(0);
	}

	const GLfloat vertex[] = {
		0, 0, (float)tgtx+w, (float)tgty+h,
		0, 0, (float)tgtx, (float)tgty,
		0, 0, (float)tgtx+w, (float)tgty,

		0, 0, (float)tgtx, (float)tgty+h,
		0, 0, (float)tgtx, (float)tgty,
		0, 0, (float)tgtx+w, (float)tgty+h,
	};

	GLint oldFB = 0;
	GLint oldViewport[4];
	OGLTexture *m_tex = (OGLTexture *)surf;
	if(m_tex) {
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFB);
		glGetIntegerv(GL_VIEWPORT, oldViewport);
		ortho_proj = glm::ortho(0.0f, (float)m_tex->m_Width, 0.0f, (float)m_tex->m_Height);
		m_tex->SetAsTarget(false);
	}

	glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
	glBindVertexArray(m_VAO);
	CheckError("glBindBuffer");
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertex), vertex);
	CheckError("glBufferSubData");
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
	CheckError("glVertexAttribPointer");
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

	s.Bind();
	s.SetMat4("projection", ortho_proj);
	glm::vec3 color;
	color.r = (float)((col>>16)&0xff)/255.0;
	color.g = (float)((col>>8)&0xff)/255.0;
	color.b = (float)((col>>0)&0xff)/255.0;
	s.SetVec3("quad_color", color);

	glBindVertexArray(m_VAO);
	CheckError("glBindVertexArray");

	glDrawArrays(GL_TRIANGLES, 0, 6);
	CheckError("glDrawArrays");

	glBindVertexArray(0);

	s.UnBind();

	if(surf) {
		glBindFramebuffer(GL_FRAMEBUFFER, oldFB);
		glViewport(oldViewport[0],oldViewport[1],oldViewport[2],oldViewport[3]);
	}

    return true;
}

/**
 * \brief Copy a rectangle from one surface to another, stretching or shrinking as required.
 * \param tgt target surface handle
 * \param tgtx left edge of target rectangle
 * \param tgty top edge of target rectangle
 * \param tgtw width of target rectangle
 * \param tgth height of target rectangle
 * \param src source surface handle
 * \param srcx left edge of source rectangle
 * \param srcy top edge of source rectangle
 * \param srcw width of source rectangle
 * \param srch height of source rectangle
 * \param flag blitting parameters
 * \return true on success, fals if the blit cannot be performed.
 * \default None, returns false.
 * \note By convention, tgt==NULL is valid and refers to the primary render
 *   surface (e.g. for copying 2-D overlay surfaces).
 * \sa clbkBlt(SURFHANDLE,int,int,SURFHANDLE,int),
 *   clbkBlt(SURFHANDLE,int,int,SURFHANDLE,int,int,int,int,int)
 */
bool OGLClient::clbkScaleBlt (SURFHANDLE tgt, int tgtx, int tgty, int tgtw, int tgth,
		                       SURFHANDLE src, int srcx, int srcy, int srcw, int srch, int flag) const
{
    //printf("OGLClient::clbkScaleBlt tgtx=%d tgty=%d tgtw=%d tgth=%d\n", tgtx,tgty,tgtw,tgth);
    //printf("OGLClient::clbkScaleBlt srcx=%d srcy=%d srcw=%d srch=%d\n", srcx,srcy,srcw,srch);
	GLboolean oldBlend = glDisableEx(GL_BLEND);
	GLboolean oldCull = glDisableEx(GL_CULL_FACE);
	glm::mat4 ortho_proj;
	GLint oldFB;
	GLint oldViewport[4];

	if(tgt) {
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFB);
		glGetIntegerv(GL_VIEWPORT, oldViewport);

		OGLTexture *m_tex = (OGLTexture *)tgt;
		//ortho_proj = glm::ortho(0.0f, (float)m_tex->m_Width, (float)m_tex->m_Height, 0.0f); //y-flipped
		ortho_proj = glm::ortho(0.0f, (float)m_tex->m_Width, 0.0f, (float)m_tex->m_Height);
		m_tex->SetAsTarget(false);
	} else {
		ortho_proj = glm::ortho(0.0f, (float)g_client->GetScene()->GetCamera()->GetWidth(), (float)g_client->GetScene()->GetCamera()->GetHeight(), 0.0f);
	}

	static Shader s("Overlay.vs","Overlay.fs");

	static GLuint m_Buffer, m_VAO;
	static bool init = false;
	if(!init) {
		init=true;
		glGenVertexArrays(1, &m_VAO);
		CheckError("glGenVertexArrays");
		glBindVertexArray(m_VAO);
		CheckError("glBindVertexArray");

		glGenBuffers(1, &m_Buffer);
		CheckError("glGenBuffers");
		glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
		CheckError("glBindBuffer");
		glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
		CheckError("glBufferSubData");
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
		CheckError("glVertexAttribPointer");
		glEnableVertexAttribArray(0);
		CheckError("glEnableVertexAttribArray");

		glBindVertexArray(0);
	}

	float tgt_w = tgtw;
	float tgt_h = tgth;

	float src_w = srcw;
	float src_h = srch;

	float s0 = 0;
	float s1 = 1;
	float t0 = 0;
	float t1 = 1;

	const GLfloat vertex[] = {
		s1, t1, (float)tgtx+tgt_w, (float)tgty+tgt_h,
		s0, t0, (float)tgtx, (float)tgty,
		s1, t0, (float)tgtx+tgt_w, (float)tgty,

		s0, t1, (float)tgtx, (float)tgty+tgt_h,
		s0, t0, (float)tgtx, (float)tgty,
		s1, t1, (float)tgtx+tgt_w, (float)tgty+tgt_h,
	};
	glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
	glBindVertexArray(m_VAO);
	CheckError("OGLPad::Text glBindBuffer");
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertex), vertex);
	CheckError("OGLPad::Text glBufferSubData");
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
	CheckError("OGLPad::Text glVertexAttribPointer");
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//	CheckError("OGLPad::Text glBindBuffer0");
	glEnableVertexAttribArray(0);
	//CheckError("OGLPad::Text glEnableVertexAttribArray0");
	glBindVertexArray(0);

	s.Bind();
	s.SetMat4("projection", ortho_proj);

	OGLTexture *key_tex = (OGLTexture *)src;
	bool hasck = key_tex->m_colorkey != SURF_NO_CK && key_tex->m_colorkey != 0;
	if(hasck) {
		uint32_t ck = key_tex->m_colorkey;
		glm::vec4 ckv;
		ckv.r = ((ck>>16)&0xff)/255.0;
		ckv.g = ((ck>>8)&0xff)/255.0;
		ckv.b = ((ck>>0)&0xff)/255.0;
		ckv.a = ((ck>>24)&0xff)/255.0;
		s.SetVec4("color_key", ckv);
		s.SetFloat("color_keyed", 1.0);
	} else {
		s.SetFloat("color_keyed", 0.0);
	}

	/*
	GLint whichID;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &whichID);
	assert(whichID==0);
	*/
	glBindTexture(GL_TEXTURE_2D, ((OGLTexture *)src)->m_TexId);
	CheckError("glBindTexture");
	if(hasck) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	glBindVertexArray(m_VAO);
	CheckError("glBindVertexArray");

	glDrawArrays(GL_TRIANGLES, 0, 6);
	CheckError("glDrawArrays");

	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	s.UnBind();

	if(tgt)
    {
		glBindFramebuffer(GL_FRAMEBUFFER, oldFB);
		glViewport(oldViewport[0],oldViewport[1],oldViewport[2],oldViewport[3]);
	}

	glRestoreEx(GL_BLEND, oldBlend);
	glRestoreEx(GL_CULL_FACE, oldCull);

	return true;
}

/**
 * \brief Copy one surface into an area of another one.
 * \param tgt target surface handle
 * \param tgtx left edge of target rectangle
 * \param tgty top edge of target rectangle
 * \param src source surface handle
 * \param flag blitting parameters (see notes)
 * \return true on success, false if the blit cannot be performed.
 * \default None, returns false.
 * \note By convention, tgt==NULL is valid and refers to the primary render
 *   surface (e.g. for copying 2-D overlay surfaces).
 * \note The following bit-flags are defined:
 *   <table col=2>
 *   <tr><td>BLT_SRCCOLORKEY</td><td>Use the colour key defined by the source surface for transparency</td></tr>
 *   <tr><td>BLT_TGTCOLORKEY</td><td>Use the colour key defined by the target surface for transparency</td></tr>
 *   </table>
 *   If a client doesn't support some of the flags, it should quietly ignore it.
 * \sa clbkBlt(SURFHANDLE,int,int,SURFHANDLE,int,int,int,int,int)
 */
bool OGLClient::clbkBlt (SURFHANDLE tgt, int tgtx, int tgty, SURFHANDLE src, int flag) const
{
	GLboolean oldBlend = glDisableEx(GL_BLEND);
	GLboolean oldCull = glDisableEx(GL_CULL_FACE);
	GLint oldFB;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFB);
	GLint oldViewport[4];
	glGetIntegerv(GL_VIEWPORT, oldViewport);

	OGLTexture *m_tex = (OGLTexture *)tgt;
	//ortho_proj = glm::ortho(0.0f, (float)m_tex->m_Width, (float)m_tex->m_Height, 0.0f); //y-flipped
	glm::mat4 ortho_proj = glm::ortho(0.0f, (float)m_tex->m_Width, 0.0f, (float)m_tex->m_Height);
	m_tex->SetAsTarget();
	static Shader s("Overlay.vs","Overlay.fs");

	static GLuint m_Buffer, m_VAO;
	static bool init = false;
	if(!init) {
		init=true;
		glGenVertexArrays(1, &m_VAO);
		CheckError("glGenVertexArrays");
		glBindVertexArray(m_VAO);
		CheckError("glBindVertexArray");

		glGenBuffers(1, &m_Buffer);
		CheckError("glGenBuffers");
		glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
		CheckError("glBindBuffer");
		glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
		CheckError("glBufferSubData");
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
		CheckError("glVertexAttribPointer");
		glEnableVertexAttribArray(0);
		CheckError("glEnableVertexAttribArray");

		glBindVertexArray(0);
	}

	float tgt_w = m_tex->m_Width;
	float tgt_h = m_tex->m_Height;

	float src_w = ((OGLTexture *)src)->m_Width;
	float src_h = ((OGLTexture *)src)->m_Height;

	float s0 = 0;
	float s1 = 1;
	float t0 = 0;
	float t1 = 1;

	const GLfloat vertex[] = {
		s1, t1, (float)tgtx+src_w, (float)tgty+src_h,
		s0, t0, (float)tgtx, (float)tgty,
		s1, t0, (float)tgtx+src_w, (float)tgty,

		s0, t1, (float)tgtx, (float)tgty+src_h,
		s0, t0, (float)tgtx, (float)tgty,
		s1, t1, (float)tgtx+src_w, (float)tgty+src_h,
	};
	glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
	glBindVertexArray(m_VAO);
	CheckError("OGLPad::Text glBindBuffer");
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertex), vertex);
	CheckError("OGLPad::Text glBufferSubData");
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
	CheckError("OGLPad::Text glVertexAttribPointer");
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//	CheckError("OGLPad::Text glBindBuffer0");
	glEnableVertexAttribArray(0);
	//CheckError("OGLPad::Text glEnableVertexAttribArray0");
	glBindVertexArray(0);

	s.Bind();
	s.SetMat4("projection", ortho_proj);

	OGLTexture *key_tex = (OGLTexture *)src;
	bool hasck = key_tex->m_colorkey != SURF_NO_CK && key_tex->m_colorkey != 0;
	if(hasck) {
		uint32_t ck = key_tex->m_colorkey;
		glm::vec4 ckv;
		ckv.r = ((ck>>16)&0xff)/255.0;
		ckv.g = ((ck>>8)&0xff)/255.0;
		ckv.b = ((ck>>0)&0xff)/255.0;
		ckv.a = ((ck>>24)&0xff)/255.0;
		s.SetVec4("color_key", ckv);
		s.SetFloat("color_keyed", 1.0);
	} else {
		s.SetFloat("color_keyed", 0.0);
	}

	/*
	GLint whichID;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &whichID);
	assert(whichID==0);
	*/
	glBindTexture(GL_TEXTURE_2D, ((OGLTexture *)src)->m_TexId);
	CheckError("glBindTexture");
	if(hasck) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	glBindVertexArray(m_VAO);
	CheckError("glBindVertexArray");

	glDrawArrays(GL_TRIANGLES, 0, 6);
	CheckError("glDrawArrays");

	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	s.UnBind();

	glBindFramebuffer(GL_FRAMEBUFFER, oldFB);
	glViewport(oldViewport[0],oldViewport[1],oldViewport[2],oldViewport[3]);

	glRestoreEx(GL_BLEND, oldBlend);
	glRestoreEx(GL_CULL_FACE, oldCull);

	return true;
}

/**
 * \brief Copy a rectangle from one surface to another.
 * \param tgt target surfac handle
 * \param tgtx left edge of target rectangle
 * \param tgty top edge of target rectangle
 * \param src source surface handle
 * \param srcx left edge of source rectangle
 * \param srcy top edge of source rectangle
 * \param w width of rectangle
 * \param h height of rectangle
 * \param flag blitting parameters (see notes)
 * \return true on success, false if the blit cannot be performed.
 * \default None, returns false.
 * \note By convention, tgt==NULL is valid and refers to the primary render
 *   surface (e.g. for copying 2-D overlay surfaces).
 * \note The following bit-flags are defined:
 *   <table col=2>
 *   <tr><td>BLT_SRCCOLORKEY</td><td>Use the colour key defined by the source surface for transparency</td></tr>
 *   <tr><td>BLT_TGTCOLORKEY</td><td>Use the colour key defined by the target surface for transparency</td></tr>
 *   </table>
 *   If a client doesn't support some of the flags, it should quietly ignore it.
 * \sa clbkBlt(SURFHANDLE,int,int,SURFHANDLE,int)
 */
bool OGLClient::clbkBlt (SURFHANDLE tgt, int tgtx, int tgty, SURFHANDLE src, int srcx, int srcy, int w, int h, int flag) const
{
	GLboolean oldBlend = glDisableEx(GL_BLEND);
	GLboolean oldCull = glDisableEx(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	static int done = 0;
	//static unsigned int VAO;
	static GLuint fbo;
	if(!done) {
		done = 1;
		glGenFramebuffers(1, &fbo);
	}

	if(tgt) {
		GLint oldFB;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFB);
		GLint oldViewport[4];
		glGetIntegerv(GL_VIEWPORT, oldViewport);
		OGLTexture *m_tex = (OGLTexture *)tgt;
		//ortho_proj = glm::ortho(0.0f, (float)m_tex->m_Width, (float)m_tex->m_Height, 0.0f); //y-flipped
		glm::mat4 ortho_proj = glm::ortho(0.0f, (float)m_tex->m_Width, 0.0f, (float)m_tex->m_Height);
		m_tex->SetAsTarget();
/*
		glBindTexture(GL_TEXTURE_2D, m_tex->m_TexId);
		CheckError("OGLPad glBindTexture");
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		CheckError("OGLPad glTexParameteri");
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		CheckError("OGLPad glTexParameteri2");
		glGenerateMipmap(GL_TEXTURE_2D);
		CheckError("OGLPad glGenerateMipmap");*/
		static Shader s("Overlay.vs","Overlay.fs");

		static GLuint m_Buffer, m_VAO;
		static bool init = false;
		if(!init) {
			init=true;
			CheckError("pre glGenVertexArrays");
			glGenVertexArrays(1, &m_VAO);
			CheckError("glGenVertexArrays");
			glBindVertexArray(m_VAO);
			CheckError("glBindVertexArray");

			glGenBuffers(1, &m_Buffer);
			CheckError("glGenBuffers");
			glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
			CheckError("glBindBuffer");
			glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
			CheckError("glBufferSubData");
			glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
			CheckError("glVertexAttribPointer");
			glEnableVertexAttribArray(0);
			CheckError("glEnableVertexAttribArray");

			glBindVertexArray(0);
		}

		float tgt_w = m_tex->m_Width;
		float tgt_h = m_tex->m_Height;

		float src_w = ((OGLTexture *)src)->m_Width;
		float src_h = ((OGLTexture *)src)->m_Height;

 		float s0 = (float)srcx / src_w;
 		float s1 = (float)(srcx+(float)w) / src_w;
 		float t0 = (float)srcy / src_h;
 		float t1 = (float)(srcy+(float)h) / src_h;

		const GLfloat vertex[] = {
			s1, t1, (float)tgtx+w, (float)tgty+h,
			s0, t0, (float)tgtx, (float)tgty,
			s1, t0, (float)tgtx+w, (float)tgty,

			s0, t1, (float)tgtx, (float)tgty+h,
			s0, t0, (float)tgtx, (float)tgty,
			s1, t1, (float)tgtx+w, (float)tgty+h,
		};

		glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
		glBindVertexArray(m_VAO);
		CheckError("OGLPad::Text glBindBuffer");
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertex), vertex);
		CheckError("OGLPad::Text glBufferSubData");
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
		CheckError("OGLPad::Text glVertexAttribPointer");
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		
//	CheckError("OGLPad::Text glBindBuffer0");
		glEnableVertexAttribArray(0);
	//CheckError("OGLPad::Text glEnableVertexAttribArray0");
		glBindVertexArray(0);

		s.Bind();
		s.SetMat4("projection", ortho_proj);

		OGLTexture *key_tex = (OGLTexture *)src;
		bool hasck = key_tex->m_colorkey != SURF_NO_CK && key_tex->m_colorkey != 0;
		if(hasck) {
			uint32_t ck = key_tex->m_colorkey;
			glm::vec4 ckv;
			ckv.r = ((ck>>16)&0xff)/255.0;
			ckv.g = ((ck>>8)&0xff)/255.0;
			ckv.b = ((ck>>0)&0xff)/255.0;
			ckv.a = ((ck>>24)&0xff)/255.0;
			s.SetVec4("color_key", ckv);
			s.SetFloat("color_keyed", 1.0);
		} else {
			s.SetFloat("color_keyed", 0.0);
		}

/*
		GLint whichID;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &whichID);
		assert(whichID==0);
*/
		glBindTexture(GL_TEXTURE_2D, ((OGLTexture *)src)->m_TexId);
		CheckError("glBindTexture");
		if(hasck) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		} else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		glBindVertexArray(m_VAO);
		CheckError("glBindVertexArray");

		glDrawArrays(GL_TRIANGLES, 0, 6);
		CheckError("glDrawArrays");

		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		s.UnBind();

		glBindFramebuffer(GL_FRAMEBUFFER, oldFB);
		glViewport(oldViewport[0],oldViewport[1],oldViewport[2],oldViewport[3]);
	} else {
		glm::mat4 ortho_proj = glm::ortho(0.0f, (float)g_client->GetScene()->GetCamera()->GetWidth(), (float)g_client->GetScene()->GetCamera()->GetHeight(), 0.0f);
		static Shader s("Overlay.vs","Overlay.fs");

		static GLuint m_Buffer, m_VAO;
		static bool init = false;
		if(!init) {
			init=true;
			glGenVertexArrays(1, &m_VAO);
			CheckError("glGenVertexArrays");
			glBindVertexArray(m_VAO);
			CheckError("glBindVertexArray");

			glGenBuffers(1, &m_Buffer);
			CheckError("glGenBuffers");
			glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
			CheckError("glBindBuffer");
			glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
			CheckError("glBufferSubData");
			glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
			CheckError("glVertexAttribPointer");
			glEnableVertexAttribArray(0);
			CheckError("glEnableVertexAttribArray");

			glBindVertexArray(0);
		}

		float tgt_w = (float)g_client->GetScene()->GetCamera()->GetWidth();
		float tgt_h = (float)g_client->GetScene()->GetCamera()->GetHeight();

		float src_w = ((OGLTexture *)src)->m_Width;
		float src_h = ((OGLTexture *)src)->m_Height;

 		float s0 = (float)srcx / src_w;
 		float s1 = (float)(srcx+(float)w) / src_w;
 		float t0 = (float)srcy / src_h;
 		float t1 = (float)(srcy+(float)h) / src_h;

		const GLfloat vertex[] = {
			s1, t1, (float)tgtx+w, (float)tgty+h,
			s0, t0, (float)tgtx, (float)tgty,
			s1, t0, (float)tgtx+w, (float)tgty,

			s0, t1, (float)tgtx, (float)tgty+h,
			s0, t0, (float)tgtx, (float)tgty,
			s1, t1, (float)tgtx+w, (float)tgty+h,
		};
		glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
		glBindVertexArray(m_VAO);
		CheckError("OGLPad::Text glBindBuffer");
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertex), vertex);
		CheckError("OGLPad::Text glBufferSubData");
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
		CheckError("OGLPad::Text glVertexAttribPointer");
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		
//	CheckError("OGLPad::Text glBindBuffer0");
		glEnableVertexAttribArray(0);
	//CheckError("OGLPad::Text glEnableVertexAttribArray0");
		glBindVertexArray(0);

		s.Bind();
		s.SetMat4("projection", ortho_proj);

		OGLTexture *key_tex = (OGLTexture *)src;
		bool hasck = key_tex->m_colorkey != SURF_NO_CK && key_tex->m_colorkey != 0;
		if(hasck) {
			uint32_t ck = key_tex->m_colorkey;
			glm::vec4 ckv;
			ckv.r = ((ck>>16)&0xff)/255.0;
			ckv.g = ((ck>>8)&0xff)/255.0;
			ckv.b = ((ck>>0)&0xff)/255.0;
			ckv.a = ((ck>>24)&0xff)/255.0;
			s.SetVec4("color_key", ckv);
			s.SetFloat("color_keyed", 1.0);
		} else {
			s.SetFloat("color_keyed", 0.0);
		}

		GLint whichID;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &whichID);
		assert(whichID==0);

		glBindTexture(GL_TEXTURE_2D, ((OGLTexture *)src)->m_TexId);
		CheckError("glBindTexture");

		if(hasck) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		} else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}

		glBindVertexArray(m_VAO);
		CheckError("glBindVertexArray");

		glDrawArrays(GL_TRIANGLES, 0, 6);
		CheckError("glDrawArrays");

		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		s.UnBind();


	}
	glRestoreEx(GL_BLEND, oldBlend);
	glRestoreEx(GL_CULL_FACE, oldCull);

	return true;
}

// ======================================================================
// ======================================================================
// class VisObject

VisObject::VisObject (OBJHANDLE hObj)
{
	hObject = hObj;
}

VisObject::~VisObject ()
{
}
