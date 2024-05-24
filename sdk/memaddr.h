// DynLibUtils
// Copyright (C) 2023 komashchenko (Phoenix)
// https://github.com/komashchenko/DynLibUtils

#ifndef DYNLIBUTILS_MEMADDR_H
#define DYNLIBUTILS_MEMADDR_H
#ifdef _WIN32
#pragma once
#endif

#include <cstdint>
#include <cstddef>

namespace DynLibUtils {

class CMemory
{
public:
	CMemory() : m_ptr(0) {}
	CMemory(const uintptr_t ptr) : m_ptr(ptr) {}
	CMemory(const void* ptr) : m_ptr(reinterpret_cast<uintptr_t>(ptr)) {}

	inline operator uintptr_t() const noexcept
	{
		return m_ptr;
	}

	inline operator void*() const noexcept
	{
		return reinterpret_cast<void*>(m_ptr);
	}

	explicit inline operator bool() const noexcept
	{
		return m_ptr != 0;
	}

	inline bool operator!= (const CMemory& addr) const noexcept
	{
		return m_ptr != addr.m_ptr;
	}

	inline bool operator== (const CMemory& addr) const noexcept
	{
		return m_ptr == addr.m_ptr;
	}

	inline bool operator== (const uintptr_t& addr) const noexcept
	{
		return m_ptr == addr;
	}

	[[nodiscard]] inline uintptr_t GetPtr() const noexcept
	{
		return m_ptr;
	}

	template<class T> [[nodiscard]] inline T GetValue() const noexcept
	{
		return *reinterpret_cast<T*>(m_ptr);
	}

	template<typename T> [[nodiscard]] inline T CCast() const noexcept
	{
		return (T)m_ptr;
	}

	template<typename T> [[nodiscard]] inline T RCast() const noexcept
	{
		return reinterpret_cast<T>(m_ptr);
	}

	template<typename T> [[nodiscard]] inline T UCast() const noexcept
	{
		union { uintptr_t m_ptr; T cptr; } cast;
		return cast.m_ptr = m_ptr, cast.cptr;
	}

	[[nodiscard]] inline CMemory Offset(ptrdiff_t offset) const noexcept
	{
		return m_ptr + offset;
	}

	inline CMemory& OffsetSelf(ptrdiff_t offset) noexcept
	{
		m_ptr += offset;
		return *this;
	}

	[[nodiscard]] inline CMemory Deref(int deref = 1) const
	{
		uintptr_t reference = m_ptr;

		while (deref--)
		{
			if (reference)
				reference = *reinterpret_cast<uintptr_t*>(reference);
		}

		return reference;
	}

	inline CMemory& DerefSelf(int deref = 1)
	{
		while (deref--)
		{
			if (m_ptr)
				m_ptr = *reinterpret_cast<uintptr_t*>(m_ptr);
		}

		return *this;
	}

	[[nodiscard]] inline CMemory FollowNearCall(const ptrdiff_t opcodeOffset = 0x1, const ptrdiff_t nextInstructionOffset = 0x5) const
	{
		return ResolveRelativeAddress(opcodeOffset, nextInstructionOffset);
	}

	inline CMemory& FollowNearCallSelf(const ptrdiff_t opcodeOffset = 0x1, const ptrdiff_t nextInstructionOffset = 0x5)
	{
		return ResolveRelativeAddressSelf(opcodeOffset, nextInstructionOffset);
	}

	[[nodiscard]] inline CMemory ResolveRelativeAddress(const ptrdiff_t registerOffset = 0x0, const ptrdiff_t nextInstructionOffset = 0x4) const
	{
		const uintptr_t skipRegister = m_ptr + registerOffset;
		const int32_t relativeAddress = *reinterpret_cast<int32_t*>(skipRegister);
		const uintptr_t nextInstruction = m_ptr + nextInstructionOffset;
		return nextInstruction + relativeAddress;
	}

	inline CMemory& ResolveRelativeAddressSelf(const ptrdiff_t registerOffset = 0x0, const ptrdiff_t nextInstructionOffset = 0x4)
	{
		const uintptr_t skipRegister = m_ptr + registerOffset;
		const int32_t relativeAddress = *reinterpret_cast<int32_t*>(skipRegister);
		const uintptr_t nextInstruction = m_ptr + nextInstructionOffset;
		m_ptr = nextInstruction + relativeAddress;

		return *this;
	}

private:
	uintptr_t m_ptr;
};

} // namespace DynLibUtils

#endif // DYNLIBUTILS_MEMADDR_H
