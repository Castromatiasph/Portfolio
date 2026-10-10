#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Engine.h"

namespace PP
{
	/**
	 * Busca un asset importado por clase y parte del nombre en una carpeta (con subcarpetas).
	 * Unreal a veces importa un archivo en una subcarpeta con otro nombre, así que no se usan rutas fijas.
	 */
	template <class T>
	T* FindAsset(const TCHAR* Folder, const TCHAR* NameContains)
	{
		FAssetRegistryModule& Reg = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> Found;
		Reg.Get().GetAssetsByPath(FName(Folder), Found, true);
		const FTopLevelAssetPath Cls = T::StaticClass()->GetClassPathName();
		for (const FAssetData& D : Found)
			if (D.AssetClassPath == Cls && D.AssetName.ToString().Contains(NameContains)) return Cast<T>(D.GetAsset());
		return nullptr;
	}

	inline void ScreenWarn(const FString& Msg)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PenalPatines] %s"), *Msg);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::Orange, TEXT("[PenalPatines] ") + Msg);
	}
}
