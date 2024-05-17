#include "RenderCore/Shader.h"


EShaderPermutationFlags GetShaderPermutationFlags(const FPlatformTypeLayoutParameters& LayoutParams)
{
	EShaderPermutationFlags Result = EShaderPermutationFlags::None;

	static bool bProjectSupportsCookedEditor = []()
		{
			bool bSupportCookedEditorConfigValue = false;
			return GConfig->GetBool(TEXT("CookedEditorSettings"), TEXT("bSupportCookedEditor"), bSupportCookedEditorConfigValue, GGameIni) && bSupportCookedEditorConfigValue;
		}();

		if (bProjectSupportsCookedEditor || LayoutParams.WithEditorOnly())
		{
			Result |= EShaderPermutationFlags::HasEditorOnlyData;
		}
		return Result;
}