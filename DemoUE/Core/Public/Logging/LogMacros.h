#pragma once
#include <atomic>
#include "HAL/Platform.h"
#include "Definitions.h"

namespace UE::Logging::Private
{

	/** Data about a static basic log that is created on-demand. */
	struct FStaticBasicLogDynamicData
	{
		std::atomic<bool> bInitialized = false;
	};

	/** Data about a static basic log that is constant for every occurrence. */
	struct FStaticBasicLogRecord
	{
		const TCHAR* Format = nullptr;
		const ANSICHAR* File = nullptr;
		int32 Line = 0;
		ELogVerbosity::Type Verbosity = ELogVerbosity::Log;
		FStaticBasicLogDynamicData& DynamicData;

		// Workaround for https://developercommunity.visualstudio.com/t/Incorrect-warning-C4700-with-unrelated-s/10285950
		constexpr FStaticBasicLogRecord(
			const TCHAR* InFormat,
			const ANSICHAR* InFile,
			int32 InLine,
			ELogVerbosity::Type InVerbosity,
			FStaticBasicLogDynamicData& InDynamicData)
			: Format(InFormat)
			, File(InFile)
			, Line(InLine)
			, Verbosity(InVerbosity)
			, DynamicData(InDynamicData)
		{
		}
	};

	CORE_API void BasicLog(const FLogCategoryBase& Category, const FStaticBasicLogRecord* Log, ...);
	CORE_API void BasicFatalLog(const FLogCategoryBase& Category, const FStaticBasicLogRecord* Log, ...);

} // UE::Logging::Private


#if UE_VALIDATE_FORMAT_STRINGS
#define UE_VALIDATE_FORMAT_STRING UE_CHECK_FORMAT_STRING
#else
#define UE_VALIDATE_FORMAT_STRING(Format, ...)
#endif

/**
 * A macro that conditionally logs a formatted message if the log category is active at the requested verbosity level.
 *
 * @note The condition is not evaluated unless the log category is active at the requested verbosity level.
 *
 * @param Condition      Condition that must evaluate to true in order for the message to be logged.
 * @param CategoryName   Name of the log category as provided to DEFINE_LOG_CATEGORY.
 * @param Verbosity      Verbosity level of this message. See ELogVerbosity.
 * @param Format         Format string literal in the style of printf.
 */
#define UE_CLOG(Condition, CategoryName, Verbosity, Format, ...) \
		UE_PRIVATE_LOG(if (Condition), constexpr, CategoryName, Verbosity, Format, ##__VA_ARGS__)

 /** Private macro used to implement the public log macros. DO NOT CALL DIRECTLY! */
#define UE_PRIVATE_LOG(Condition, CategoryConst, Category, Verbosity, Format, ...) \
	{ \
		static_assert(std::is_const_v<std::remove_reference_t<decltype(Format)>>, "Formatting string must be a const TCHAR array."); \
		static_assert(TIsArrayOrRefOfTypeByPredicate<decltype(Format), TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array."); \
		UE_VALIDATE_FORMAT_STRING(Format, ##__VA_ARGS__); \
		static ::UE::Logging::Private::FStaticBasicLogDynamicData LOG_Dynamic; \
		static constexpr ::UE::Logging::Private::FStaticBasicLogRecord LOG_Static(Format, __builtin_FILE(), __builtin_LINE(), ::ELogVerbosity::Verbosity, LOG_Dynamic); \
		static_assert((::ELogVerbosity::Verbosity & ::ELogVerbosity::VerbosityMask) < ::ELogVerbosity::NumVerbosity && ::ELogVerbosity::Verbosity > 0, "Verbosity must be constant and in range."); \
		if constexpr ((::ELogVerbosity::Verbosity & ELogVerbosity::VerbosityMask) == ::ELogVerbosity::Fatal) \
		{ \
			Condition \
			{ \
				::UE::Logging::Private::BasicFatalLog(Category, &LOG_Static, ##__VA_ARGS__); \
				CA_ASSUME(false); \
			} \
		} \
		else if constexpr ((::ELogVerbosity::Verbosity & ::ELogVerbosity::VerbosityMask) <= ::ELogVerbosity::COMPILED_IN_MINIMUM_VERBOSITY) \
		{ \
			if CategoryConst ((::ELogVerbosity::Verbosity & ::ELogVerbosity::VerbosityMask) <= Category.GetCompileTimeVerbosity()) \
			{ \
				if (!Category.IsSuppressed(::ELogVerbosity::Verbosity)) \
				{ \
					Condition \
					{ \
						::UE::Logging::Private::BasicLog(Category, &LOG_Static, ##__VA_ARGS__); \
					} \
				} \
			} \
		} \
	}