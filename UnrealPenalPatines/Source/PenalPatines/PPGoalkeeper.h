#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PPTypes.h"
#include "PPGoalkeeper.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class APPBall;
class APPShooter;

/**
 * Arquero de la máquina. Toda la lógica (lectura del tiro, desplazamiento, salida a achicar,
 * estocada con el palo, zambullida) es un port directo de updateGoalie()/goalieBlocks() del juego web.
 *
 * Visualmente arranca con piezas básicas (torso, careta, polainas, guante, blocker) para que el juego
 * sea jugable al instante. Para el look final, asigná un SkeletalMesh en `BodyMesh` y un AnimBP que lea
 * las variables expuestas (Kneel, Play, Lean, PadExt, GloveLocal, BlockerLocal, Poke).
 */
UCLASS()
class PENALPATINES_API APPGoalkeeper : public AActor
{
	GENERATED_BODY()
public:
	APPGoalkeeper();
	virtual void BeginPlay() override;

	// ---- estado (espacio de juego, metros) ----
	FPPVec2 Pos = FPPVec2(0.f, 0.42f);  // x, z
	FPPVec2 Vel = FPPVec2::ZeroVector;
	float Roll = 0.f;        // inclinación lateral (G.rotation.z)
	float Yaw = 0.f;

	UPROPERTY(BlueprintReadOnly) float Kneel = 0.f;    // 0 de pie/agachado, 1 de rodillas (reacción al tiro)
	UPROPERTY(BlueprintReadOnly) float Play = 0.f;     // 0 antes de que la bocha esté en juego, 1 en juego
	UPROPERTY(BlueprintReadOnly) float Lean = 0.f;
	UPROPERTY(BlueprintReadOnly) float PokeAlpha = 0.f;
	FPPVec2 PadExt = FPPVec2::ZeroVector;
	FPPVec GloveLocal = FPPVec(0.5f, 0.85f, 0.3f);     // espacio de juego, local al arquero
	FPPVec BlockerLocal = FPPVec(-0.42f, 0.2f, 0.46f);

	float AccX = 0.f, BallVx = 0.f, LastBx = 0.f;
	bool bInPlayLatched = false;
	float PokeT = 0.f, PokeCd = 1.2f;
	float EarlyT = 0.f; int32 EarlySide = 0; bool bEarlyUsed = false;
	FPPVec ReachH0Glove = FPPVec::ZeroVector, ReachH0Block = FPPVec::ZeroVector; bool bHasH0 = false;

	EPPGKStyle StyleSel = EPPGKStyle::Variado;
	EPPGKStyle Style = EPPGKStyle::Atrevido;
	FPPDiff Diff;

	UPROPERTY(EditAnywhere, Category = "Visual") TObjectPtr<USkeletalMeshComponent> BodyMesh;

	// ---- arquero manejado por una persona ----
	bool bUser = false;
	FPPGkInput In;
	bool bOuting = false;
	float UserExt = 0.f;
	void UserUpdate(float Dt, float Time, EPPState State, APPBall* Ball, bool bDuel);
	void TriggerPoke() { if (PokeAlpha <= 0.f) { PokeAlpha = .01f; PokeT = 0.f; } }

	void ResetForShot(bool bPickStyle);
	void PickStyle();

	/** Un paso de IA. Ball/Shooter pueden ser nulos. Devuelve true si la estocada tocó la bocha (debe robarla). */
	void Update(float Dt, float Time, FPPShot* Shot, bool bShotLaunched, EPPState State,
	            APPBall* Ball, APPShooter* Shooter, FPPVec StartPos);

	/** Qué parte del arquero detiene la bocha en B (espacio de juego); "" si pasa. */
	FString BlocksBall(const FPPVec& B) const;
	float PlaneZ() const { return Pos.Y + 0.14f; }
	/** ¿La pala del palo toca la bocha (que va por el piso)? */
	bool StickHits(const FPPVec& BallPos) const;
	FPPVec StickTip() const;
	FPPVec GlovePos() const;

	void Deflect(const FString& Part, const FPPVec& P, FPPVec& OutVel, FPPShot& Shot) const;

	/** Aplica la pose a los componentes visuales. */
	void ApplyVisual();

private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Torso;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Pads;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Glove;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Blocker;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Stick;

	struct FPose { float Up[5]; float Pads[2][6]; float Knee[2][3]; float Hip[2][3]; FPPVec Glove; FPPVec Block; };
	static FPose PoseAt(float Kneel, float Play);
	void Move(FPPVec2 Target, float VMax, float Dt, float Acc);
	void Stance(float K);
	void IdleArms(float K, float Dt);
};
