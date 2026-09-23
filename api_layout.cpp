#include "utils.h"
#include "menus_internal.h"

static CCSCustomHudLayout* UTIL_ResolveLayout(int iSlot, const char* szLayoutName)
{
	if(iSlot >= 0 && iSlot < 64)
	{
		auto it = g_mapHudLayouts[iSlot].find(szLayoutName);
		if(it != g_mapHudLayouts[iSlot].end() && it->second)
			return it->second;
	}
	auto itg = g_mapGlobalHudLayouts.find(szLayoutName);
	if(itg != g_mapGlobalHudLayouts.end() && itg->second)
		return itg->second;
	return nullptr;
}

void LayoutApi::Create(int iSlot, const char* szLayoutName, const char* szLayoutPath)
{
	if(iSlot < 0 || iSlot >= 64) return;
	CCSCustomHudLayout* layout = (CCSCustomHudLayout*)g_pUtilsApi->CreateEntityByName("custom_hud_layout", -1);
	CEntityKeyValues* pKeyValues = new CEntityKeyValues();
	pKeyValues->SetString("targetname", szLayoutName);
	pKeyValues->SetString("layout", szLayoutPath);
	g_pUtilsApi->DispatchSpawn(layout, pKeyValues);
	g_mapHudLayouts[iSlot][szLayoutName] = CHandle<CCSCustomHudLayout>(layout);
	// g_pUtilsApi->SetTransmitState(layout->entindex(), false, {});
	// g_pUtilsApi->SetTransmitState(layout->entindex(), true, {iSlot});
}

void LayoutApi::CreateGlobal(const char* szLayoutName, const char* szLayoutPath)
{
	if(!szLayoutName || !szLayoutName[0]) return;
	CCSCustomHudLayout* layout = (CCSCustomHudLayout*)g_pUtilsApi->CreateEntityByName("custom_hud_layout", -1);
	CEntityKeyValues* pKeyValues = new CEntityKeyValues();
	pKeyValues->SetString("targetname", szLayoutName);
	pKeyValues->SetString("layout", szLayoutPath);
	g_pUtilsApi->DispatchSpawn(layout, pKeyValues);
	g_mapGlobalHudLayouts[szLayoutName] = CHandle<CCSCustomHudLayout>(layout);
}

void LayoutApi::DestroyGlobal(const char* szLayoutName, float flDelay)
{
	std::string key = szLayoutName ? szLayoutName : "";
	auto it = g_mapGlobalHudLayouts.find(key);
	if(it == g_mapGlobalHudLayouts.end()) return;
	CHandle<CCSCustomHudLayout> layout = it->second;
	if(!layout)
	{
		g_mapGlobalHudLayouts.erase(key);
		return;
	}
	g_mapTransmitState[layout->entindex()].clear();
	if(flDelay > 0.f)
	{
		new CTimer(flDelay, [key]()
		{
			auto it2 = g_mapGlobalHudLayouts.find(key);
			if(it2 != g_mapGlobalHudLayouts.end())
			{
				g_pUtilsApi->RemoveEntity(it2->second);
				g_mapGlobalHudLayouts.erase(it2);
			}
			return -1.0f;
		});
	}
	else
	{
		g_pUtilsApi->RemoveEntity(layout);
		g_mapGlobalHudLayouts.erase(it);
	}
}

void LayoutApi::Destroy(int iSlot, const char* szLayoutName, float flDelay)
{
	if(iSlot < 0 || iSlot >= 64) return;
	std::string key = szLayoutName;
	if(g_mapHudLayouts[iSlot].find(key) != g_mapHudLayouts[iSlot].end())
	{
		CHandle<CCSCustomHudLayout> layout = g_mapHudLayouts[iSlot][key];
		if(layout)
		{
			g_mapTransmitState[layout->entindex()].clear();
			if(flDelay > 0.f)
			{
				new CTimer(flDelay, [iSlot, key]()
				{
					g_pUtilsApi->RemoveEntity(g_mapHudLayouts[iSlot][key]);
					g_mapHudLayouts[iSlot].erase(key);
					return -1.0f;
				});
			}
			else
			{
				g_pUtilsApi->RemoveEntity(layout);
				g_mapHudLayouts[iSlot].erase(key);
			}
		}
	}
}

void LayoutApi::SetHasClass(int iSlot, const char* szLayoutName, CUtlString szPanelId, CUtlString szClassName, bool bHasClass)
{
	if(iSlot < 0 || iSlot >= 64) return;
	CCSCustomHudLayout* layout = UTIL_ResolveLayout(iSlot, szLayoutName);
	if(layout)
		layout->SetHasClass(iSlot, szPanelId, szClassName, bHasClass);
}

void LayoutApi::SetInputCapture(int iSlot, const char* szLayoutName, bool bCapture)
{
	if(iSlot < 0 || iSlot >= 64) return;
	CCSCustomHudLayout* layout = UTIL_ResolveLayout(iSlot, szLayoutName);
	if(layout)
		layout->SetInputCaptureEnabled(iSlot, bCapture);
}

bool LayoutApi::GetInputCapture(int iSlot, const char* szLayoutName)
{
	if(iSlot < 0 || iSlot >= 64) return false;
	CCSCustomHudLayout* layout = UTIL_ResolveLayout(iSlot, szLayoutName);
	if(layout)
		return layout->GetInputCaptureEnabled(iSlot);
	return false;
}

void LayoutApi::SetDialogVariable(int iSlot, const char* szLayoutName, CUtlString szPanelId, CUtlString szVariableName, CUtlString szValue)
{
	if(iSlot < 0 || iSlot >= 64) return;
	CCSCustomHudLayout* layout = UTIL_ResolveLayout(iSlot, szLayoutName);
	if(layout)
		layout->SetDialogVariableString(iSlot, szPanelId, szVariableName, szValue);
}

const char* LayoutApi::GetDialogVariable(int iSlot, const char* szLayoutName, CUtlString szPanelId, CUtlString szVariableName)
{
	if(iSlot < 0 || iSlot >= 64) return "";
	CCSCustomHudLayout* layout = UTIL_ResolveLayout(iSlot, szLayoutName);
	if(layout)
		return layout->GetDialogVariableString(iSlot, szPanelId, szVariableName);
	return "";
}

void LayoutApi::SetGlobalHasClass(const char* szLayoutName, CUtlString szPanelId, CUtlString szClassName, bool bHasClass)
{
	for(int i = 0; i < 64; i++)
	{
		auto it = g_mapHudLayouts[i].find(szLayoutName);
		if(it == g_mapHudLayouts[i].end()) continue;
		CHandle<CCSCustomHudLayout> layout = it->second;
		if(layout)
			layout->SetGlobalHasClass(szPanelId.Get(), szClassName.Get(), bHasClass);
	}
	auto itg = g_mapGlobalHudLayouts.find(szLayoutName);
	if(itg != g_mapGlobalHudLayouts.end() && itg->second)
		itg->second->SetGlobalHasClass(szPanelId.Get(), szClassName.Get(), bHasClass);
}

void LayoutApi::SetGlobalDialogVariable(const char* szLayoutName, CUtlString szPanelId, CUtlString szVariableName, CUtlString szValue)
{
	for(int i = 0; i < 64; i++)
	{
		auto it = g_mapHudLayouts[i].find(szLayoutName);
		if(it == g_mapHudLayouts[i].end()) continue;
		CHandle<CCSCustomHudLayout> layout = it->second;
		if(layout)
			layout->SetGlobalDialogVariableString(szPanelId.Get(), szVariableName.Get(), szValue.Get());
	}
	auto itg = g_mapGlobalHudLayouts.find(szLayoutName);
	if(itg != g_mapGlobalHudLayouts.end() && itg->second)
		itg->second->SetGlobalDialogVariableString(szPanelId.Get(), szVariableName.Get(), szValue.Get());
}

void LayoutApi::SetGlobalInputCapture(const char* szLayoutName, bool bCapture)
{
	for(int i = 0; i < 64; i++)
	{
		auto it = g_mapHudLayouts[i].find(szLayoutName);
		if(it == g_mapHudLayouts[i].end()) continue;
		CHandle<CCSCustomHudLayout> layout = it->second;
		if(layout)
			layout->SetGlobalInputCaptureEnabled(bCapture);
	}
	auto itg = g_mapGlobalHudLayouts.find(szLayoutName);
	if(itg != g_mapGlobalHudLayouts.end() && itg->second)
		itg->second->SetGlobalInputCaptureEnabled(bCapture);
}
