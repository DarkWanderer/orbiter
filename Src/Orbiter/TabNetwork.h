// Copyright (c) Martin Schweiger
// Licensed under the MIT License

//-----------------------------------------------------------------------------
// Launchpad tab definition: class NetworkTab
// Tab for "about" page
//-----------------------------------------------------------------------------

#ifndef __TABNETWORK_H
#define __TABNETWORK_H

#include "LpadTab.h"

namespace orbiter {

	class NetworkTab : public LaunchpadTab {
	public:
		NetworkTab(const LaunchpadDialog* lp);

		void Create();
		bool OpenHelp();

		INT_PTR TabProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	private:
		static INT_PTR CALLBACK AboutProc(HWND, UINT, WPARAM, LPARAM);
	};

}

#endif // !__TABNETWORK_H
