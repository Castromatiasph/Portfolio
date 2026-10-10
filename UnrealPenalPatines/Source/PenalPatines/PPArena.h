#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PPArena.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * Pista 40×20 m de hockey sobre patines (aquí la mitad de juego: 20×20 m), arco reglamentario, vallas con esquinas de 3 m,
 * líneas, gradas, techo con luminarias (RectLight), SkyLight y un PostProcessVolume con Lumen en calidad máxima.
 *
 * Todo se arma con formas básicas para que el proyecto arranque sin assets. Para el look final, asigná tus
 * materiales/meshes en las propiedades de abajo (los crea Tools/setup_level.py) y se usan en lugar de los provisorios.
 * Si no hay un APPArena en el mapa, APPGameMode::BeginPlay lo crea.
 */
UCLASS()
class PENALPATINES_API APPArena : public AActor
{
	GENERATED_BODY()
public:
	APPArena();
	virtual void OnConstruction(const FTransform& Xf) override;

	UPROPERTY(EditAnywhere, Category = "Look") TObjectPtr<UMaterialInterface> FloorMaterial;     // parquet barnizado (clear coat)
	UPROPERTY(EditAnywhere, Category = "Look") TObjectPtr<UMaterialInterface> BoardMaterial;
	UPROPERTY(EditAnywhere, Category = "Look") TObjectPtr<UMaterialInterface> LineMaterial;
	UPROPERTY(EditAnywhere, Category = "Look") TObjectPtr<UMaterialInterface> StandMaterial;
	UPROPERTY(EditAnywhere, Category = "Look") TObjectPtr<UStaticMesh> GoalMesh;                 // SM_Arco importado (con red)
	UPROPERTY(EditAnywhere, Category = "Look") float GoalMeshYaw = 180.f;                         // corrige el eje del importador glTF
	UPROPERTY(EditAnywhere, Category = "Look") TObjectPtr<UStaticMesh> SeatMesh;                  // SM_Butaca importado
	UPROPERTY(EditAnywhere, Category = "Look") float LightLumens = 9000.f;
	UPROPERTY(EditAnywhere, Category = "Look") bool bSpawnPostProcess = true;

private:
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Built;
	void Build();
	UStaticMeshComponent* Box(const FVector& GamePos, const FVector& SizeM, float YawDeg, UMaterialInterface* Mat, const FLinearColor& Col, UStaticMesh* Mesh = nullptr);
	UStaticMeshComponent* Cyl(const FVector& GamePosA, const FVector& GamePosB, float RadiusM, const FLinearColor& Col);
	UMaterialInterface* Tint(UMaterialInterface* Override, const FLinearColor& Col);
};
