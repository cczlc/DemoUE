#pragma once
#include "Containers/ContainerAllocationPolicies.h"
/**
 * A dynamically sizeable string.
 * @see https://docs.unrealengine.com/latest/INT/Programming/UnrealArchitecture/StringHandling/FString/
 *
 * When dealing with UTF-8 literals, the following advice is recommended:
 *
 * - Do not use the u8"..." prefix (gives the wrong array type until C++20).
 * - Use UTF8TEXT("...") for array literals (type is const UTF8CHAR[n]).
 * - Use UTF8TEXTVIEW("...") for string view literals (type is FUtf8StringView).
 * - Use \uxxxx or \Uxxxxxxxx escape sequences rather than \x to specify Unicode code points.
 */
class FString
{
public:
	using AllocatorType = TSizedDefaultAllocator<32>;

private:
	/** Array holding the character data */
	typedef TArray<TCHAR, AllocatorType> DataType;
	DataType Data;

	template <typename RangeType>
	using TRangeElementType = std::remove_cv_t<typename TRemovePointer<decltype(GetData(DeclVal<RangeType>()))>::Type>;

	template <typename CharRangeType>
	struct TIsRangeOfCharType : TIsCharType<TRangeElementType<CharRangeType>>
	{
	};
}