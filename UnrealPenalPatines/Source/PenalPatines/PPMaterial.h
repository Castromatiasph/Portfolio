#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"

namespace PP
{
	/**
	 * Material de color liso con textura opcional y clear coat opcional.
	 * Usa /Game/Materials/M_Color (lo crea Tools/setup_level.py); si no existe, cae al material básico del motor.
	 */
	inline UMaterialInstanceDynamic* ColorMID(UObject* Outer, const FLinearColor& C, float Rough = .5f, const FLinearColor& Emis = FLinearColor::Black,
	                                          UTexture* Tex = nullptr, float Tiling = 1.f, float ClearCoat = 0.f)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Color.M_Color"));
		const bool bCustom = Base != nullptr;
		if (!Base) Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Base) return nullptr;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
		M->SetVectorParameterValue(TEXT("Color"), C);
		if (bCustom)
		{
			M->SetScalarParameterValue(TEXT("Roughness"), Rough);
			M->SetVectorParameterValue(TEXT("Emissive"), Emis);
			M->SetScalarParameterValue(TEXT("Tiling"), Tiling);
			M->SetScalarParameterValue(TEXT("ClearCoat"), ClearCoat);
			if (Tex) M->SetTextureParameterValue(TEXT("Tex"), Tex);
		}
		return M;
	}
}
