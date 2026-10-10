#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace PP
{
	/** Material de color liso. Usa /Game/Materials/M_Color (lo crea Tools/setup_level.py); si no existe, cae al material básico del motor. */
	inline UMaterialInstanceDynamic* ColorMID(UObject* Outer, const FLinearColor& C, float Rough = .5f, const FLinearColor& Emis = FLinearColor::Black)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Color.M_Color"));
		const bool bCustom = Base != nullptr;
		if (!Base) Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Base) return nullptr;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
		M->SetVectorParameterValue(TEXT("Color"), C);
		if (bCustom) { M->SetScalarParameterValue(TEXT("Roughness"), Rough); M->SetVectorParameterValue(TEXT("Emissive"), Emis); }
		return M;
	}
}
