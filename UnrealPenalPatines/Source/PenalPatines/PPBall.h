#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PPTypes.h"
#include "PPBall.generated.h"

class UStaticMeshComponent;

/** La bocha. Guarda su estado en espacio de juego (metros) y mueve el actor en espacio de Unreal. */
UCLASS()
class PENALPATINES_API APPBall : public AActor
{
	GENERATED_BODY()
public:
	APPBall();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;

	FPPVec Pos = FPPVec(0.f, 0.0365f, 5.4f);  // espacio de juego
	FPPVec Vel = FPPVec::ZeroVector;
	FPPVec Spin = FPPVec::ZeroVector;      // rad/s, eje en espacio de juego

	void SetGamePos(const FPPVec& P) { Pos = P; SyncActor(); }
	void SyncActor();
	/** Gira la bocha según el desplazamiento (rodadura) o según Spin. */
	void RollBy(const FPPVec& Delta);
	void ApplySpin(float Dt);
};
