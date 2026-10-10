#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PPTypes.h"
#include "PPArena.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class URectLightComponent;
class UPointLightComponent;
class UPostProcessComponent;

/**
 * Pista de hockey sobre patines (mitad de juego de 20 × 20 m), arco reglamentario con red, vallas con esquinas de 3 m,
 * publicidad, líneas, gradas con hinchada, techo con luminarias y un PostProcess con calidad ajustable (Baja / Media / Alta).
 *
 * Todo se arma con formas básicas e instancias (pocas llamadas de dibujo) para que ande fluido. Los materiales con textura
 * (parquet, publicidad) salen de /Game/Materials/M_Color, que crea Tools/setup_level.py.
 * Si no hay un APPArena en el mapa, APPGameMode::BeginPlay lo crea.
 */
UCLASS()
class PENALPATINES_API APPArena : public AActor
{
	GENERATED_BODY()
public:
	APPArena();
	virtual void OnConstruction(const FTransform& Xf) override;
	virtual void Tick(float Dt) override;

	UPROPERTY(EditAnywhere, Category = "Look") TObjectPtr<UMaterialInterface> FloorMaterial;
	UPROPERTY(EditAnywhere, Category = "Look") TObjectPtr<UMaterialInterface> BoardMaterial;
	UPROPERTY(EditAnywhere, Category = "Look") TObjectPtr<UMaterialInterface> LineMaterial;
	UPROPERTY(EditAnywhere, Category = "Look") TObjectPtr<UMaterialInterface> StandMaterial;
	UPROPERTY(EditAnywhere, Category = "Look") TObjectPtr<UStaticMesh> GoalMesh;      // opcional: arco importado (reemplaza al procedural)
	UPROPERTY(EditAnywhere, Category = "Look") float GoalMeshYaw = 180.f;
	UPROPERTY(EditAnywhere, Category = "Look") float LightLumens = 7500.f;
	UPROPERTY(EditAnywhere, Category = "Look") bool bSpawnPostProcess = true;
	UPROPERTY(EditAnywhere, Category = "Look") int32 CrowdDensity = 1;                 // 0 sin hinchada, 1 normal

	/** Baja: sin GI, resolución 60 %. Media: Lumen GI, 80 %. Alta: Lumen GI + reflejos, 100 %. */
	void ApplyQuality(EPPQuality Q);
	/** La hinchada salta durante un rato. */
	void Cheer(float Seconds = 2.6f);
	/** Papelitos de la tribuna. */
	void Confetti();

private:
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Built;
	UPROPERTY() TArray<TObjectPtr<URectLightComponent>> Lights;
	UPROPERTY() TObjectPtr<UPointLightComponent> Fill;
	UPROPERTY() TObjectPtr<UPostProcessComponent> Pp;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> NetISM;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> CrowdTorso;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> CrowdHead;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> ConfISM;
	UPROPERTY() TMap<uint32, TObjectPtr<UMaterialInstanceDynamic>> MatCache;

	struct FFan { FVector Base; float Phase; int32 Group; int32 Idx; };
	TArray<FFan> Fans;
	struct FConf { FVector P, V, W; FRotator R; int32 Group; int32 Idx; bool bOn; };
	TArray<FConf> Conf;
	float CheerT = 0.f, CheerAcc = 0.f, ConfT = 0.f;
	EPPQuality CurQ = EPPQuality::Media;

	void Build();
	UStaticMeshComponent* Box(const FVector& GamePos, const FVector& SizeM, float YawDeg, UMaterialInterface* Mat, const FLinearColor& Col, UStaticMesh* Mesh = nullptr);
	UStaticMeshComponent* Cyl(const FVector& GamePosA, const FVector& GamePosB, float RadiusM, const FLinearColor& Col);
	void NetLine(const FVector& A, const FVector& B);
	void BuildNet();
	void BuildCrowd();
	UMaterialInterface* Tint(UMaterialInterface* Override, const FLinearColor& Col);
	template <class T> T* Mk();
	UInstancedStaticMeshComponent* MkISM(UStaticMesh* Mesh, UMaterialInterface* Mat, bool bShadow);
	void PlaceFans(float T, float Jump);
};
