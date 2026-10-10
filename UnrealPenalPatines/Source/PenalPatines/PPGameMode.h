#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PPTypes.h"
#include "PPShooter.h"
#include "PPGameMode.generated.h"

class APPBall;
class APPGoalkeeper;
class APPArena;
class USoundBase;

/** Un cuadro de la jugada grabada, para la repetición en cámara lenta. */
struct FPPFrame
{
	float T = 0.f;
	FPPVec SPos = FPPVec::ZeroVector; float SHead = 0.f, SAmp = 0.f, SPhase = 0.f, SLean = 0.f, SBrake = 0.f, SSweep = 0.f; FPPPose Pose;
	FPPVec BallPos = FPPVec::ZeroVector; FQuat BallQ = FQuat::Identity; bool bHide = false;
	FPPVec2 GPos = FPPVec2::ZeroVector; float GRoll = 0.f, GYaw = 0.f, GKneel = 0.f, GPlay = 0.f;
	FPPVec GGlove = FPPVec::ZeroVector, GBlock = FPPVec::ZeroVector; FPPVec2 GPad = FPPVec2::ZeroVector;
};

/** Plan de tiro del rival de la máquina (modo "Atajá vos"). */
struct FPPAiPlan
{
	float Wait = 1.f, StopZ = 0.f, DekeAt = .5f, Weave = 0.f, Tx = 0.f, Ty = .3f, Power = .7f, HoldT = 0.f, DribT = 0.f, DribDir = 0.f;
	bool bRun = false, bDeke = false, bDekeDone = false, bCurve = false, bFake = false, bFakeDone = false, bPunished = false, bLobChecked = false, bDribble = false;
	EPPShotKind Kind = EPPShotKind::Golpe;
};

/**
 * Orquesta el partido: estados (apuntar, cargar, golpear, vuelo, rebote, repetición), física de la bocha,
 * resolución del tiro, tanda, práctica, "Atajá vos", desafíos, 2 jugadores, calidad gráfica y sonido.
 * Port de launch(), stepBall(), settle(), finalize(), nextTurn(), stepAI() y tick() del juego web.
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
	UPROPERTY(BlueprintReadWrite, Category = "Partido") EPPQuality Quality = EPPQuality::Baja;

	// ---- estado ----
	UPROPERTY(BlueprintReadOnly) EPPState State = EPPState::Menu;
	UPROPERTY(BlueprintReadOnly) bool bPaused = false;
	UPROPERTY(BlueprintReadOnly) float Time = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 BestKmh = 0;
	UPROPERTY(BlueprintReadOnly) int32 ReboundCount = 0;

	TArray<EPPOutcome> Results;     // tiros del jugador (en "Atajá vos": los del rival)
	TArray<bool> Rival;             // tiros del rival (tanda)
	TArray<EPPOutcome> DuelA, DuelB; bool bDuelA = true;
	// desafíos
	struct FChallenge { const TCHAR* Name; const TCHAR* Desc; int32 Tries; int32 Streak; };
	static const FChallenge Challenges[7];
	int32 ChI = 0, ChTries = 0, ChStreak = 0, ChTotal = 0; TArray<int32> ChStars;

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
	UPROPERTY() TObjectPtr<APPArena> Arena;
	UPROPERTY() TMap<FName, TObjectPtr<USoundBase>> Sounds;

	/** Evento extra para Blueprints: "hit", "net", "post", "save", "goal", "whistle", "bounce", "board", "groan", "cheer". */
	UFUNCTION(BlueprintImplementableEvent, Category = "Partido") void OnPPEvent(FName EventName, float Param, FVector WorldLocation);
	void Fire(const TCHAR* Name, float Param = 0.f, const FPPVec* GamePosition = nullptr);

	// ---- acciones (las llaman el pawn y el HUD) ----
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
	void CycleQuality();

	bool UserShoots() const { return Format != EPPFormat::Arquero; }
	EPPCamera NextCamera(EPPCamera Cur) const;
	FPPVec StartPos() const;
	float SpotZ() const { return ShotMode == EPPShotMode::Penal ? PP::SpotZ : PP::FreeZ; }
	int32 GoalsOf(const TArray<EPPOutcome>& A) const;
	int32 GoalsOfRival() const;
	FString ShotName() const;
	FString PlayerName(bool bA) const { return bA ? TEXT("Jugador 1") : TEXT("Jugador 2"); }
	float EstimatedError(float P) const;

private:
	float ResultTimer = 0.f, RivalT = 0.f, LooseT = 0.f; bool bRivalDone = false;
	float DekeX = 0.f, FlashT = 0.f, SwapT = 0.f;
	// IA del rival (Atajá vos)
	FPPAiPlan Ai; float AiT = 0.f; FPPVec2 AiMove = FPPVec2::ZeroVector; bool bAiActive = false;
	// repetición
	TArray<FPPFrame> Rec;
	struct FReplay { TArray<FPPFrame> Frames; float T0 = 0.f, T1 = 0.f, Tt = 0.f, Speed = .38f; } Rp;
	EPPCamera CamBeforeReplay = EPPCamera::Jugador;

	void ResolveActors();
	void StepAim(float Dt);
	void StepCharge(float Dt);
	void StepSwing(float Dt);
	void StepFlight(float Dt);
	void StepLoose(float Dt);
	void StepRival(float Dt);
	void StepReplay(float Dt);
	void StepAI(float Dt);
	void StepBall(float Dt);
	void GoalieInput(float Dt);
	void Launch();
	void Settle(EPPOutcome Kind, const FString& Detail);
	void Finalize(EPPOutcome Kind, const FString& Detail);
	void NextTurn();
	void EndMatch();
	bool Decided() const;
	bool DuelDecided() const;
	void RivalShot();
	void DoSteal(bool bByUser);
	bool Stealable() const;
	bool StartReplay();
	void Record();
	FPPAiPlan MakePlan() const;
	bool ChallengeOk(int32 I, float Gx, float Gy, EPPShotKind K, int32 Kmh, bool bDeke) const;
	void SetFlash(const FString& Big, const FString& Small, int32 Cls, float Seconds = -1.f);
	void ClearFlash() { FlashBig.Empty(); FlashSmall.Empty(); FlashClass = 0; FlashT = 0.f; }
	void BallToShooter();
	FString ZoneName(float X, float Y) const;
	void MenuInput();
	void PlaySfx(const FString& Name, float Volume, const FVector& Loc);
};
