// DynLibUtils
// Copyright (C) 2023 komashchenko (Phoenix)
// https://github.com/komashchenko/DynLibUtils

#include "module.h"
#include "memaddr.h"
#include <cstring>
#include <cmath>
#include <windows.h>

using namespace DynLibUtils;

CModule::~CModule()
{
	if (m_pModuleHandle)
		FreeLibrary(reinterpret_cast<HMODULE>(m_pModuleHandle));
}

static std::string GetModulePath(HMODULE hModule)
{
	std::string modulePath(MAX_PATH, '\0');
	while (true)
	{
		size_t len = GetModuleFileNameA(hModule, modulePath.data(), static_cast<DWORD>(modulePath.length()));
		if (len == 0)
		{
			modulePath.clear();
			break;
		}

		if (len < modulePath.length())
		{
			modulePath.resize(len);
			break;
		}
		else
			modulePath.resize(modulePath.length() * 2);
	}

	return modulePath;
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the module from module name
// Input  : svModuleName
//          bExtension
// Output : bool
//-----------------------------------------------------------------------------
bool CModule::InitFromName(const std::string_view svModuleName, bool bExtension)
{
	if (m_pModuleHandle)
		return false;

	if (svModuleName.empty())
		return false;

	std::string sModuleName(svModuleName);
	if (!bExtension)
		sModuleName.append(".dll");

	HMODULE handle = GetModuleHandleA(sModuleName.c_str());
	if (!handle)
		return false;

	std::string modulePath = ::GetModulePath(handle);
	if(modulePath.empty())
		return false;

	if (!Init(modulePath))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the module from module memory
// Input  : pModuleMemory
// Output : bool
//-----------------------------------------------------------------------------
bool CModule::InitFromMemory(const CMemory pModuleMemory)
{
	if (m_pModuleHandle)
		return false;

	if (!pModuleMemory)
		return false;

	MEMORY_BASIC_INFORMATION mbi;
	if (!VirtualQuery(pModuleMemory, &mbi, sizeof(mbi)))
		return false;

	std::string modulePath = ::GetModulePath(reinterpret_cast<HMODULE>(mbi.AllocationBase));
	if (modulePath.empty())
		return false;

	if (!Init(modulePath))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Initializes a module descriptors
//-----------------------------------------------------------------------------
bool CModule::Init(const std::string_view svModelePath)
{
	HMODULE handle = LoadLibraryExA(svModelePath.data(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
	if (!handle)
		return false;

	IMAGE_DOS_HEADER* pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(handle);
	IMAGE_NT_HEADERS64* pNTHeaders = reinterpret_cast<IMAGE_NT_HEADERS64*>(reinterpret_cast<uintptr_t>(handle) + pDOSHeader->e_lfanew);

	const IMAGE_SECTION_HEADER* hSection = IMAGE_FIRST_SECTION(pNTHeaders); // Get first image section.

	for (WORD i = 0; i < pNTHeaders->FileHeader.NumberOfSections; ++i) // Loop through the sections.
	{
		const IMAGE_SECTION_HEADER& hCurrentSection = hSection[i]; // Get current section.
		m_vModuleSections.emplace_back(reinterpret_cast<const char*>(hCurrentSection.Name), static_cast<uintptr_t>(reinterpret_cast<uintptr_t>(handle) + hCurrentSection.VirtualAddress), hCurrentSection.SizeOfRawData); // Push back a struct with the section data.
	}

	m_pModuleHandle = handle;
	m_sModulePath.assign(svModelePath);

	m_ExecutableCode = GetSectionByName(".text");

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Gets an address of a virtual method table by rtti type descriptor name
// Input  : svTableName
//          bDecorated
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::GetVirtualTableByName(const std::string_view svTableName, bool bDecorated) const
{
	if(svTableName.empty())
		return CMemory();
	
	CModule::ModuleSections_t runTimeData = GetSectionByName(".data"), readOnlyData = GetSectionByName(".rdata");
	if(!runTimeData.IsSectionValid() || !readOnlyData.IsSectionValid())
		return CMemory();

	std::string sDecoratedTableName(bDecorated ? svTableName : ".?AV" + std::string(svTableName) + "@@");
	std::string sMask(sDecoratedTableName.length() + 1, 'x');

	CMemory typeDescriptorName = FindPattern(sDecoratedTableName.data(), sMask, nullptr, &runTimeData);
	if (!typeDescriptorName)
		return CMemory();

	CMemory rttiTypeDescriptor = typeDescriptorName.Offset(-0x10);
	const uintptr_t rttiTDRva = rttiTypeDescriptor - GetModuleBase(); // The RTTI gets referenced by a 4-Byte RVA address. We need to scan for that address.

	CMemory reference;
	while ((reference = FindPattern(&rttiTDRva, "xxxx", reference, &readOnlyData))) // Get reference typeinfo in vtable
	{
		// Check if we got a RTTI Object Locator for this reference by checking if -0xC is 1, which is the 'signature' field which is always 1 on x64.
		// Check that offset of this vtable is 0
		if (reference.Offset(-0xC).GetValue<int32_t>() == 1 && reference.Offset(-0x8).GetValue<int32_t>() == 0)
		{
			CMemory referenceOffset = reference.Offset(-0xC);
			CMemory rttiCompleteObjectLocator = FindPattern(&referenceOffset, "xxxxxxxx", nullptr, &readOnlyData);
			if (rttiCompleteObjectLocator)
				return rttiCompleteObjectLocator.Offset(0x8);
		}

		reference.OffsetSelf(0x4);
	}

	return CMemory();
}

//-----------------------------------------------------------------------------
// Purpose: Gets an address of a virtual method table by rtti type descriptor name
// Input  : svFunctionName
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::GetFunctionByName(const std::string_view svFunctionName) const noexcept
{
	if(!m_pModuleHandle)
		return CMemory();

	if (svFunctionName.empty())
		return CMemory();

	return GetProcAddress(reinterpret_cast<HMODULE>(m_pModuleHandle), svFunctionName.data());
}

//-----------------------------------------------------------------------------
// Purpose: Returns the module base
//-----------------------------------------------------------------------------
CMemory CModule::GetModuleBase() const noexcept
{
	return m_pModuleHandle;
}
