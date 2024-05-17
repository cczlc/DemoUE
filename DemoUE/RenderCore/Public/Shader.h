#pragma once
#include "RHIShaderPlatform.h"
#include "Misc/EnumClassFlags.h"

// Flags that can specialize shader permutations compiled for specific platforms
enum class EShaderPermutationFlags : uint32
{
	None = 0u,
	HasEditorOnlyData = (1u << 0),
};
ENUM_CLASS_FLAGS(EShaderPermutationFlags);

RENDERCORE_API EShaderPermutationFlags GetShaderPermutationFlags(const FPlatformTypeLayoutParameters& LayoutParams);

struct FShaderPermutationParameters
{
	// Shader platform to compile to.
	const EShaderPlatform Platform;

	// Unique permutation identifier of the material shader type.
	const int32 PermutationId;

	// Flags that describe the permutation
	const EShaderPermutationFlags Flags;

	// Default to include editor-only shaders, to maintain backwards-compatibility
	explicit FShaderPermutationParameters(EShaderPlatform InPlatform, int32 InPermutationId = 0, EShaderPermutationFlags InFlags = EShaderPermutationFlags::HasEditorOnlyData)
		: Platform(InPlatform)
		, PermutationId(InPermutationId)
		, Flags(InFlags)
	{}
};

/** A compiled shader and its parameter bindings. */
class FShader
{
	friend class FShaderType;
	DECLARE_EXPORTED_TYPE_LAYOUT(FShader, RENDERCORE_API, NonVirtual);

public:
	using FPermutationDomain = FShaderPermutationNone;
	using FPermutationParameters = FShaderPermutationParameters;
	using CompiledShaderInitializerType = FShaderCompiledShaderInitializerType;
	using ShaderMetaType = FShaderType;

	/**
	 * Used to construct a shader for deserialization.
	 * This still needs to initialize members to safe values since FShaderType::GenerateSerializationHistory uses this constructor.
	 */
	RENDERCORE_API FShader();

	/**
	 * Construct a shader from shader compiler output.
	 */
	RENDERCORE_API FShader(const CompiledShaderInitializerType& Initializer);

	RENDERCORE_API ~FShader();

	/** Can be overridden by FShader subclasses to modify their compile environment just before compilation occurs. */
	static void ModifyCompilationEnvironment(const FShaderPermutationParameters&, FShaderCompilerEnvironment&) {}

	/** Can be overridden by FShader subclasses to determine whether a specific permutation should be compiled. */
	static bool ShouldCompilePermutation(const FShaderPermutationParameters&) { return true; }

	/** Can be overridden by FShader subclasses to determine whether compilation is valid. */
	static bool ValidateCompiledResult(EShaderPlatform InPlatform, const FShaderParameterMap& InParameterMap, TArray<FString>& OutError) { return true; }

}