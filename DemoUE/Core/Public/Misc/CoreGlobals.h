#pragma once
#include "Definitions.h"
#include "CoreTypes.h"

/**
 * Ensures that current thread is during retrieval of vtable ptr of some
 * UClass.
 *
 * @param CtorSignature The signature of the ctor currently running to
 *		construct proper error message.
 */
CORE_API void EnsureRetrievingVTablePtrDuringCtor(const TCHAR* CtorSignature);