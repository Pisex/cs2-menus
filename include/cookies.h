#pragma once

#define COOKIES_INTERFACE "ICookiesApi"

typedef std::function<void(int iSlot)> ClientCookieLoadedCallback;

class ICookiesApi
{
public:
	virtual void SetCookie(int iSlot, const char* sCookieName, const char* sData) = 0;
    virtual const char* GetCookie(int iSlot, const char* sCookieName) = 0;
    virtual void HookClientCookieLoaded(SourceMM::PluginId id, ClientCookieLoadedCallback callback) = 0;
};