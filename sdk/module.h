// DynLibUtils
// Copyright (C) 2023 komashchenko (Phoenix)
// https://github.com/komashchenko/DynLibUtils

#ifndef DYNLIBUTILS_MODULE_H
#define DYNLIBUTILS_MODULE_H
#ifdef _WIN32
#pragma once
#endif

#include "memaddr.h"
#include <vector>
#include <string>
#include <string_view>

namespace DynLibUtils {

class CModule
{
public:
	struct ModuleSections_t
	{
		ModuleSections_t() : m_nSectionSize(0) {}
		ModuleSections_t(const std::string_view svSectionName, uintptr_t pSectionBase, size_t nSectionSize) :
			m_svSectionName(svSectionName), m_pSectionBase(pSectionBase), m_nSectionSize(nSectionSize) {}

		[[nodiscard]] inline bool IsSectionValid() const noexcept
		{
			return m_pSectionBase;
		}

		std::string m_svSectionName; // Name of section.
		CMemory m_pSectionBase;      // Start address of section.
		size_t m_nSectionSize;       // Size of section.
	};

	CModule() : m_pModuleHandle(nullptr) {}
	~CModule();
	CModule (const CModule&) = delete;
	CModule& operator= (const CModule&) = delete;
	explicit CModule(const std::string_view svModuleName);
	explicit CModule(const char* pszModuleName) : CModule(std::string_view(pszModuleName)) {};
	explicit CModule(const std::string& sModuleName) : CModule(std::string_view(sModuleName)) {};
	CModule(const CMemory pModuleMemory);

	bool InitFromName(const std::string_view svModuleName, bool bExtension = false);
	bool InitFromMemory(const CMemory pModuleMemory);

	[[nodiscard]] static std::pair<std::vector<uint8_t>, std::string> PatternToMaskedBytes(const std::string_view svInput);
	[[nodiscard]] CMemory FindPattern(const CMemory pPattern, const std::string_view szMask, const CMemory pStartAddress = nullptr, const ModuleSections_t* pModuleSection = nullptr) const;
	[[nodiscard]] CMemory FindPattern(const std::string_view svPattern, const CMemory pStartAddress = nullptr, const ModuleSections_t* pModuleSection = nullptr) const;

	[[nodiscard]] CMemory GetVirtualTableByName(const std::string_view svTableName, bool bDecorated = false) const;
	[[nodiscard]] CMemory GetFunctionByName(const std::string_view svFunctionName) const noexcept;

	[[nodiscard]] ModuleSections_t GetSectionByName(const std::string_view svSectionName) const;
	[[nodiscard]] void* GetModuleHandle() const noexcept;
	[[nodiscard]] CMemory GetModuleBase() const noexcept;
	[[nodiscard]] std::string_view GetModulePath() const;
	[[nodiscard]] std::string_view GetModuleName() const;

private:
	bool Init(const std::string_view svModelePath);

	ModuleSections_t m_ExecutableCode;
	std::string m_sModulePath;
	void* m_pModuleHandle;
	std::vector<ModuleSections_t> m_vModuleSections;
};

} // namespace DynLibUtils

#endif // DYNLIBUTILS_MODULE_H
