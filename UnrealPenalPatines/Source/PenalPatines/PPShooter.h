#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PPTypes.h"
#include "PPRig.h"
#include "PPPoseMesh.h"
#include "PPShooter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UPPPoseMesh;
class APPGameMode;

/** Pose del tiro en el espacio del torso (REST/BACK/FOLLOW del juego web). */
struct FPPPose
{
	FPPVec H = FPPVec(-.18f, .12f, -.30f);  // mano de arriba
	float Th = 0.f;                          // ángulo del palo (solo para el AnimBP)
	FPPVec D = FPPVec(-.34f, -.84f, -.42f);  // dirección del palo (de la mano hacia la pala), espacio del torso
	float Tw = 0.f;                          // giro de hombros
	float Ln = -.8f;                         // inclinación
	static FPPPose Lerp(const FPPPose& A, const FPPPose& B, float K)
	{
		FPPPose R; R.H = FMath::Lerp(A.H, B.H, K); R.Th = FMath::Lerp(A.Th, B.Th, K); R.Tw = FMath::Lerp(A.Tw, B.Tw, K); R.Ln = FMath::Lerp(A.Ln, B.Ln, K); R.D = FMath::Lerp(A.D, B.D, K); return R;
	}
};

/**
 * El jugador que patea: patinaje con inercia (port de skateStep()), mira en el plano del arco,
 * carga de potencia tirando del mouse hacia atrás, palo con las poses del tiro, y la cámara.
 * El estado del partido lo lleva APPGameMode.
 *
 * Para el look final asigná un SkeletalMesh (MetaHuman o el SK_Matias importado) en `BodyMesh` y un AnimBP
 * que lea Speed, StridePhase, Amp, Brake, Lean, TwistAlpha y StickPitch (todas BlueprintReadOnly).
 */
UCLASS()
class PENALPATINES_API APPShooter : public APawn
{
	GENERATED_BODY()
public:
	APPShooter();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;

	// ---- componentes ----
	UPROPERTY() TArray<TObjectPtr<UPoseableMeshComponent>> ExtraParts;                // pelo, ojos, cejas... (el importador los separa en mallas distintas)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UPPPoseMesh> BodyMesh;   // SK_Matias, posado por código (PPRig)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> StickMesh;
	// cuerpo provisorio armado con formas básicas (se oculta cuando se asigna un SkeletalMesh en BodyMesh)
	UPROPERTY() TObjectPtr<UStaticMeshComponent> TorsoC;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> HeadC;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> LegC;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SkateC;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ArmC;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCameraComponent> Cam;

	// ---- estado para el AnimBP ----
	UPROPERTY(BlueprintReadOnly) float Speed = 0.f;
	UPROPERTY(BlueprintReadOnly) float StridePhase = 0.f;
	UPROPERTY(BlueprintReadOnly) float Amp = 0.f;
	UPROPERTY(BlueprintReadOnly) float Brake = 0.f;
	UPROPERTY(BlueprintReadOnly) float LeanRoll = 0.f;
	UPROPERTY(BlueprintReadOnly) float TwistAlpha = 0.f;
	UPROPERTY(BlueprintReadOnly) float StickPitch = 0.f;

	// ---- espacio de juego ----
	FPPVec GamePos = FPPVec(.45f, 0.f, PP::SpotZ + .8f);
	FPPVec Pvel = FPPVec::ZeroVector;
	float Heading = 0.f, HeadV = 0.f;
	FPPVec Aim = FPPVec(.5f, .78f, 0.f);    // x, y sobre el plano del arco
	float Power = 0.f, PowerVis = 0.f, ChargeT = 0.f;
	float Sweep = 0.f, SweepT = 0.f;        // barrido lateral del palo
	float DekeT = 0.f; int32 DekeDir = 0;
	float LastDekeTime = -9.f;
	FPPPose CurPose, ContactPose, SwingStart;
	float SwingT = 0.f, Contact = .12f, SwingEnd = .42f;
	EPPShotKind ShotKind = EPPShotKind::Golpe;
	FPPVec BallLocal = FPPVec(-.62f, PP::BallR, -.42f);   // la bocha va al costado izquierdo, adelante
	FPPVec BallLocalPress;
	float DribA = 0.f, DribP = 0.f, DribVal = 0.f;   // vaivén de la bocha al conducir
	float LastPin = 0.f, ReplayPin = 0.f;            // cuánto está la pala "pegada" a la bocha (0..1)
	FPPRigTargets SmoothT; bool bSmoothInit = false;  // objetivos del rig suavizados

	// entradas
	FPPVec2 MoveIn = FPPVec2::ZeroVector;
	bool bPullHeld = false; float PullStartY = 0.f; bool bMouseAim = true;
	bool bLobHeld = false, bCurveHeld = false;
	EPPCamera CamMode = EPPCamera::Jugador;
	bool bCamInit = false;
	float Shake = 0.f;

	// ---- API para el GameMode ----
	void Place(const FPPVec& P);                                  // coloca y frena al jugador
	FPPVec BallWorld() const;                                     // dónde está la bocha cuando la lleva
	FPPVec BallAtPress() const;                                   // dónde queda mientras carga
	void SkateStep(float Dt, EPPState State, bool bCanMove, const FPPVec2* AiMove = nullptr);
	void SetPose(const FPPPose& P);
	FPPPose SwingPose(float T, float PowerForFollow) const;
	void BeginSwing(EPPShotKind Kind, float C);
	void CamUpdate(float Dt, EPPState State, const FPPVec& BallPos, float GkX = 0.f);
	void CamReplay(float Dt, const FPPVec& BallPos);
	void CycleCamera();
	void Seg(UStaticMeshComponent* C, const FPPVec& A, const FPPVec& B, float DiaM);
	void UpdateAimFromMouse();
	void UpdatePullFromMouse();
	void ApplyVisual(float Dt, EPPState State);

	static FPPPose Rest() { return FPPPose(); }
	static FPPPose Back() { FPPPose P; P.H = FPPVec(-.1f, .38f, -.26f); P.D = FPPVec(-.68f, .28f, .68f); P.Th = -2.2f; P.Tw = -.5f; P.Ln = -.52f; return P; }
	static FPPPose Follow() { FPPPose P; P.H = FPPVec(.04f, .4f, -.5f); P.D = FPPVec(-.25f, .62f, -.74f); P.Th = 1.5f; P.Tw = -.6f; P.Ln = -.48f; return P; }
	static FPPPose FollowLow() { FPPPose P; P.H = FPPVec(.02f, .18f, -.55f); P.D = FPPVec(-.35f, -.35f, -.87f); P.Th = .75f; P.Tw = -.35f; P.Ln = -.72f; return P; }
	static FPPPose Backhand() { FPPPose P; P.H = FPPVec(.16f, .2f, -.42f); P.D = FPPVec(.35f, -.45f, -.82f); P.Th = .85f; P.Tw = .6f; P.Ln = -.7f; return P; }

private:
	APPGameMode* Match() const;
	// acciones de input
	void InMoveF(float V) { MoveIn.Y = V; }
	void InMoveR(float V) { MoveIn.X = V; }
	void InAimX(float V);
	void InAimY(float V);
	void OnShootPressed();
	void OnShootReleased();
	void OnPush();
	void OnWrist();
	void OnBack();
	void OnDekeL();
	void OnDekeR();
	void OnReset();
	void OnCam();
	void OnLobPressed() { bLobHeld = true; }
	void OnLobReleased() { bLobHeld = false; }
	void OnCurvePressed() { bCurveHeld = true; }
	void OnCurveReleased() { bCurveHeld = false; }
	void OnPause();

	float Braking = 0.f, BrakeK = 0.f, StartK = 0.f, HipSway = 0.f;
	FPPRig Rig; bool bRig = false;
	/** Diagnóstico: dibuja esferas en cabeza, manos y pies del esqueleto y muestra datos del modelo en pantalla. */
	UPROPERTY(EditAnywhere, Category = "Debug") bool bDebugRig = false;
	float DbgT = 0.f;
	float AnkleSeatY = .168f;   // altura del tobillo dentro del botín (55% del alto del patín)
	bool bRealSkate = false; float SkateScale = 1.f, SkateYaw = 0.f; FVector SkateToeLocal = FVector::XAxisVector;   // patín real (SM_PatinReal)
	bool bRealStick = false; float StickScale = 1.f; FVector StickTipLocal = -FVector::ZAxisVector, StickToeLocal = FVector::XAxisVector;   // palo real (SM_Palo)
	FPPVec CamLookG = FPPVec(0.f, .62f, .4f);
	FPPVec CamPosG = FPPVec::ZeroVector;
	FPPVec2 AimStick = FPPVec2::ZeroVector;
	float CamFov = 50.f;
};
