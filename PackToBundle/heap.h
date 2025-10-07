#pragma once
#include <Windows.h>
#include <vector>
#include <algorithm>
#include "threadcer.h"

class heapmgr
{
	public:
	static heapmgr &instance () noexcept
	{
		static heapmgr s_instance;
		return s_instance;
	}
	template <typename T> T *alloc (size_t size, DWORD dwFlags = HEAP_ZERO_MEMORY) noexcept
	{
		CreateScopedLock (m_cs);
		if (!m_hHeap) return nullptr;
		T *p = static_cast <T *> (HeapAlloc (m_hHeap, dwFlags, sizeof (T) * size));
		if (p) m_allocations.push_back (p);
		return p;
	}
	void *allocRaw (size_t size = 1, DWORD dwFlags = HEAP_ZERO_MEMORY) noexcept
	{
		CreateScopedLock (m_cs);
		if (!m_hHeap) return nullptr;
		void *p = HeapAlloc (m_hHeap, dwFlags, size);
		if (p) m_allocations.push_back (p);
		return p;
	}
	void free (void *pBlock) noexcept
	{
		CreateScopedLock (m_cs);
		if (pBlock && m_hHeap)
		{
			HeapFree (m_hHeap, 0, pBlock);
			auto it = std::find (m_allocations.begin (), m_allocations.end (), pBlock);
			if (it != m_allocations.end ()) m_allocations.erase (it);
		}
	}
	HANDLE getHeap () const noexcept { return m_hHeap; }
	private:
	HANDLE m_hHeap;
	CriticalSection m_cs;
	std::vector <void *> m_allocations;
	public:
	heapmgr () noexcept : m_hHeap (HeapCreate (0, 0, 0)) {}
	~heapmgr () noexcept
	{
		for (auto &p : m_allocations) HeapFree (m_hHeap, 0, p);
		m_allocations.clear ();
		if (m_hHeap)
		{
			HeapDestroy (m_hHeap);
			m_hHeap = nullptr;
		}
	}
	heapmgr (const heapmgr &rhs) = delete;
	heapmgr &operator = (const heapmgr &rhs) = delete;
	heapmgr (heapmgr &&rhs) noexcept = delete;
	heapmgr &operator = (heapmgr&& rhs) noexcept = delete;
};
