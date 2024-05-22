#pragma once

/**
 * Macro for marking up deprecated code, functions and types.
 *
 * This should be used as syntactic replacement for the [[deprecated]] attribute
 * which provides a UE version number like the old DEPRECATED macro.
 *
 * Features that are marked as deprecated are scheduled to be removed from the code base
 * in a future release. If you are using a deprecated feature in your code, you should
 * replace it before upgrading to the next release. See the Upgrade Notes in the release
 * notes for the release in which the feature was marked deprecated.
 *
 * Sample usage (note the slightly different syntax for classes and structures):
 *
 *		UE_DEPRECATED(5.xx, "Message")
 *		void MyFunction();
 *
 *		UE_DEPRECATED(5.xx, "Message")
 *		typedef FThing MyType;
 *
 *		using MyAlias UE_DEPRECATED(5.xx, "Message") = FThing;
 *
 *		UE_DEPRECATED(5.xx, "Message")
 *		int32 MyVariable;
 *
 *		namespace UE_DEPRECATED(5.xx, "Message") MyNamespace
 *		{
 *		}
 *
 *		Unfortunately, clang will complain that [the] "declaration of [an] anonymous class must
 *		be a definition" for API types.  To work around this, first forward declare the type as
 *		deprecated, then declare the type with the visibility macro.  Note that macros like
 *		USTRUCT must immediately precede the the type declaration, not the forward declaration.
 *
 *		struct UE_DEPRECATED(5.xx, "Message") FMyStruct;
 *		USTRUCT()
 *		struct MODULE_API FMyStruct
 *		{
 *		};
 *
 *		class UE_DEPRECATED(5.xx, "Message") FMyClass;
 *		class MODULE_API FMyClass
 *		{
 *		};
 *
 *		enum class UE_DEPRECATED(5.xx, "Message") EMyEnumeration
 *		{
 *			Zero = 0,
 *			One UE_DEPRECATED(5.xx, "Message") = 1,
 *			Two = 2
 *		};
 *
 *		Unfortunately, VC++ will complain about using member functions and fields from deprecated
 *		class/structs even for class/struct implementation e.g.:
 *		class UE_DEPRECATED(5.xx, "") DeprecatedClass
 *		{
 *		public:
 *			DeprecatedClass() {}
 *
 *			float GetMyFloat()
 *			{
 *				return MyFloat; // This line will cause warning that deprecated field is used.
 *			}
 *		private:
 *			float MyFloat;
 *		};
 *
 *		To get rid of this warning, place all code not called in class implementation in non-deprecated
 *		base class and deprecate only derived one. This may force you to change some access specifiers
 *		from private to protected, e.g.:
 *
 *		class DeprecatedClass_Base_DEPRECATED
 *		{
 *		protected: // MyFloat is protected now, so DeprecatedClass has access to it.
 *			float MyFloat;
 *		};
 *
 *		class UE_DEPRECATED(5.xx, "") DeprecatedClass : DeprecatedClass_Base_DEPRECATED
 *		{
 *		public:
 *			DeprecatedClass() {}
 *
 *			float GetMyFloat()
 *			{
 *				return MyFloat;
 *			}
 *		};
 *
 *		template <typename T>
 *		class UE_DEPRECATED(5.xx, "") DeprecatedClassTemplate
 *		{
 *		};
 *
 *		template <typename T>
 *		UE_DEPRECATED(5.xx, "")
 *		void DeprecatedFunctionTemplate()
 *		{
 *		}
 *
 * @param VERSION The release number in which the feature was marked deprecated.
 * @param MESSAGE A message containing upgrade notes.
 */

#if defined (__INTELLISENSE__)
#define UE_DEPRECATED(Version, Message)
#else
#define UE_DEPRECATED(Version, Message) [[deprecated(Message " Please update your code to the new API before upgrading to the next release, otherwise your project will no longer compile.")]]
#endif