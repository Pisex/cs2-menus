#pragma once

#include <functional>

#define LAYOUT_INTERFACE "ILayoutApi"

/////////////////////////////////////////////////////////////////
///////////////////////      LAYOUTS     //////////////////////////
/////////////////////////////////////////////////////////////////

typedef std::function<void(int iSlot, const char* szLayoutName, const char* szButton)> OnCustomHudClicked;

class ILayoutApi
{
public:
    virtual void Create(int iSlot, const char* szLayoutName, const char* szLayoutPath) = 0;
    virtual void SetHasClass(int iSlot, const char* szLayoutName, CUtlString szPanelId, CUtlString szClassName, bool bHasClass) = 0;
    virtual void SetInputCapture(int iSlot, const char* szLayoutName, bool bCapture) = 0;
    virtual bool GetInputCapture(int iSlot, const char* szLayoutName) = 0;
    virtual void SetDialogVariable(int iSlot, const char* szLayoutName, CUtlString szPanelId, CUtlString szVariableName, CUtlString szValue) = 0;
    virtual const char* GetDialogVariable(int iSlot, const char* szLayoutName, CUtlString szPanelId, CUtlString szVariableName) = 0;
    virtual void Destroy(int iSlot, const char* szLayoutName, float flDelay) = 0;
    virtual void HookOnCustomHudClicked(SourceMM::PluginId id, OnCustomHudClicked callback) = 0;

    virtual void SetGlobalHasClass(const char* szLayoutName, CUtlString szPanelId, CUtlString szClassName, bool bHasClass) = 0;
    virtual void SetGlobalDialogVariable(const char* szLayoutName, CUtlString szPanelId, CUtlString szVariableName, CUtlString szValue) = 0;
    virtual void SetGlobalInputCapture(const char* szLayoutName, bool bCapture) = 0;

    // Новые методы добавлены В КОНЕЦ vtable, чтобы не сдвигать индексы существующих
    // виртуальных методов и не ломать ABI для уже собранных плагинов.
    // Глобальный лейаут: одна сущность на всех игроков (без привязки к слоту).
    // Per-player состояние всё равно доступно через слот-индексированные Set*/Get*.
    virtual void CreateGlobal(const char* szLayoutName, const char* szLayoutPath) = 0;
    virtual void DestroyGlobal(const char* szLayoutName, float flDelay) = 0;
};
