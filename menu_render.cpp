#include "utils.h"
#include "menus_internal.h"

bool MenusApi::IsMenuOpen(int iSlot) {
	return g_MenuPlayer[iSlot].bEnabled;
}

void MenusApi::SetTitleMenu(Menu& hMenu, const char* szTitle) {
	hMenu.szTitle = std::string(szTitle);
}

void MenusApi::SetDescriptionMenu(Menu& hMenu, const char* szDescription) {
	if(szDescription && szDescription[0])
		g_mapMenuDesc[&hMenu] = szDescription;
	else
		g_mapMenuDesc.erase(&hMenu);
}

void MenusApi::SetBackMenu(Menu& hMenu, bool bBack) {
	hMenu.bBack = bBack;
}

void MenusApi::SetExitMenu(Menu& hMenu, bool bExit) {
	hMenu.bExit = bExit;
}

std::string GetMenuText(int iSlot)
{
	if (iSlot < 0 || iSlot >= 64) return "";
	auto& hMenuPlayer = g_MenuPlayer[iSlot];
	auto& hMenu = hMenuPlayer.hMenu;

	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if(!pController) return "";
	CBasePlayerPawn* pPlayerPawn = pController->m_hPawn();
	if(!pPlayerPawn) return "";
	CPlayer_MovementServices* pMovementServices = pPlayerPawn->m_pMovementServices();
	if(!pMovementServices) return "";
	CCSPlayerPawn* pPawn = pController->GetPlayerPawn();
	if(pPawn && pPawn->IsAlive() && g_bStopingUser && pPlayerPawn->m_nActualMoveType() == MOVETYPE_WALK) {
		g_pPlayersApi->SetMoveType(iSlot, MOVETYPE_NONE);
	}
	int buttons = pMovementServices->m_nButtons().m_pButtonStates()[0];
	auto now = std::chrono::system_clock::now();
	std::chrono::milliseconds iTime = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

	bool bUp = false;
	bool bDown = false;
	bool bLeft = false;
	bool bRight = false;
	bool bEnter = false;

	if(buttons)
	{
		if(buttons & (1 << 3))
			bUp = true;
		else if(buttons & (1 << 4))
			bDown = true;
		else if(buttons & (1 << 9))
			bLeft = true;
		else if(buttons & (1 << 10))
			bRight = true;
		else if(buttons & (1 << 5))
			bEnter = true;
		if(g_iMenuLastButtonInput[iSlot] < iTime) {
			g_iMenuLastButtonInput[iSlot] = iTime + std::chrono::milliseconds(g_iTimeoutMenu);
			if(buttons & (1 << 3)) {
				if(g_iMenuItem[iSlot] > 1) {
					if(g_mapSounds.find("move") != g_mapSounds.end()) {
						const char* szSound = g_mapSounds["move"].c_str();
						if(g_iSoundType == 1) g_pPlayersApi->EmitSound(iSlot, pPawn->entindex(), szSound, 100, 1.0f);
						else if(g_iSoundType == 2) engine->ClientCommand(iSlot, "play %s", szSound);
					}
					g_iMenuItem[iSlot]--;
				}
			} else if(buttons & (1 << 4)) {
				if(g_iMenuItem[iSlot] < 5 && g_iMenuItem[iSlot] < hMenu.hItems.size()) {
					if(g_mapSounds.find("move") != g_mapSounds.end()) {
						const char* szSound = g_mapSounds["move"].c_str();
						if(g_iSoundType == 1) g_pPlayersApi->EmitSound(iSlot, pPawn->entindex(), szSound, 100, 1.0f);
						else if(g_iSoundType == 2) engine->ClientCommand(iSlot, "play %s", szSound);
					}
					g_iMenuItem[iSlot]++;
				}
			} else if(buttons & (1 << 9)) {
				if(hMenuPlayer.iList != 0 || hMenuPlayer.hMenu.bBack)
				{
					g_iMenuItem[iSlot] = 1;
					if(hMenuPlayer.iList != 0)
					{
						hMenuPlayer.iList--;
						hMenuPlayer.iEnd = std::time(0) + g_iMenuTime;
					}
					else if(hMenu.hFunc) hMenu.hFunc("back", "back", 7, iSlot);
					if(g_mapSounds.find("back") != g_mapSounds.end()) {
						const char* szSound = g_mapSounds["back"].c_str();
						if(g_iSoundType == 1) g_pPlayersApi->EmitSound(iSlot, pPawn->entindex(), szSound, 100, 1.0f);
						else if(g_iSoundType == 2) engine->ClientCommand(iSlot, "play %s", szSound);
					}
				}
			} else if(buttons & (1 << 10)) {
				int iItems = size(hMenu.hItems) / 5;
				if (size(hMenu.hItems) % 5 > 0) iItems++;
				if(iItems > hMenuPlayer.iList+1)
				{
					g_iMenuItem[iSlot] = 1;
					hMenuPlayer.iList++;
					hMenuPlayer.iEnd = std::time(0) + g_iMenuTime;
					if(hMenu.hFunc) hMenu.hFunc("next", "next", 8, iSlot);
					if(g_mapSounds.find("next") != g_mapSounds.end()) {
						const char* szSound = g_mapSounds["next"].c_str();
						if(g_iSoundType == 1) g_pPlayersApi->EmitSound(iSlot, pPawn->entindex(), szSound, 100, 1.0f);
						else if(g_iSoundType == 2) engine->ClientCommand(iSlot, "play %s", szSound);
					}
				}
			} else if(buttons & (1 << 5)) {
				int iButton = g_iMenuItem[iSlot];
				g_iMenuItem[iSlot] = 1;
				int iItems = size(hMenu.hItems);
				int iItem = hMenuPlayer.iList*5+iButton-1;
				if(iItems > iItem && hMenu.hItems[iItem].iType == 1)
				{
					if(hMenu.hFunc) hMenu.hFunc(hMenu.hItems[iItem].sBack.c_str(), hMenu.hItems[iItem].sText.c_str(), iButton, iSlot);
					
					if(g_mapSounds.find("select") != g_mapSounds.end()) {
						const char* szSound = g_mapSounds["select"].c_str();
						if(g_iSoundType == 1) g_pPlayersApi->EmitSound(iSlot, pPawn->entindex(), szSound, 100, 1.0f);
						else if(g_iSoundType == 2) engine->ClientCommand(iSlot, "play %s", szSound);
					}
				}
			} else if(buttons & (1 << 13) && hMenu.bExit) {
				if(g_bStopingUser) g_pPlayersApi->SetMoveType(iSlot, MOVETYPE_WALK);
				CheckActionMenu(iSlot, CCSPlayerController::FromSlot(iSlot), 9);
				if(g_mapSounds.find("exit") != g_mapSounds.end()) {
					const char* szSound = g_mapSounds["exit"].c_str();
					if(g_iSoundType == 1) g_pPlayersApi->EmitSound(iSlot, pPawn->entindex(), szSound, 100, 1.0f);
					else if(g_iSoundType == 2) engine->ClientCommand(iSlot, "play %s", szSound);
				}
				return "";
			}
		}
	}

	std::string sBuff = "";
	char sBuff2[256];
	int iCount = 0;
	int iItems = size(hMenu.hItems) / 5;
	if (size(hMenu.hItems) % 5 > 0) iItems++;
	g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlTitle")].c_str(), hMenu.szTitle.c_str());
	sBuff += std::string(sBuff2);
	for (size_t l = hMenuPlayer.iList*5; l < hMenu.hItems.size(); ++l) {
		switch (hMenu.hItems[l].iType)
		{
			case 1:
				g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlButton")].c_str(), hMenu.hItems[l].sText.c_str());
				sBuff += std::string(sBuff2);
				break;
			case 2:
				g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlButtonBlock")].c_str(), hMenu.hItems[l].sText.c_str());
				sBuff += std::string(sBuff2);
				break;
		}
		if(g_iMenuItem[iSlot] == iCount+1) {
			if(g_bMenuAddon) {
				if(bEnter) sBuff += g_vecPhrases[std::string("HtmlE_PButtons")];
				else sBuff += g_vecPhrases[std::string("HtmlEButtons")];
			} else {
				if(bEnter) g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlE_PButtons_Web")].c_str(), g_szMenuURL);
				else g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlEButtons_Web")].c_str(), g_szMenuURL);
				sBuff += sBuff2;
			}
		}
		sBuff += g_vecPhrases[std::string("HtmlButtonBR")];
		iCount++;
		if(iCount == 5 || l == hMenu.hItems.size()-1)
		{
			int iC = 5;
			if(hMenuPlayer.iList == 0 && !hMenu.bBack) iC++;

			if(g_bMenuAddon) {
				if(bUp) sBuff += g_vecPhrases[std::string("HtmlW_PButtons")];
				else sBuff += g_vecPhrases[std::string("HtmlWButtons")];
			} else {
				if(bUp) g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlW_PButtons_Web")].c_str(), g_szMenuURL);
				else g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlWButtons_Web")].c_str(), g_szMenuURL);
				sBuff += sBuff2;
			}

			if(g_bMenuAddon) {
				if(bDown) sBuff += g_vecPhrases[std::string("HtmlS_PButtons")];
				else sBuff += g_vecPhrases[std::string("HtmlSButtons")];
			} else {
				if(bDown) g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlS_PButtons_Web")].c_str(), g_szMenuURL);
				else g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlSButtons_Web")].c_str(), g_szMenuURL);
				sBuff += sBuff2;
			}

			if(hMenuPlayer.iList > 0 || hMenu.bBack) 
			{
				if(g_bMenuAddon) {
					if(bLeft) sBuff += g_vecPhrases[std::string("HtmlA_PButtons")];
					else sBuff += g_vecPhrases[std::string("HtmlAButtons")];
				} else {
					if(bLeft) g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlA_PButtons_Web")].c_str(), g_szMenuURL);
					else g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlAButtons_Web")].c_str(), g_szMenuURL);
					sBuff += sBuff2;
				}

				if(iItems <= hMenuPlayer.iList+1) {
					if(g_bMenuAddon) sBuff += g_vecPhrases[std::string("HtmlSpaceShortButtons")];
					else {
						g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlSpaceShortButtons_Web")].c_str(), g_szMenuURL);
						sBuff += sBuff2;
					}
				} else {
					if(g_bMenuAddon) {
						if(bRight) sBuff += g_vecPhrases[std::string("HtmlD_PButtons")];
						else sBuff += g_vecPhrases[std::string("HtmlDButtons")];
					} else {
						if(bRight) g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlD_PButtons_Web")].c_str(), g_szMenuURL);
						else g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlDButtons_Web")].c_str(), g_szMenuURL);
						sBuff += sBuff2;
					}
				}
			}
			else if(iItems > hMenuPlayer.iList+1) 
			{
				if(hMenuPlayer.iList == 0 || !hMenu.bBack) {
					if(g_bMenuAddon) sBuff += g_vecPhrases[std::string("HtmlSpaceShortButtons")];
					else {
						g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlSpaceShortButtons_Web")].c_str(), g_szMenuURL);
						sBuff += sBuff2;
					}
				}
				
				if(g_bMenuAddon) {
					if(bRight) sBuff += g_vecPhrases[std::string("HtmlD_PButtons")];
					else sBuff += g_vecPhrases[std::string("HtmlDButtons")];
				} else {
					if(bRight) g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlD_PButtons_Web")].c_str(), g_szMenuURL);
					else g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlDButtons_Web")].c_str(), g_szMenuURL);
					sBuff += sBuff2;
				}
			}
			else if(hMenu.bExit) {
				if(g_bMenuAddon) sBuff += g_vecPhrases[std::string("HtmlSpaceButtons")];
				else {
					g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlSpaceButtons_Web")].c_str(), g_szMenuURL);
					sBuff += sBuff2;
				}
			}
			if(hMenu.bExit) {
				if(g_bMenuAddon) sBuff += g_vecPhrases[std::string("HtmlFButtons")];
				else {
					g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlFButtons_Web")].c_str(), g_szMenuURL);
					sBuff += sBuff2;
				}
			}

			break;
		}
	}
	return sBuff;
}

int RoundToCeil(float value) {
	return static_cast<int>(ceil(value));
}

MenuType MenusApi::GetMenuType(int iSlot)
{
    if (iSlot < 0 || iSlot >= 64)
        return static_cast<MenuType>(g_iMenuTypeDefault);

    return static_cast<MenuType>(g_iMenuType[iSlot]);
}

static const char* const g_szLayoutName = "utils-dialog";
static const char* const g_szItemKinds[] = { "k-empty", "k-value", "k-submenu", "k-toggle", "k-select" };
static const int g_iItemsPerPage = 6;
static const int g_iMaxOptions = 8;

static int g_iNotifyGen[64] = {0};

static void UTIL_SetLayoutItemKind(int iSlot, const char* szItemId, const char* szKind)
{
	for(const char* k : g_szItemKinds)
	{
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItemId, k,
			(strcmp(k, szKind) == 0) ? true : false);
	}
}

static void UTIL_RenderLayoutItem(int iSlot, Menu& hMenu, MenuPlayer& hMenuPlayer, int i)
{
	char szItem[64];
	g_SMAPI->Format(szItem, sizeof(szItem), "utils-item-%i", i);
	int iIndex = hMenuPlayer.iList * g_iItemsPerPage + i;

	if(iIndex >= (int)hMenu.hItems.size())
	{
		UTIL_SetLayoutItemKind(iSlot, szItem, "k-empty");
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-disabled", false);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-on", false);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-open", false);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-up", false);
		g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, szItem, "label", "");
		g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, szItem, "value", "");
		return;
	}

	const auto& item = hMenu.hItems[iIndex];
	g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, szItem, "label", item.sText.c_str());
	g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-disabled",
		item.iType == 2 ? true : false);

	static ItemExtra kEmptyExtra;
	const ItemExtra& ex = (iIndex < (int)g_vItemExtra[iSlot].size())
		? g_vItemExtra[iSlot][iIndex] : kEmptyExtra;

	if(ex.iKind == ItemKind::TOGGLE)
	{
		UTIL_SetLayoutItemKind(iSlot, szItem, "k-toggle");
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-on",
			ex.bToggled ? true : false);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-open", false);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-up", false);
	}
	else if(ex.iKind == ItemKind::SELECT)
	{
		UTIL_SetLayoutItemKind(iSlot, szItem, "k-select");
		int iSel = ex.iSelected;
		if(iSel < 0 || iSel >= (int)ex.vOptions.size()) iSel = 0;
		const char* szValue = ex.vOptions.empty() ? "" : ex.vOptions[iSel].sText.c_str();
		g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, szItem, "value", szValue);
		char szOpt[64];
		for(int j = 0; j < g_iMaxOptions; j++)
		{
			g_SMAPI->Format(szOpt, sizeof(szOpt), "utils-opt-%i-%i", i, j);
			if(j < (int)ex.vOptions.size())
			{
				g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, szOpt, "value", ex.vOptions[j].sText.c_str());
				g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szOpt, "o-show", true);
				g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szOpt, "utils-sel",
					(j == iSel) ? true : false);
			}
			else
			{
				g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szOpt, "o-show", false);
				g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szOpt, "utils-sel", false);
			}
		}
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-up",
			(i >= 3) ? true : false);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-open",
			ex.bOpened ? true : false);
	}
	else
	{
		UTIL_SetLayoutItemKind(iSlot, szItem, "k-submenu");
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-on", false);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-open", false);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, szItem, "is-up", false);
	}
}

static void UTIL_RenderLayoutContent(int iSlot, Menu& hMenu, MenuPlayer& hMenuPlayer)
{
	g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, "utils-dialog", "title", hMenu.szTitle.c_str());
	g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, "utils-dialog", "desc",
		g_szMenuDesc[iSlot].size() > 0 ? g_szMenuDesc[iSlot].c_str() : "");

	int iPages = RoundToCeil(size(hMenu.hItems) / (double)g_iItemsPerPage);
	if(iPages < 1) iPages = 1;
	char szPage[64];
	g_SMAPI->Format(szPage, sizeof(szPage), "%i / %i", hMenuPlayer.iList + 1, iPages);
	g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, "utils-page", "page", szPage);
	g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-prev", "utils-disabled",
		hMenuPlayer.iList == 0 ? true : false);
	g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-next", "utils-disabled",
		iPages <= hMenuPlayer.iList + 1 ? true : false);

	g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-back-anchor", "utils-hide",
		hMenu.bBack ? false : true);

	for(int i = 0; i < g_iItemsPerPage; i++)
		UTIL_RenderLayoutItem(iSlot, hMenu, hMenuPlayer, i);
}

static void UTIL_CloseLayout(int iSlot, float flDelay)
{
	(void)flDelay;
	g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-dialog", "utils-dismissed", true);
	g_pLayoutApi->SetInputCapture(iSlot, g_szLayoutName, false);
}

void UTIL_EnsureLayout(int iSlot)
{
	(void)iSlot;
	if(g_mapGlobalHudLayouts.find(g_szLayoutName) == g_mapGlobalHudLayouts.end())
		g_pLayoutApi->CreateGlobal(g_szLayoutName, "panorama/layout/custom_game/menu_ui.xml");
}

void UTIL_DestroyLayout(int iSlot)
{
	(void)iSlot;
}

void UTIL_ResetPlayerSlot(int iSlot)
{
	if(iSlot < 0 || iSlot >= 64) return;

	// Погасить панораму и снять input capture именно для этого слота,
	// если глобальный layout уже создан. Нужно при переиспользовании слота:
	// новый игрок получает индекс, где мог остаться стейт прошлого владельца.
	if(g_pLayoutApi && g_mapGlobalHudLayouts.find(g_szLayoutName) != g_mapGlobalHudLayouts.end())
	{
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-dialog", "utils-dismissed", true);
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-notify", "ntf-hidden", true);
		g_pLayoutApi->SetInputCapture(iSlot, g_szLayoutName, false);
	}

	g_MenuPlayer[iSlot].clear();
	g_TextMenuPlayer[iSlot].clear();
	g_szMenuDesc[iSlot].clear();
	g_vItemExtra[iSlot].clear();
	g_iMenuItem[iSlot] = 1;
	g_iMenuLastButtonInput[iSlot] = std::chrono::milliseconds(0);
	g_iNotifyGen[iSlot]++; // инвалидировать висящие notify-таймеры прошлого игрока
	g_iMenuType[iSlot] = UTIL_SanitizeMenuType(g_iMenuTypeDefault);
	g_bNotifyDisabled[iSlot] = g_bNotifyDisabledDefault;
}

void UtilsApi::ShowNotify(int iSlot, int iType, const char* szTitle, const char* szText, float flDuration, int iPos, const char* szChatFallback)
{
	if(iSlot < 0 || iSlot >= 64) return;
	if(!g_pLayoutApi) return;

	if(g_bAllowDisableNotify && g_bNotifyDisabled[iSlot])
	{
		if(szChatFallback && szChatFallback[0])
			g_pUtilsCore->PrintToChat(iSlot, "%s", szChatFallback);
		return;
	}

	if(!g_bPanoramaMenu)
	{
		if(szChatFallback && szChatFallback[0])
			g_pUtilsCore->PrintToChat(iSlot, "%s", szChatFallback);
		return;
	}

	UTIL_EnsureLayout(iSlot);

	static const char* const s_szTypeClass[] = { "ntf-success", "ntf-warning", "ntf-error" };
	static const char* const s_szTypeIcon[]  = { "✓", "!", "✕" };
	if(iType < 0 || iType > 2) iType = 0;

	for(int i = 0; i < 3; i++)
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-notify", s_szTypeClass[i],
			(i == iType) ? true : false);

	static const char* const s_szPosClass[] = {
		"ntf-pos-tr", "ntf-pos-tl", "ntf-pos-br", "ntf-pos-bl",
		"ntf-pos-tc", "ntf-pos-c", "ntf-pos-bc", "ntf-pos-cl", "ntf-pos-cr"
	};
	const int iPosCount = (int)(sizeof(s_szPosClass) / sizeof(s_szPosClass[0]));
	if(iPos < 0 || iPos >= iPosCount) iPos = 0;

	for(int i = 0; i < iPosCount; i++)
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-notify", s_szPosClass[i],
			(i == iPos) ? true : false);

	g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, "utils-notify", "ntficon", s_szTypeIcon[iType]);
	g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, "utils-notify", "ntftitle", szTitle ? szTitle : "");
	g_pLayoutApi->SetDialogVariable(iSlot, g_szLayoutName, "utils-notify", "ntftext", szText ? szText : "");

	g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-notify", "ntf-hidden", false);

	int iGen = ++g_iNotifyGen[iSlot];
	if(flDuration < 0.1f) flDuration = 0.1f;
	new CTimer(flDuration, [iSlot, iGen]() {
		if(g_iNotifyGen[iSlot] == iGen && g_pLayoutApi)
			g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-notify", "ntf-hidden", true);
		return -1.0f;
	});
}

void UtilsApi::ShowNotifyAll(int iType, const char* szTitle, const char* szText, float flDuration, int iPos, const char* szChatFallback)
{
	for(int i = 0; i < 64; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
		if(!pController || g_pPlayersApi->IsFakeClient(i)) continue;
		ShowNotify(i, iType, szTitle, szText, flDuration, iPos, szChatFallback);
	}
}


void UTIL_HandleLayoutClick(int iSlot, const char* szLayoutName, const char* szButton)
{
	if(iSlot < 0 || iSlot >= 64) return;
	if(!szLayoutName || strcmp(szLayoutName, g_szLayoutName) != 0) return;
	auto& hMenuPlayer = g_MenuPlayer[iSlot];
	if(!hMenuPlayer.bEnabled) return;
	if(g_iMenuType[iSlot] != 3) return;
	auto& hMenu = hMenuPlayer.hMenu;
	hMenuPlayer.iEnd = std::time(0) + g_iMenuTime;

	if(strcmp(szButton, "utils-closeBtn") == 0)
	{
		if(hMenu.hFunc && hMenu.bExit) hMenu.hFunc("exit", "exit", 9, iSlot);
		UTIL_CloseLayout(iSlot, 0.2f);
		hMenuPlayer.clear();
		g_iMenuItem[iSlot] = 1;
		return;
	}

	if(strcmp(szButton, "utils-back") == 0)
	{
		if(hMenu.bBack && hMenu.hFunc) hMenu.hFunc("back", "back", 7, iSlot);
		return;
	}

	if(strcmp(szButton, "utils-prev") == 0)
	{
		if(hMenuPlayer.iList > 0)
		{
			hMenuPlayer.iList--;
			UTIL_RenderLayoutContent(iSlot, hMenu, hMenuPlayer);
		}
		return;
	}
	if(strcmp(szButton, "utils-next") == 0)
	{
		int iPages = RoundToCeil(size(hMenu.hItems) / (double)g_iItemsPerPage);
		if(iPages > hMenuPlayer.iList + 1)
		{
			hMenuPlayer.iList++;
			UTIL_RenderLayoutContent(iSlot, hMenu, hMenuPlayer);
			if(hMenu.hFunc) hMenu.hFunc("next", "next", 8, iSlot);
		}
		return;
	}

	int iVis = -1, iOpt = -1;
	if(sscanf(szButton, "utils-opt-%d-%d", &iVis, &iOpt) == 2 && iVis >= 0 && iVis < g_iItemsPerPage && iOpt >= 0)
	{
		int iIndex = hMenuPlayer.iList * g_iItemsPerPage + iVis;
		if(iIndex < (int)hMenu.hItems.size())
		{
			auto& item = hMenu.hItems[iIndex];
			if(iIndex >= (int)g_vItemExtra[iSlot].size()) return;
			ItemExtra& ex = g_vItemExtra[iSlot][iIndex];
			if(ex.iKind == ItemKind::SELECT && iOpt < (int)ex.vOptions.size())
			{
				ex.iSelected = iOpt;
				ex.bOpened = false;
				if(ex.hSelectFunc)
					ex.hSelectFunc(item.sBack.c_str(), ex.vOptions[iOpt].sBack.c_str(), iOpt, iVis + 1, iSlot);
				UTIL_RenderLayoutItem(iSlot, hMenu, hMenuPlayer, iVis);
			}
		}
		return;
	}

	if(sscanf(szButton, "utils-item-%d", &iVis) == 1 && iVis >= 0 && iVis < g_iItemsPerPage)
	{
		int iIndex = hMenuPlayer.iList * g_iItemsPerPage + iVis;
		if(iIndex >= (int)hMenu.hItems.size()) return;
		auto& item = hMenu.hItems[iIndex];
		if(item.iType == 2) return;
		if(iIndex >= (int)g_vItemExtra[iSlot].size()) return;
		ItemExtra& ex = g_vItemExtra[iSlot][iIndex];
		if(ex.iKind == ItemKind::TOGGLE)
		{
			ex.bToggled = !ex.bToggled;
			if(ex.hToggleFunc) ex.hToggleFunc(item.sBack.c_str(), ex.bToggled, iVis + 1, iSlot);
			UTIL_RenderLayoutItem(iSlot, hMenu, hMenuPlayer, iVis);
		}
		else if(ex.iKind == ItemKind::SELECT)
		{
			bool bOpenNow = !ex.bOpened;
			for(int k = 0; k < g_iItemsPerPage; k++)
			{
				int idx = hMenuPlayer.iList * g_iItemsPerPage + k;
				if(idx < (int)g_vItemExtra[iSlot].size() && g_vItemExtra[iSlot][idx].iKind == ItemKind::SELECT)
				{
					bool bWant = (k == iVis) ? bOpenNow : false;
					if(g_vItemExtra[iSlot][idx].bOpened != bWant)
					{
						g_vItemExtra[iSlot][idx].bOpened = bWant;
						UTIL_RenderLayoutItem(iSlot, hMenu, hMenuPlayer, k);
					}
				}
			}
		}
		else
		{
			if(item.iType == 1 && hMenu.hFunc)
				hMenu.hFunc(item.sBack.c_str(), item.sText.c_str(), iVis + 1, iSlot);
		}
		return;
	}
}

void MenusApi::DisplayPlayerMenu(Menu& hMenu, int iSlot, bool bClose = true)
{
	if(iSlot < 0 || iSlot >= 64) return;
	DisplayPlayerMenu(hMenu, iSlot, bClose, true);
}

void MenusApi::DisplayPlayerMenu(Menu& hMenu, int iSlot, bool bClose = true, bool bReset = true)
{
	if(iSlot < 0 || iSlot >= 64) return;
	
	g_iMenuType[iSlot] = UTIL_SanitizeMenuType(g_iMenuType[iSlot]);
    MenuPlayer& hMenuPlayer = g_MenuPlayer[iSlot];
	if (hMenuPlayer.bEnabled && bClose && bReset) {
		hMenuPlayer.clear();
	}
	if(!hMenuPlayer.bEnabled || !bReset)
	{
		g_iMenuItem[iSlot] = 1;
		hMenuPlayer.bEnabled = true;
		hMenuPlayer.hMenu = hMenu;
		{
			auto it = g_mapMenuDesc.find(&hMenu);
			g_szMenuDesc[iSlot] = (it != g_mapMenuDesc.end()) ? it->second : std::string();
		}
		{
			auto it = g_mapItemExtra.find(&hMenu);
			if(it != g_mapItemExtra.end()) g_vItemExtra[iSlot] = it->second;
			else g_vItemExtra[iSlot].clear();
			g_vItemExtra[iSlot].resize(hMenuPlayer.hMenu.hItems.size());
		}
		hMenuPlayer.iEnd = std::time(0) + g_iMenuTime;
		if(hMenuPlayer.iList > 0) {
			double dPerPage = (g_iMenuType[iSlot] == 3) ? (double)g_iItemsPerPage : 5.0;
			int iLists = RoundToCeil(size(hMenu.hItems) / dPerPage);
			while (iLists < hMenuPlayer.iList + 1) {
				hMenuPlayer.iList--;
			}
		}
		new CTimer(0.0f,[iSlot, &hMenu, &hMenuPlayer]() {
			if(!hMenuPlayer.bEnabled) return -1.0f;
			if(std::time(0) >= hMenuPlayer.iEnd)
			{
				hMenuPlayer.clear();
				if(g_iMenuType[iSlot] == 3) UTIL_CloseLayout(iSlot, 0.2f);
				if(g_iMenuType[iSlot] >= 2 && g_bStopingUser) {
					g_pPlayersApi->SetMoveType(iSlot, MOVETYPE_WALK);
				}
				return -1.0f;
			}
			if(g_iMenuType[iSlot] == 1 && g_TextMenuPlayer[iSlot].size() > 0) {
				g_pUtilsCore->PrintToCenterHtml(iSlot, 0.0f, g_TextMenuPlayer[iSlot].c_str());
			}
			if(g_iMenuType[iSlot] == 2) {
				std::string szMenu = GetMenuText(iSlot);
				if(szMenu.size() > 0) g_pUtilsCore->PrintToCenterHtml(iSlot, 0.0f, szMenu.c_str());
				else {
					hMenuPlayer.clear();
					if(g_iMenuType[iSlot] == 2 && g_bStopingUser) {
						g_pPlayersApi->SetMoveType(iSlot, MOVETYPE_WALK);
					}
					return -1.0f;
				}
			}
			return 0.0f;
		});
	}
	if(g_iMenuType[iSlot] != 3 && g_mapGlobalHudLayouts.find(g_szLayoutName) != g_mapGlobalHudLayouts.end())
	{
		g_pLayoutApi->SetHasClass(iSlot, g_szLayoutName, "utils-dialog", "utils-dismissed", true);
		g_pLayoutApi->SetInputCapture(iSlot, g_szLayoutName, false);
	}
	if(g_iMenuType[iSlot] == 0)
	{
		char sBuff[128] = "\0";
		int iCount = 0;
		int iItems = size(hMenu.hItems) / 5;
		if (size(hMenu.hItems) % 5 > 0) iItems++;
		g_pUtilsCore->PrintToChat(iSlot, hMenu.szTitle.c_str());
		for (size_t l = hMenuPlayer.iList*5; l < hMenu.hItems.size(); ++l) {
			switch (hMenu.hItems[l].iType)
			{
				case 1:
				case 2:
				{
					std::string cleanText = hMenu.hItems[l].sText;

					size_t startPos = 0;
					while ((startPos = cleanText.find('<', startPos)) != std::string::npos)
					{
						size_t endPos = cleanText.find('>', startPos);
						if (endPos != std::string::npos)
							cleanText.erase(startPos, endPos - startPos + 1);
						else
							break;
					}

					const char* colorCode = (hMenu.hItems[l].iType == 1) ? "\x04" : "\x08";
					g_SMAPI->Format(sBuff, sizeof(sBuff), " %s[!%i]\x01 %s", colorCode, iCount + 1, cleanText.c_str());
					g_pUtilsCore->PrintToChat(iSlot, sBuff);
					break;
				}
			}
			iCount++;
			if(iCount == 5 || l == hMenu.hItems.size()-1)
			{
				int iC = 5;
				if(hMenuPlayer.iList == 0 && !hMenu.bBack) iC++;
				if(l == hMenu.hItems.size()-1)
				{
					for (int i = 0; i < iC-iCount; i++)
					{
						g_pUtilsCore->PrintToChat(iSlot, " \x08-\x01");
					}
				}
				if(hMenuPlayer.iList > 0 || hMenu.bBack) g_pUtilsCore->PrintToChat(iSlot, g_vecPhrases[std::string("Back")].c_str());
				if(iItems > hMenuPlayer.iList+1) g_pUtilsCore->PrintToChat(iSlot, g_vecPhrases[std::string("Next")].c_str());
				g_pUtilsCore->PrintToChat(iSlot, g_vecPhrases[std::string("Exit")].c_str());
				break;
			}
		}
	}
	else if(g_iMenuType[iSlot] == 1)
	{
		std::string sBuff = "";
		char sBuff2[256];
		int iCount = 0;
		int iItems = size(hMenu.hItems) / 5;
		if (size(hMenu.hItems) % 5 > 0) iItems++;
		g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlTitle")].c_str(), hMenu.szTitle.c_str());
		sBuff += std::string(sBuff2);
		for (size_t l = hMenuPlayer.iList*5; l < hMenu.hItems.size(); ++l) {
			switch (hMenu.hItems[l].iType)
			{
				case 1:
					g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlNumber")].c_str(), iCount+1, hMenu.hItems[l].sText.c_str());
					sBuff += std::string(sBuff2);
					break;
				case 2:
					g_SMAPI->Format(sBuff2, sizeof(sBuff2), g_vecPhrases[std::string("HtmlNumberBlock")].c_str(), iCount+1, hMenu.hItems[l].sText.c_str());
					sBuff += std::string(sBuff2);
					break;
			}
			iCount++;
			if(iCount == 5 || l == hMenu.hItems.size()-1)
			{
				int iC = 5;
				if(hMenuPlayer.iList == 0 && !hMenu.bBack) iC++;
				if(hMenuPlayer.iList > 0 || hMenu.bBack) sBuff += g_vecPhrases[std::string("HtmlBack")];
				if(iItems > hMenuPlayer.iList+1) sBuff += g_vecPhrases[std::string("HtmlNext")];
				sBuff += g_vecPhrases[std::string("HtmlExit")];
				break;
			}
		}
		g_TextMenuPlayer[iSlot] = sBuff;
	}
	else if(g_iMenuType[iSlot] == 3)
	{
		UTIL_EnsureLayout(iSlot);

		UTIL_RenderLayoutContent(iSlot, hMenu, hMenuPlayer);

		g_pLayoutApi->SetHasClass(iSlot, "utils-dialog", "utils-dialog", "utils-dismissed", false);
		g_pLayoutApi->SetInputCapture(iSlot, "utils-dialog", true);
	}
}

std::string MenusApi::escapeString(const std::string& input) {
    std::string escaped;
    for (char c : input) {
        switch (c) {
            case '\\': escaped += "\\\\"; break;
            case '\"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
			case '%': escaped += "%%"; break;
            default: escaped += c; break;
        }
    }
    return escaped;
}

void MenusApi::AddItemMenu(Menu& hMenu, const char* sBack, const char* sText, int iType = 1)
{
    if (iType == 0) return;

	Items hItem;
	hItem.iType = iType;
	hItem.sBack = std::string(sBack);
	hItem.sText = escapeString(sText);
	hMenu.hItems.push_back(hItem);
	
	UTIL_PushItemExtra(hMenu);
}

void MenusApi::AddRawItemMenu(Menu &hMenu, const char* sBack, const char* sText, int iType = 1)
{
    if (iType == 0) return;

    Items hItem;
    hItem.iType = iType;
    hItem.sBack = std::string(sBack);
    hItem.sText = std::string(sText);
    hMenu.hItems.push_back(hItem);
	UTIL_PushItemExtra(hMenu);
}

void MenusApi::AddToggleMenu(Menu& hMenu, const char* sBack, const char* sText, bool bDefault = false, MenuToggleCallbackFunc func = nullptr, int iType = 1)
{
	if (iType == 0) return;

	Items hItem;
	hItem.iType = iType;
	hItem.sBack = std::string(sBack);
	hItem.sText = escapeString(sText);
	hMenu.hItems.push_back(hItem);
	ItemExtra& ex = UTIL_PushItemExtra(hMenu);
	ex.iKind = ItemKind::TOGGLE;
	ex.bToggled = bDefault;
	ex.hToggleFunc = func;
}

ItemRef MenusApi::AddSelectMenu(Menu& hMenu, const char* sBack, const char* sText, int iDefault = 0, MenuSelectCallbackFunc func = nullptr, int iType = 1)
{
	Items hItem;
	hItem.iType = (iType == 0) ? 1 : iType;
	hItem.sBack = std::string(sBack);
	hItem.sText = escapeString(sText);
	hMenu.hItems.push_back(hItem);
	ItemExtra& ex = UTIL_PushItemExtra(hMenu);
	ex.iKind = ItemKind::SELECT;
	ex.iSelected = iDefault < 0 ? 0 : iDefault;
	ex.bOpened = false;
	ex.hSelectFunc = func;
	return ItemRef{ &hMenu, (int)hMenu.hItems.size() - 1 };
}

void MenusApi::AddSelectOption(ItemRef hItem, const char* sOptionBack, const char* sOptionText)
{
	ItemExtra* ex = UTIL_ResolveItemExtra(hItem);
	if (!ex) return;
	SelectOption hOption;
	hOption.sBack = std::string(sOptionBack);
	hOption.sText = escapeString(sOptionText);
	ex->vOptions.push_back(hOption);
}

void MenusApi::SetToggleState(ItemRef hItem, bool bState)
{
	ItemExtra* ex = UTIL_ResolveItemExtra(hItem);
	if (ex) ex->bToggled = bState;
}

bool MenusApi::GetToggleState(ItemRef hItem)
{
	ItemExtra* ex = UTIL_ResolveItemExtra(hItem);
	return ex ? ex->bToggled : false;
}

void MenusApi::SetSelectedOption(ItemRef hItem, int iOption)
{
	ItemExtra* ex = UTIL_ResolveItemExtra(hItem);
	if (!ex) return;
	if (ex->vOptions.empty()) { ex->iSelected = 0; return; }
	if (iOption < 0) iOption = 0;
	if (iOption >= (int)ex->vOptions.size()) iOption = (int)ex->vOptions.size() - 1;
	ex->iSelected = iOption;
}

int MenusApi::GetSelectedOption(ItemRef hItem)
{
	ItemExtra* ex = UTIL_ResolveItemExtra(hItem);
	return ex ? ex->iSelected : 0;
}

void MenusApi::ClosePlayerMenu(int iSlot)
{
	if(iSlot < 0 || iSlot >= 64) return;
	if(g_iMenuType[iSlot] == 3) UTIL_CloseLayout(iSlot, 0.2f);
	if(g_iMenuType[iSlot] >= 2 && g_bStopingUser) {
		g_pPlayersApi->SetMoveType(iSlot, MOVETYPE_WALK);
	}
	g_MenuPlayer[iSlot].clear();
	g_TextMenuPlayer[iSlot] = "";
	g_iMenuItem[iSlot] = 1;
}

