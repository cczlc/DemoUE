#pragma once
#include "Definitions.h"
#include "Windows/WindowsPlatform.h"

/*-----------------------------------------------------------------------------
	FMemory.
-----------------------------------------------------------------------------*/

struct FMemory
{
	/** Some allocators can be given hints to treat allocations differently depending on how the memory is used, it's lifetime etc. */
	enum AllocationHints
	{
		None = -1,
		Default,
		Temporary,
		SmallPool,

		Max
	};


	/** @name Memory functions (wrapper for FPlatformMemory) */

	static FORCEINLINE void* Memmove(void* Dest, const void* Src, SIZE_T Count)
	{
		return FPlatformMemory::Memmove(Dest, Src, Count);
	}

	static FORCEINLINE int32 Memcmp(const void* Buf1, const void* Buf2, SIZE_T Count)
	{
		return FPlatformMemory::Memcmp(Buf1, Buf2, Count);
	}

	static FORCEINLINE void* Memset(void* Dest, uint8 Char, SIZE_T Count)
	{
		return FPlatformMemory::Memset(Dest, Char, Count);
	}

	template< class T >
	static FORCEINLINE void Memset(T& Src, uint8 ValueToSet)
	{
		static_assert(!TIsPointer<T>::Value, "For pointers use the three parameters function");
		Memset(&Src, ValueToSet, sizeof(T));
	}

	static FORCEINLINE void* Memzero(void* Dest, SIZE_T Count)
	{
		return FPlatformMemory::Memzero(Dest, Count);
	}

	/** Returns true if memory is zeroes, false otherwise. */
	static FORCEINLINE bool MemIsZero(const void* Ptr, SIZE_T Count)
	{
		// first pass implementation
		uint8* Start = (uint8*)Ptr;
		uint8* End = Start + Count;
		while (Start < End)
		{
			if ((*Start++) != 0)
			{
				return false;
			}
		}

		return true;
	}

	template< class T >
	static FORCEINLINE void Memzero(T& Src)
	{
		static_assert(!TIsPointer<T>::Value, "For pointers use the two parameters function");
		Memzero(&Src, sizeof(T));
	}

	static FORCEINLINE void* Memcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return FPlatformMemory::Memcpy(Dest, Src, Count);
	}

	template< class T >
	static FORCEINLINE void Memcpy(T& Dest, const T& Src)
	{
		static_assert(!TIsPointer<T>::Value, "For pointers use the three parameters function");
		Memcpy(&Dest, &Src, sizeof(T));
	}

	static FORCEINLINE void* BigBlockMemcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return FPlatformMemory::BigBlockMemcpy(Dest, Src, Count);
	}

	static FORCEINLINE void* StreamingMemcpy(void* Dest, const void* Src, SIZE_T Count)
	{
		return FPlatformMemory::StreamingMemcpy(Dest, Src, Count);
	}

	static FORCEINLINE void* ParallelMemcpy(void* Dest, const void* Src, SIZE_T Count, EMemcpyCachePolicy Policy = EMemcpyCachePolicy::StoreCached)
	{
		return FPlatformMemory::ParallelMemcpy(Dest, Src, Count, Policy);
	}

	static FORCEINLINE void Memswap(void* Ptr1, void* Ptr2, SIZE_T Size)
	{
		FPlatformMemory::Memswap(Ptr1, Ptr2, Size);
	}

	//
	// C style memory allocation stubs that fall back to C runtime
	//
	static FORCEINLINE void* SystemMalloc(SIZE_T Size)
	{
		/* TODO: Trace! */
		return ::malloc(Size);
	}

	static FORCEINLINE void SystemFree(void* Ptr)
	{
		/* TODO: Trace! */
		::free(Ptr);
	}

	//
	// C style memory allocation stubs.
	//

	static CORE_API void* Malloc(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
	static CORE_API void* Realloc(void* Original, SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
	static CORE_API void Free(void* Original);
	static CORE_API SIZE_T GetAllocSize(void* Original);

	static FORCEINLINE_DEBUGGABLE void* MallocZeroed(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT)
	{
		void* Memory = Malloc(Count, Alignment);
		Memzero(Memory, Count);
		return Memory;
	}

	/**
	* For some allocators this will return the actual size that should be requested to eliminate
	* internal fragmentation. The return value will always be >= Count. This can be used to grow
	* and shrink containers to optimal sizes.
	* This call is always fast and threadsafe with no locking.
	*/
	static CORE_API SIZE_T QuantizeSize(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);

	/**
	* Releases as much memory as possible. Must be called from the main thread.
	*/
	static CORE_API void Trim(bool bTrimThreadCaches = true);

	/**
	* Set up TLS caches on the current thread. These are the threads that we can trim.
	*/
	static CORE_API void SetupTLSCachesOnCurrentThread();

	/**
	* Clears the TLS caches on the current thread and disables any future caching.
	*/
	static CORE_API void ClearAndDisableTLSCachesOnCurrentThread();

	/**
	 * A helper function that will perform a series of random heap allocations to test
	 * the internal validity of the heap. Note, this function will "leak" memory, but another call
	 * will clean up previously allocated blocks before returning. This will help to A/B testing
	 * where you call it in a good state, do something to corrupt memory, then call this again
	 * and hopefully freeing some pointers will trigger a crash.
	 */
	static CORE_API void TestMemory();
	/**
	* Called once main is started and we have -purgatorymallocproxy.
	* This uses the purgatory malloc proxy to check if things are writing to stale pointers.
	*/
	static CORE_API void EnablePurgatoryTests();
	/**
	* Called once main is started and we have -purgatorymallocproxy.
	* This uses the purgatory malloc proxy to check if things are writing to stale pointers.
	*/
	static CORE_API void EnablePoisonTests();
	/**
	* Set global allocator instead of creating it lazily on first allocation.
	* Must only be called once and only if lazy init is disabled via a macro.
	*/
	static CORE_API void ExplicitInit(FMalloc& Allocator);

	/**
	* Functions to handle special memory given to the title from the platform
	* This memory is allocated like a stack, it's never really freed
	*/
	static CORE_API void RegisterPersistentAuxiliary(void* InMemory, SIZE_T InSize);
	static CORE_API void* MallocPersistentAuxiliary(SIZE_T InSize, uint32 InAlignment = 0);
	static CORE_API void FreePersistentAuxiliary(void* InPtr);
	static CORE_API bool IsPersistentAuxiliaryActive();
	static CORE_API void DisablePersistentAuxiliary();
	static CORE_API void EnablePersistentAuxiliary();
	static CORE_API SIZE_T GetUsedPersistentAuxiliary();
private:
	static CORE_API void GCreateMalloc();
	// These versions are called either at startup or in the event of a crash
	static CORE_API void* MallocExternal(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
	static CORE_API void* ReallocExternal(void* Original, SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
	static CORE_API void FreeExternal(void* Original);
	static CORE_API SIZE_T GetAllocSizeExternal(void* Original);
	static CORE_API SIZE_T QuantizeSizeExternal(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
};