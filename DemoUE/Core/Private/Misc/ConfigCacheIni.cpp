#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreGlobals.h"

FConfigCacheIni::FConfigCacheIni(EConfigCacheType InType)
	: bAreFileOperationsDisabled(false), bIsReadyForUse(false), Type(InType)
{}

FConfigCacheIni::FConfigCacheIni()
{
	EnsureRetrievingVTablePtrDuringCtor(TEXT("FConfigCacheIni()"));
}

FConfigCacheIni::~FConfigCacheIni()
{
	// this destructor can run at file scope, static shutdown
	Flush(1);
}
