#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PPTypes.h"
#include "PPGameMode.generated.h"

class APPBall;
class APPGoalkeeper;
class APPShooter;

/**
 * Orquesta el partido: estados (apuntar, cargar, golpear, vuelo, rebote), física de la bocha,
 * resolución del tiro (gol / atajada / palo / afuera), la tanda contra un rival y la práctica libre.
 * Es el port de las funciones launch(), stepBall(), settle(), finalize(), nextTurn() y tick() del juego web.
 */
UCLASS()
class PENALPATINES_API APPGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	APPGameMode();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	static constexpr float DekeDuration = .34f;
	static constexpr float SwingEndExtra = .3f;

	// ---- configuración (el menú del HUD la cambia) ----
	UPROPERTY(BlueprintReadWrite, Category = "Partido") EPPFormat Format = EPPFormat::Practica;
	UPROPERTY(BlueprintReadWrite, Category = "Partido") EPPShotMode ShotMode = EPPShotMode::Penal;
	UPROPERTY(BlueprintReadWrite, Category = "Partido") EPPDifficulty Difficulty = EPPDifficulty::Primera;
	UPROPERTY(BlueprintReadWrite, Category = "Partido") EPPGKStyle GoalieStyle = EPPGKStyle::Variado;

	// ---- estado ----
	UPROPERTY(BlueprintReadOnly) EPPState State = EPPState::Menu;
	UPROPERTY(BlueprintReadOnly) bool bPaused = false;
	UPROPERTY(BlueprintReadOnly) float Time = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 BestKmh = 0;
	UPROPERTY(BlueprintReadOnly) int32 ReboundCount = 0;

	TArray<EPPOutcome> Results;     // mis tiros
	TArray<bool> Rival;             // tiros del rival (tanda)
	FPPShot Shot; bool bHasShot = false; bool bLaunched = false;
	bool bShotCurve = false;

	// ---- texto en pantalla (lo dibuja APPHUD) ----
	UPROPERTY(BlueprintReadOnly) FString FlashBig;
	UPROPERTY(BlueprintReadOnly) FString FlashSmall;
	UPROPERTY(BlueprintReadOnly) int32 FlashClass = 0;   // 0 neutro, 1 bueno (verde), 2 malo (rojo)
	UPROPERTY(BlueprintReadOnly) FString Hint;

	UPROPERTY() TObjectPtr<APPBall> Ball;
	UPROPERTY() TObjectPtr<APPGoalkeeper> Gk;
	UPROPERTY() TObjectPtr<APPShooter> Shooter;

	/** Evento para audio / efectos: "hit", "net", "post", "save", "goal", "whistle", "bounce", "board", "groan", "cheer". */
	UFUNCTION(BlueprintImplementableEvent, Category = "Partido") void OnPPEvent(FName EventName, float Param, FVector WorldLocation);
	void Fire(const TCHAR* Name, float Param = 0.f, const FPPVec* GamePosition = nullptr);

	// ---- acciones (las llama el pawn / HUD) ----
	UFUNCTION(BlueprintCallable) void StartMatch();
	UFUNCTION(BlueprintCallable) void ToMenu();
	void SetupPenalty();
	void StartCharge();
	void Release(bool bLob, bool bCurve);
	void PushShot();
	void WristShot();
	void BackhandShot();
	void ResetToSpot();
	void TogglePause();

	FPPVec StartPos() const;
	float SpotZ() const { return ShotMode == EPPShotMode::Penal ? PP::SpotZ : PP::FreeZ; }
	int32 GoalsOf(const TArray<EPPOutcome>& A) const;
	int32 GoalsOfRival() const;
	FString ShotName() const;
	float EstimatedError(float P) const;

private:
	float ResultTimer = 0.f, RivalT = 0.f, LooseT = 0.f; bool bRivalDone = false;
	float DekeX = 0.f;

	void ResolveActors();
	void StepAim(float Dt);
	void StepCharge(float Dt);
	void StepSwing(float Dt);
	void StepFlight(float Dt);
	void StepLoose(float Dt);
	void StepRival(float Dt);
	void StepBall(float Dt);
	void Launch();
	void Settle(EPPOutcome Kind, const FString& Detail);
	void Finalize(EPPOutcome Kind, const FString& Detail);
	void NextTurn();
	void EndMatch();
	bool Decided() const;
	void RivalShot();
	void DoSteal();
	bool Stealable() const;
	void SetFlash(const FString& Big, const FString& Small, int32 Cls);
	void ClearFlash() { FlashBig.Empty(); FlashSmall.Empty(); FlashClass = 0; }
	void BallToShooter();
	FString ZoneName(float X, float Y) const;
	void MenuInput();
};
