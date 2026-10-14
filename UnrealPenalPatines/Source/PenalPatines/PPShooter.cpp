#include "PPShooter.h"
#include "PPGameMode.h"
#include "PPBall.h"
#include "PPMaterial.h"
#include "PPAssets.h"
#include "DrawDebugHelpers.h"
#include "PPPoseMesh.h"
#include "PPHuman.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

namespace
{
	constexpr float HipY = .76f;
	// el tobillo del jugador queda adentro del botín, a 7,5 cm del talón del patín (la altura sale del alto del patín: AnkleSeatY)
	constexpr float AnkleSeatZ = .075f;
	// palo (SM_Palo, en metros desde el tapón): la caña baja por el eje y la pala (gancho) sale hacia adelante en la punta.
	// Punto de la pala que toca la bocha: 1,20 m caña abajo y 17 cm hacia la punta. La mano de arriba agarra 12 cm bajo el tapón.
	constexpr float StickCapToHand = .12f, ContactAlong = 1.20f, ContactToe = .17f, HandsGap = .28f;
	// contorno de la pala (caña abajo, hacia la punta) para apoyarla justo sobre el piso
	const float BladePts[6][2] = { { 1.23f, .10f }, { 1.23f, .275f }, { 1.20f, .275f }, { 1.15f, .04f }, { 1.10f, .016f }, { 1.15f, .26f } };

	/** Hacia dónde apunta el gancho: hacia donde se inclina el palo (Lean, horizontal), perpendicular a la caña. */
	FPPVec ToeFor(const FPPVec& D, const FPPVec* Lean = nullptr)
	{
		FPPVec H = Lean ? *Lean : FPPVec(D.X, 0.f, D.Z);
		if (!Lean) H = H.SizeSquared() > 2.5e-3f ? H.GetSafeNormal() : FPPVec(0.f, 0.f, -1.f);
		const FPPVec T = H - D * FPPVec::DotProduct(H, D);
		return T.SizeSquared() > 1e-6f ? T.GetSafeNormal() : FPPVec(0.f, 0.f, -1.f);
	}
}

APPShooter::APPShooter()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	BodyMesh = CreateDefaultSubobject<UPPPoseMesh>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Root);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sph(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	auto Make = [&](const TCHAR* Name, UStaticMesh* M)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (M) C->SetStaticMesh(M);
		return C;
	};
	UStaticMesh* CylM = Cyl.Succeeded() ? Cyl.Object : nullptr;
	UStaticMesh* CubeM = Cube.Succeeded() ? Cube.Object : nullptr;
	UStaticMesh* SphM = Sph.Succeeded() ? Sph.Object : nullptr;
	TorsoC = Make(TEXT("TorsoC"), CylM);
	HeadC = Make(TEXT("HeadC"), SphM);
	LegC.Add(Make(TEXT("LegL"), CylM)); LegC.Add(Make(TEXT("LegR"), CylM));
	SkateC.Add(Make(TEXT("SkateL"), CubeM)); SkateC.Add(Make(TEXT("SkateR"), CubeM));
	ArmC.Add(Make(TEXT("ArmL"), CylM)); ArmC.Add(Make(TEXT("ArmR"), CylM));
	StickMesh = Make(TEXT("StickMesh"), CubeM);

	Cam = CreateDefaultSubobject<UCameraComponent>(TEXT("Cam"));
	Cam->SetupAttachment(Root);
	Cam->bUsePawnControlRotation = false;
	Cam->SetFieldOfView(50.f);
	Cam->PostProcessSettings.bOverride_DepthOfFieldFstop = false;
	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

APPGameMode* APPShooter::Match() const { return GetWorld() ? GetWorld()->GetAuthGameMode<APPGameMode>() : nullptr; }

// ---------------------------------------------------------------- entrada
void APPShooter::SetupPlayerInputComponent(UInputComponent* In)
{
	Super::SetupPlayerInputComponent(In);
	In->BindAxis("MoveForward", this, &APPShooter::InMoveF);
	In->BindAxis("MoveRight", this, &APPShooter::InMoveR);
	In->BindAxis("AimX", this, &APPShooter::InAimX);
	In->BindAxis("AimY", this, &APPShooter::InAimY);
	In->BindAction("Shoot", IE_Pressed, this, &APPShooter::OnShootPressed);
	In->BindAction("Shoot", IE_Released, this, &APPShooter::OnShootReleased);
	In->BindAction("Push", IE_Pressed, this, &APPShooter::OnPush);
	In->BindAction("Wrist", IE_Pressed, this, &APPShooter::OnWrist);
	In->BindAction("Backhand", IE_Pressed, this, &APPShooter::OnBack);
	In->BindAction("Lob", IE_Pressed, this, &APPShooter::OnLobPressed);
	In->BindAction("Lob", IE_Released, this, &APPShooter::OnLobReleased);
	In->BindAction("Curve", IE_Pressed, this, &APPShooter::OnCurvePressed);
	In->BindAction("Curve", IE_Released, this, &APPShooter::OnCurveReleased);
	In->BindAction("DekeLeft", IE_Pressed, this, &APPShooter::OnDekeL);
	In->BindAction("DekeRight", IE_Pressed, this, &APPShooter::OnDekeR);
	In->BindAction("Reset", IE_Pressed, this, &APPShooter::OnReset);
	In->BindAction("CycleCamera", IE_Pressed, this, &APPShooter::OnCam);
	In->BindAction("Pause", IE_Pressed, this, &APPShooter::OnPause);
}

void APPShooter::InAimX(float V) { AimStick.X = V; }
void APPShooter::InAimY(float V) { AimStick.Y = V; }

void APPShooter::OnShootPressed()
{
	APPGameMode* M = Match(); if (!M || !M->UserShoots()) return;
	APlayerController* PC = Cast<APlayerController>(GetController());
	float Mx = 0, My = 0;
	if (PC && PC->GetMousePosition(Mx, My) && M->State == EPPState::Aim) { bPullHeld = true; PullStartY = My; }
	M->StartCharge();
}

void APPShooter::OnShootReleased()
{
	APPGameMode* M = Match(); if (!M || !M->UserShoots()) return;
	bPullHeld = false;
	M->Release(bLobHeld, bCurveHeld);
}

void APPShooter::OnPush() { if (APPGameMode* M = Match()) if (M->UserShoots()) M->PushShot(); }
void APPShooter::OnWrist() { if (APPGameMode* M = Match()) if (M->UserShoots()) M->WristShot(); }
void APPShooter::OnBack() { if (APPGameMode* M = Match()) if (M->UserShoots()) M->BackhandShot(); }
void APPShooter::OnReset() { if (APPGameMode* M = Match()) M->ResetToSpot(); }
void APPShooter::OnCam() { CycleCamera(); }
void APPShooter::OnPause() { if (APPGameMode* M = Match()) M->TogglePause(); }

void APPShooter::OnDekeL() { APPGameMode* M = Match(); if (M && M->UserShoots() && M->State == EPPState::Aim && DekeT <= 0.f) { DekeDir = 1; DekeT = APPGameMode::DekeDuration; LastDekeTime = M->Time; } }
void APPShooter::OnDekeR() { APPGameMode* M = Match(); if (M && M->UserShoots() && M->State == EPPState::Aim && DekeT <= 0.f) { DekeDir = -1; DekeT = APPGameMode::DekeDuration; LastDekeTime = M->Time; } }

void APPShooter::CycleCamera()
{
	if (APPGameMode* M = Match()) CamMode = M->NextCamera(CamMode);
	bCamInit = false;
}

void APPShooter::UpdateAimFromMouse()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;
	FVector O, D;
	if (!PC->DeprojectMousePositionToWorld(O, D) || FMath::Abs(D.X) < 1e-4) return;
	const double T = (0.0 - O.X) / D.X;          // plano del arco: X = 0 de Unreal
	if (T <= 0.0) return;
	const FVector Hit = O + D * T;
	Aim.X = FMath::Clamp(float(Hit.Y / 100.0), -1.25f, 1.25f);
	Aim.Y = FMath::Clamp(float(Hit.Z / 100.0), .04f, 1.5f);
}

void APPShooter::UpdatePullFromMouse()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !bPullHeld) return;
	float Mx, My; int32 Vx, Vy;
	if (!PC->GetMousePosition(Mx, My)) return;
	PC->GetViewportSize(Vx, Vy);
	Power = FMath::Clamp((My - PullStartY) / (Vy * .3f), 0.f, 1.f);   // tirar el mouse hacia atrás (abajo) carga la fuerza
}

// ---------------------------------------------------------------- colocación y bocha
void APPShooter::Place(const FPPVec& P)
{
	GamePos = P; Pvel = FPPVec::ZeroVector; Heading = 0.f; HeadV = 0.f; Amp = 0.f; StridePhase = 0.f;
	Sweep = SweepT = 0.f; DekeT = 0.f; DribA = DribVal = 0.f; Braking = BrakeK = 0.f; LeanRoll = 0.f;
	CurPose = Rest(); Power = PowerVis = 0.f; bPullHeld = false;
	ApplyVisual(0.f, EPPState::Aim);
}

FPPVec APPShooter::BallWorld() const
{
	const float A = Sweep + DribVal;
	const float C = FMath::Cos(A), S = FMath::Sin(A);
	const FPPVec L(BallLocal.X * C + BallLocal.Z * S, PP::BallR, -BallLocal.X * S + BallLocal.Z * C);
	const float Ch = FMath::Cos(Heading), Sh = FMath::Sin(Heading);
	// rotación de rumbo alrededor de Y (el rumbo 0 mira hacia -z)
	return FPPVec(GamePos.X + L.X * Ch + L.Z * Sh, PP::BallR, GamePos.Z - L.X * Sh + L.Z * Ch);
}

FPPVec APPShooter::BallAtPress() const
{
	const float Ch = FMath::Cos(Heading), Sh = FMath::Sin(Heading);
	const FPPVec& L = BallLocalPress;
	return FPPVec(GamePos.X + L.X * Ch + L.Z * Sh, PP::BallR, GamePos.Z - L.X * Sh + L.Z * Ch);
}

void APPShooter::SetPose(const FPPPose& P) { CurPose = P; }

FPPPose APPShooter::SwingPose(float T, float PowerForFollow) const
{
	if (T < Contact) { const float K = T / Contact; return FPPPose::Lerp(SwingStart, ContactPose, K * K); }
	FPPPose F;
	switch (ShotKind)
	{
	case EPPShotKind::Arrastre:
	case EPPShotKind::Reves: F = FollowLow(); break;
	case EPPShotKind::Fuetazo: F = FPPPose::Lerp(ContactPose, Follow(), .3f); break;
	default: F = FPPPose::Lerp(ContactPose, Follow(), .35f + .65f * PowerForFollow); break;
	}
	const float U = FMath::Min(1.f, (T - Contact) / APPGameMode::SwingEndExtra);
	const float Back = (ShotKind == EPPShotKind::Golpe || ShotKind == EPPShotKind::Picada) ? 1.25f : .6f;
	return FPPPose::Lerp(ContactPose, F, 1.f + (Back + 1.f) * FMath::Pow(U - 1.f, 3.f) + Back * FMath::Pow(U - 1.f, 2.f));   // acompañamiento con un leve pasarse y volver
}

void APPShooter::BeginSwing(EPPShotKind Kind, float C)
{
	ShotKind = Kind; SwingT = 0.f; Contact = C; SwingEnd = C + APPGameMode::SwingEndExtra;
}

// ---------------------------------------------------------------- patinaje (port de skateStep)
void APPShooter::SkateStep(float Dt, EPPState State, bool bCanMove, const FPPVec2* AiMove)
{
	using namespace PP;
	float Ix = MoveIn.X, Iy = MoveIn.Y;
	if (AiMove) { Ix = AiMove->X; Iy = AiMove->Y; }
	if (!bCanMove) { Ix = 0.f; Iy = 0.f; }
	FPPVec Want = FPPVec(1.f, 0.f, 0.f) * Ix + FPPVec(0.f, 0.f, -1.f) * Iy;   // ejes fijos: arriba = hacia el arco
	const float Mag = FMath::Min(1.f, Want.Size());
	const float Cap = State == EPPState::Charge ? 3.2f : State == EPPState::Swing ? 2.2f : MaxSpeed;
	if (Mag > .05f)
	{
		Want.Normalize();
		const float Sp0 = Pvel.Size();
		if (Sp0 > .2f)
		{
			FPPVec Cur = Pvel / Sp0;
			const float Dot = FPPVec::DotProduct(Cur, Want);
			if (Dot < -.35f) { Pvel += Want * ((Sp0 > 2.5f ? 11.f : 20.f) * Mag * Dt); Braking = FMath::Min(1.f, Braking + Dt * 7.f); }   // frenada en T
			else
			{
				Cur = FMath::Lerp(Cur, Want, PowDt(.00002f, Dt)).GetSafeNormal();
				Pvel = Cur * Sp0;
				Pvel += Want * ((Sp0 < 2.2f ? 13.f : Accel) * Mag * Dt * FMath::Clamp(1.1f - Sp0 / Cap, 0.f, 1.f));
			}
		}
		else Pvel = Want * FMath::Max(Sp0, .55f * Mag);
	}
	float Sp = Pvel.Size();
	const float Drag = State == EPPState::Swing ? 5.f : (Mag > .05f ? .3f : FMath::Max(1.5f, Glide * (.35f + .65f * FMath::Min(1.f, Sp / MaxSpeed)) + .4f));
	Sp = FMath::Max(0.f, Sp - Drag * Dt);
	if (Sp > Cap) Sp = FMath::Lerp(Sp, Cap, PowDt(.002f, Dt));
	if (Pvel.SizeSquared() > 1e-6f) Pvel = Pvel.GetSafeNormal() * Sp;

	GamePos += Pvel * Dt;
	{
		FPPVec N;
		if (InsideRink(GamePos, .32f, -.5f, N)) { const float Vn = FPPVec::DotProduct(Pvel, N); if (Vn < 0.f) Pvel += N * (-Vn * 1.2f); }
	}
	// zona del arquero (semicírculo de palo a palo) y estructura del arco
	{
		const float R = .32f;
		const float D = Hypot(GamePos.X, GamePos.Z);
		if (GamePos.Z >= 0.f && D < PostHalf + R)
		{
			const float K = (PostHalf + R) / FMath::Max(D, 1e-4f);
			GamePos.X *= K; GamePos.Z *= K;
			const FPPVec Nn = FPPVec(GamePos.X, 0.f, GamePos.Z).GetSafeNormal();
			const float Vn = FPPVec::DotProduct(Pvel, Nn); if (Vn < 0.f) Pvel += Nn * (-Vn * 1.2f);
		}
		else if (GamePos.Z < 0.f && GamePos.Z > GoalBack - R && FMath::Abs(GamePos.X) < GoalSide + R)
		{
			const float Ox = GoalSide + R - FMath::Abs(GamePos.X), Oz = GamePos.Z - (GoalBack - R);
			if (Ox < Oz) { GamePos.X = FMath::Sign(GamePos.X) * (GoalSide + R); Pvel.X = FMath::Sign(GamePos.X) * FMath::Max(0.f, FMath::Sign(GamePos.X) * Pvel.X); }
			else { GamePos.Z = GoalBack - R; Pvel.Z = FMath::Min(0.f, Pvel.Z); }
		}
	}
	Speed = Sp;

	// hacia dónde mira: hacia el arco al tirar, si no hacia donde patina
	float Target = Heading;
	if (State == EPPState::Charge || (State == EPPState::Swing && SwingT < .1f)) Target = FMath::Atan2(GamePos.X - .25f - Aim.X, GamePos.Z);
	else if (Sp > .6f && State != EPPState::Swing) Target = FMath::Atan2(-Pvel.X, -Pvel.Z);
	float Dh = FMath::Fmod(FMath::Fmod(Target - Heading + PI, 2.f * PI) + 2.f * PI, 2.f * PI) - PI;
	const float W = State == EPPState::Charge ? 20.f : 16.f;      // resorte críticamente amortiguado
	HeadV += (Dh * W * W - 2.f * W * HeadV) * Dt;
	HeadV = FMath::Clamp(HeadV, -14.f, 14.f);
	Heading += HeadV * Dt;

	Braking = FMath::Max(0.f, Braking - Dt * (Sp < .8f ? 6.f : 2.6f));
	BrakeK = FMath::Lerp(BrakeK, (Braking > .15f && Sp > .9f) ? 1.f : 0.f, PowDt(.0008f, Dt));
	Brake = BrakeK;
	const bool bPushing = Mag > .05f && State != EPPState::Swing && BrakeK < .5f;
	const float AmpT = bPushing ? FMath::Clamp(.45f + Sp / MaxSpeed * .55f, 0.f, 1.f) : 0.f;
	Amp = FMath::Lerp(Amp, AmpT, PowDt(bPushing ? .0008f : .01f, Dt));
	if (Amp > .01f) StridePhase += Dt * (2.7f + Sp * .85f);
	StartK = FMath::Lerp(StartK, (bPushing && Sp < 2.4f) ? 1.f - Sp / 2.4f : 0.f, PowDt(.02f, Dt));
	if (Amp > .01f) StridePhase += Dt * StartK * 1.6f;
	LeanRoll = FMath::Lerp(LeanRoll, FMath::Clamp(-HeadV * Sp * .014f, -.2f, .2f), PowDt(.003f, Dt));
	HipSway = FMath::Lerp(HipSway, -.06f * FMath::Sin(StridePhase - .35f) * Amp, PowDt(.0005f, Dt));
}

// ---------------------------------------------------------------- visual y cámara
void APPShooter::Seg(UStaticMeshComponent* C, const FPPVec& A, const FPPVec& B, float DiaM)
{
	if (!C) return;
	const FVector Ua = PP::ToUE(A), Ub = PP::ToUE(B);
	const FVector D = Ub - Ua;
	const double Len = D.Size();
	if (Len < 1e-3) return;
	C->SetRelativeLocation((Ua + Ub) * .5);
	C->SetRelativeRotation(FRotationMatrix::MakeFromZ(D).Rotator());
	C->SetRelativeScale3D(FVector(DiaM, DiaM, Len / 100.0));
}

void APPShooter::ApplyVisual(float Dt, EPPState State)
{
	(void)Dt; (void)State;
	SetActorLocation(PP::ToUE(GamePos));
	// rumbo 0 mira hacia -z de juego = +X de Unreal; el yaw de Unreal es -rumbo
	SetActorRotation(FRotator(0.f, FMath::RadiansToDegrees(-Heading), FMath::RadiansToDegrees(-LeanRoll)));
	TwistAlpha = CurPose.Tw; StickPitch = CurPose.Th;

	// ---- cuerpo provisorio (espacio del torso: x derecha, y arriba, -z adelante) ----
	const float HipH = HipY - .05f * Amp - .04f * Brake;
	const FPPVec Hip(HipSway, HipH, 0.f);
	const float A = FMath::Clamp(-CurPose.Ln, 0.f, 1.1f);                 // inclinación hacia adelante
	const FPPVec Sh0 = Hip + FPPVec(0.f, .52f * FMath::Cos(A), -.52f * FMath::Sin(A));
	const float Tw = CurPose.Tw, Ctw = FMath::Cos(Tw), Stw = FMath::Sin(Tw);
	const FPPVec ShL = Sh0 + FPPVec(-.2f * Ctw, 0.f, .2f * Stw), ShR = Sh0 + FPPVec(.2f * Ctw, 0.f, -.2f * Stw);
	Seg(TorsoC, Hip, Sh0, .32f);
	if (HeadC) { HeadC->SetRelativeLocation(PP::ToUE(Sh0 + FPPVec(0.f, .2f, -.1f * FMath::Sin(A)))); HeadC->SetRelativeScale3D(FVector(.22)); }
	FPPVec Feet[2]; float Lifts[2] = { 0.f, 0.f };
	for (int i = 0; i < 2; i++)
	{
		const float Sd = i == 0 ? -1.f : 1.f;
		const float Ph = StridePhase + i * PI;
		const float Zf = -FMath::Sin(Ph) * .35f * Amp;                   // adelante = -z
		const float Lift = FMath::Max(0.f, FMath::Cos(Ph)) * .1f * Amp;
		const FPPVec Foot(Sd * .14f + HipSway * .5f + Sd * Brake * .12f, .09f + Lift, Zf);
		Feet[i] = Foot; Lifts[i] = Lift;
		Seg(LegC[i], Hip + FPPVec(Sd * .11f, 0.f, 0.f), Foot + FPPVec(0.f, .03f, 0.f), .16f);
		if (SkateC[i])
		{
			if (bRealSkate)
			{
				// ruedas apoyadas en el piso; la punta baja un poco al levantar el pie; el asiento del tobillo del patín (7,5 cm del talón) queda bajo el del jugador
				const FRotator R(-FMath::RadiansToDegrees(FMath::Clamp(Lift * 3.f, 0.f, .5f)), SkateYaw, 0.f);
				const FVector GroundUE = PP::ToUE(Foot + FPPVec(0.f, -.09f, 0.f));
				SkateC[i]->SetRelativeRotation(R);
				SkateC[i]->SetRelativeLocation(GroundUE - R.RotateVector(SkateToeLocal * (AnkleSeatZ * 100.0 * double(SkateScale))));
				SkateC[i]->SetRelativeScale3D(FVector(double(SkateScale)));
			}
			else
			{
				SkateC[i]->SetRelativeLocation(PP::ToUE(Foot + FPPVec(0.f, -.035f, 0.f)));
				SkateC[i]->SetRelativeScale3D(FVector(.32, .1, .1));          // largo adelante (X), ancho (Y), alto (Z)
			}
		}
	}

	// ---- palo y manos ----
	// La mano de arriba y la dirección salen de la pose; mientras la bocha está en la pala ("Pin"), la pala se apoya en la bocha
	// y la mano se corre sobre su línea para que el palo (de largo fijo) llegue justo hasta ahí.
	FPPVec Hand(CurPose.H.X, HipY + CurPose.H.Y + .08f, CurPose.H.Z);   // +8 cm: el jugador está sobre los patines
	FPPVec D = CurPose.D.GetSafeNormal();
	float Pin = 0.f;
	FPPVec PinLean(0.f, 0.f, -1.f);
	switch (State)
	{
	case EPPState::Aim: Pin = 1.f; break;
	case EPPState::Charge: Pin = 1.f - PP::Smooth(FMath::Clamp(PowerVis, 0.f, 1.f)); break;
	case EPPState::Swing: { const float K = FMath::Clamp(SwingT / FMath::Max(Contact, .01f), 0.f, 1.f); Pin = K * K; break; }
	case EPPState::Flight: Pin = FMath::Clamp(1.f - (SwingT - Contact) / .06f, 0.f, 1.f); break;
	case EPPState::Replay: Pin = ReplayPin; break;
	default: break;
	}
	LastPin = Pin;
	if (Pin > .001f)
	{
		FPPVec BallL = BallLocalPress;
		if (State != EPPState::Flight)
		{
			const APPGameMode* M = Match();
			if (M && M->Ball)
			{
				const float Ch = FMath::Cos(Heading), Sh = FMath::Sin(Heading);
				const float Dx = M->Ball->Pos.X - GamePos.X, Dz = M->Ball->Pos.Z - GamePos.Z;
				BallL = FPPVec(Dx * Ch - Dz * Sh, M->Ball->Pos.Y, Dx * Sh + Dz * Ch);
			}
		}
		// el palo se inclina desde la mano hacia la bocha; el gancho queda en ese plano y la bocha toca la cara de la pala que mira al arco
		const float BR = PP::BallR;
		const FPPVec Lean = FPPVec(BallL.X - Hand.X, 0.f, BallL.Z - Hand.Z).GetSafeNormal();
		FPPVec Face(-Lean.Z, 0.f, Lean.X);
		if (Face.Z > 0.f) Face = -Face;
		const FPPVec ContactP = FPPVec(BallL.X, .045f, BallL.Z) - Face * (BR + .0135f);
		FPPVec Dp = D, Hp = Hand, Tp = ToeFor(D, &Lean);
		for (int It = 0; It < 4; It++)
		{
			Tp = ToeFor(Dp, &Lean);
			const FPPVec Axis = ContactP - Tp * ContactToe;                         // punto de la caña a la altura del contacto
			Dp = (Axis - Hand).GetSafeNormal();
			Hp = Axis - Dp * (ContactAlong - StickCapToHand);
		}
		// apoyar la pala justo sobre el piso
		const FPPVec CapP = Hp - Dp * StickCapToHand;
		float Low = 1e9f;
		for (const auto& Bp : BladePts) Low = FMath::Min(Low, (CapP + Dp * Bp[0] + Tp * Bp[1]).Y);
		Hp.Y -= Low;
		Hand = FMath::Lerp(Hand, Hp, Pin);
		D = FMath::Lerp(D, Dp, Pin).GetSafeNormal();
		PinLean = Lean;
	}
	const FPPVec Cap = Hand - D * StickCapToHand;
	const FPPVec TopGrip = Hand, LowGrip = Hand + D * HandsGap;
	if (bRealStick)
	{
		// orientación: la caña sigue a D y el gancho apunta hacia donde se inclina el palo
		const FPPVec Toe = ToeFor(D, Pin > .5f ? &PinLean : nullptr);
		const FVector DirUE = PP::ToUE(D).GetSafeNormal(), ToeUE = PP::ToUE(Toe).GetSafeNormal();
		const FQuat QLocal = FRotationMatrix::MakeFromZX(StickTipLocal, StickToeLocal).ToQuat();
		const FQuat QWorld = FRotationMatrix::MakeFromZX(DirUE, ToeUE).ToQuat();
		StickMesh->SetRelativeLocation(PP::ToUE(Cap));
		StickMesh->SetRelativeRotation((QWorld * QLocal.Inverse()).Rotator());
		StickMesh->SetRelativeScale3D(FVector(double(StickScale)));
	}
	else Seg(StickMesh, Cap, Cap + D * 1.23f, .028f);
	Seg(ArmC[0], ShL, LowGrip, .1f);    // mano de abajo (izquierda)
	Seg(ArmC[1], ShR, TopGrip, .1f);    // mano de arriba (derecha)

	if (bRig)
	{
		FPPRigTargets T;
		T.HipHeight = Rig.PelvisRestH + (AnkleSeatY - .077f) - .10f - .05f * Amp - .04f * Brake;   // sobre los patines, con las rodillas flexionadas
		T.HipX = HipSway; T.Lean = A; T.Twist = Tw * .45f;
		T.HandL = LowGrip; T.HandR = TopGrip; T.StickDir = D;
		T.AnkleL = FPPVec(Feet[0].X, Lifts[0] + AnkleSeatY, Feet[0].Z); T.AnkleR = FPPVec(Feet[1].X, Lifts[1] + AnkleSeatY, Feet[1].Z);
		T.FootPitchL = FMath::Clamp(Lifts[0] * 3.f, 0.f, .5f); T.FootPitchR = FMath::Clamp(Lifts[1] * 3.f, 0.f, .5f);
		// suavizado del torso (la mano, el palo y los pies van directos para no despegarse de la bocha ni del patín)
		const float Sp = State == EPPState::Swing || State == EPPState::Flight ? 28.f : 12.f;
		if (!bSmoothInit || Dt <= 0.f) { SmoothT = T; bSmoothInit = true; }
		else
		{
			SmoothT.HipHeight = FMath::FInterpTo(SmoothT.HipHeight, T.HipHeight, Dt, 10.f);
			SmoothT.HipX = FMath::FInterpTo(SmoothT.HipX, T.HipX, Dt, 10.f);
			SmoothT.Lean = FMath::FInterpTo(SmoothT.Lean, T.Lean, Dt, Sp);
			SmoothT.Twist = FMath::FInterpTo(SmoothT.Twist, T.Twist, Dt, Sp);
			SmoothT.HandL = T.HandL; SmoothT.HandR = T.HandR; SmoothT.StickDir = T.StickDir;
			SmoothT.AnkleL = T.AnkleL; SmoothT.AnkleR = T.AnkleR; SmoothT.FootPitchL = T.FootPitchL; SmoothT.FootPitchR = T.FootPitchR;
		}
		T = SmoothT;
		Rig.Pose(T);
	}
}

void APPShooter::CamUpdate(float Dt, EPPState State, const FPPVec& BallPos, float GkX)
{
	using namespace PP;
	FPPVec Want, Look;
	const FPPVec& P = GamePos;
	switch (CamMode)
	{
	case EPPCamera::TV:   // cámara de transmisión: alta, del lado de enfrente de los bancos
	{
		const float Lz = FMath::Clamp((BallPos.Z + P.Z) * .5f * .6f + .8f, 0.f, 9.f);
		Want = FPPVec(-11.4f, 4.8f, Lz + 2.2f); Look = FPPVec(BallPos.X * .3f, .5f, Lz); break;
	}
	case EPPCamera::Primera:
	{
		const FPPVec F(-FMath::Sin(Heading), 0.f, -FMath::Cos(Heading));
		Want = P + FPPVec(0.f, 1.64f, 0.f) + F * .16f;
		Look = Want + F * 4.f + FPPVec(0.f, -1.05f, 0.f);
		if (State == EPPState::Charge || State == EPPState::Swing || State == EPPState::Flight) Look = FMath::Lerp(Look, FPPVec(0.f, .55f, 0.f), .5f);
		break;
	}
	case EPPCamera::Arquero:   // detrás del arco, como si fuera el arquero
	{
		Want = FPPVec(GkX * .5f, 1.45f, -2.05f);
		Look = FPPVec(BallPos.X * .25f, .5f, FMath::Min(BallPos.Z, 7.f) * .55f + 1.2f);
		break;
	}
	default:             // detrás del jugador, mirando al arco
	{
		const FPPVec Back = FPPVec(P.X * .5f, 0.f, P.Z + 9.f).GetSafeNormal();
		Want = FPPVec(P.X, 0.f, P.Z) + Back * 7.6f + FPPVec(Back.Z * -.6f, 3.3f, 0.f);
		const float K = State == EPPState::Flight ? .75f : .5f;
		Look = FMath::Lerp(FPPVec(P.X, .7f, P.Z), FPPVec(0.f, .55f, 0.f), K) + Pvel * .1f;
		break;
	}
	}
	const bool bFp = CamMode == EPPCamera::Primera;
	if (!bCamInit) { CamPosG = Want; CamLookG = Look; bCamInit = true; }
	CamPosG = FMath::Lerp(CamPosG, Want, PowDt(bFp ? 1e-6f : .004f, Dt));
	CamLookG = FMath::Lerp(CamLookG, Look, PowDt(bFp ? 1e-5f : .001f, Dt));
	FPPVec Pos = CamPosG;
	if (Shake > 0.f) { Shake = FMath::Max(0.f, Shake - Dt * 1.6f); Pos.X += (FMath::FRand() - .5f) * .05f * Shake; Pos.Y += (FMath::FRand() - .5f) * .05f * Shake; }
	const float FovWant = 54.f + (CamMode == EPPCamera::Jugador ? FMath::Clamp(Speed / 7.f, 0.f, 1.f) * 3.5f : 0.f);
	CamFov = FMath::Lerp(CamFov, FovWant, PowDt(.05f, Dt));
	Cam->SetFieldOfView(CamFov);
	const FVector PosUE = ToUE(Pos), LookUE = ToUE(CamLookG);
	Cam->SetWorldLocationAndRotation(PosUE, FRotationMatrix::MakeFromX(LookUE - PosUE).Rotator());
}

void APPShooter::BeginPlay()
{
	Super::BeginPlay();
	auto Col = [&](UStaticMeshComponent* C, FLinearColor K, float R = .6f) { if (C) C->SetMaterial(0, PP::ColorMID(this, K, R)); };
	Col(TorsoC, FLinearColor(.04f, .06f, .22f), .8f);                       // camiseta azul
	Col(HeadC, FLinearColor(.45f, .28f, .2f), .55f);                        // piel
	for (auto& L : LegC) Col(L, FLinearColor(.03f, .04f, .12f), .8f);       // pantalón / medias
	for (auto& S : SkateC) Col(S, FLinearColor(.02f, .02f, .025f), .35f);   // patines
	for (auto& A : ArmC) Col(A, FLinearColor(.45f, .28f, .2f), .55f);
	Col(StickMesh, FLinearColor(.75f, .55f, .3f), .4f);                     // palo de madera
	// patines reales (SM_PatinReal: origen en el tobillo, ruedas apoyadas en y=0, punta hacia adelante, 30 cm de largo)
	UStaticMesh* SK2 = PP::FindAsset<UStaticMesh>(TEXT("/Game/Models"), TEXT("PatinReal"));
	if (!SK2) PP::ScreenWarn(TEXT("No encontré el patín (SM_PatinReal) en Content/Models. Corré Tools/setup_level.py o importá SourceAssets/Models/SM_PatinReal.glb."));
	if (SK2)
	{
		// el modelo tiene el origen en el talón: la punta está del lado del centro de la caja; el eje largo es el de mayor extensión horizontal
		const FBoxSphereBounds B = SK2->GetBounds();
		const bool bLenX = B.BoxExtent.X >= B.BoxExtent.Y;
		const double ExtLen = bLenX ? B.BoxExtent.X : B.BoxExtent.Y;
		if (ExtLen > 1.0)
		{
			SkateToeLocal = bLenX ? FVector(B.Origin.X >= 0.0 ? 1.0 : -1.0, 0.0, 0.0) : FVector(0.0, B.Origin.Y >= 0.0 ? 1.0 : -1.0, 0.0);
			SkateYaw = bLenX ? (SkateToeLocal.X > 0.0 ? 0.f : 180.f) : (SkateToeLocal.Y > 0.0 ? -90.f : 90.f);   // lleva la punta a +X (adelante)
			SkateScale = float(30.0 / (2.0 * ExtLen));
			for (auto& S : SkateC) if (S) { S->SetStaticMesh(SK2); S->EmptyOverrideMaterials(); }   // conserva los materiales importados
			bRealSkate = true;
			AnkleSeatY = FMath::Clamp(.55f * float(2.0 * B.BoxExtent.Z) * SkateScale / 100.f, .10f, .22f);
		}
	}
	// palo real (SM_Palo: tapón en el origen, caña hacia abajo, pala hacia adelante); si falta, queda el palo provisorio
	UStaticMesh* SM = PP::FindAsset<UStaticMesh>(TEXT("/Game/Models"), TEXT("SM_Palo"));
	if (!SM) PP::ScreenWarn(TEXT("No encontré el palo (SM_Palo) en Content/Models. Corré Tools/setup_level.py o importá SourceAssets/Models/SM_Palo.glb."));
	if (SM)
	{
		// origen en el tapón: la caña se extiende hacia abajo (eje Z) y la pala hacia un lado del eje horizontal más largo
		const FBoxSphereBounds B = SM->GetBounds();
		if (B.BoxExtent.Z > 1.0)
		{
			StickMesh->SetStaticMesh(SM);
			StickMesh->SetMaterial(0, PP::ColorMID(this, FLinearColor(.75f, .55f, .3f), .4f));
			StickTipLocal = FVector(0.0, 0.0, B.Origin.Z < 0.0 ? -1.0 : 1.0);
			const bool bBladeX = B.BoxExtent.X >= B.BoxExtent.Y;
			StickToeLocal = bBladeX ? FVector(B.Origin.X >= 0.0 ? 1.0 : -1.0, 0.0, 0.0) : FVector(0.0, B.Origin.Y >= 0.0 ? 1.0 : -1.0, 0.0);
			StickScale = float(123.0 / (2.0 * B.BoxExtent.Z));  // largo final: 1,23 m aunque el importador escale distinto
			bRealStick = true;
		}
	}
	// ---- Matías (SK_Matias) posado por código ----
	if (PP::SetupHuman(this, BodyMesh, ExtraParts, Rig, TEXT("T_Matias_BaseColor"), TEXT("Jugador")))
	{
		bRig = true;
		BodyMesh->SetRelativeRotation(Rig.CompRot.Rotator());
		BodyMesh->SetRelativeLocation(Rig.CompLoc);
		BodyMesh->SetRelativeScale3D(FVector(Rig.CompScale));
		for (auto* C : {TorsoC.Get(), HeadC.Get(), ArmC[0].Get(), ArmC[1].Get(), LegC[0].Get(), LegC[1].Get()}) if (C) C->SetVisibility(false);
	}
}

void APPShooter::CamReplay(float Dt, const FPPVec& B)
{
	// cámara de transmisión al costado del arco, baja, siguiendo la bocha
	const FPPVec Want(4.2f, 1.25f, FMath::Max(1.2f, B.Z * .35f + 1.4f));
	const FPPVec Look(B.X * .6f, .55f, FMath::Min(B.Z, 4.f) * .6f);
	if (!bCamInit) { CamPosG = Want; CamLookG = Look; bCamInit = true; }
	CamPosG = FMath::Lerp(CamPosG, Want, PP::PowDt(.02f, Dt));
	CamLookG = FMath::Lerp(CamLookG, Look, PP::PowDt(.005f, Dt));
	const FVector PosUE = PP::ToUE(CamPosG), LookUE = PP::ToUE(CamLookG);
	Cam->SetFieldOfView(50.f);
	Cam->SetWorldLocationAndRotation(PosUE, FRotationMatrix::MakeFromX(LookUE - PosUE).Rotator());
}

void APPShooter::Tick(float Dt)
{
	Super::Tick(Dt);
	if (bDebugRig && bRig && BodyMesh && GEngine)
	{
		DbgT += Dt;
		const FVector Hd = BodyMesh->BoneWorld(Rig.BoneHead());
		DrawDebugSphere(GetWorld(), Hd, 6.f, 8, FColor::Red);
		for (int s = 0; s < 2; s++)
		{
			DrawDebugSphere(GetWorld(), BodyMesh->BoneWorld(Rig.BoneWrist(s)), 4.f, 8, FColor::Green);
			DrawDebugSphere(GetWorld(), BodyMesh->BoneWorld(Rig.BoneFoot(s)), 4.f, 8, FColor::Cyan);
		}
		if (DbgT < 20.f)
		{
			const FBoxSphereBounds Bn = BodyMesh->Bounds;
			GEngine->AddOnScreenDebugMessage(701, 0.f, FColor::Yellow, FString::Printf(TEXT("[Matías] visible=%d materiales=%d  bounds centro=%s extensión=%s  cabeza=%s  actor=%s"),
				BodyMesh->IsVisible() ? 1 : 0, BodyMesh->GetNumMaterials(), *Bn.Origin.ToString(), *Bn.BoxExtent.ToString(), *Hd.ToString(), *GetActorLocation().ToString()));
		}
	}
	// el resto (estado, bocha, tiro) lo orquesta APPGameMode::Tick; acá solo la entrada continua del mouse/stick
	APPGameMode* M = Match(); if (!M) return;
	if (M->UserShoots() && (M->State == EPPState::Aim || M->State == EPPState::Charge))
	{
		if (AimStick.SizeSquared() > .04f)
		{
			Aim.X = FMath::Clamp(Aim.X + AimStick.X * Dt * 1.6f, -1.25f, 1.25f);
			Aim.Y = FMath::Clamp(Aim.Y + AimStick.Y * Dt * 1.2f, .04f, 1.5f);
		}
		else UpdateAimFromMouse();
	}
	if (M->UserShoots() && M->State == EPPState::Charge) UpdatePullFromMouse();
	if (M->UserShoots() && M->State == EPPState::Aim)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		float Mx = 0, My = 0; int32 Vx = 1, Vy = 1;
		if (PC && PC->GetMousePosition(Mx, My)) { PC->GetViewportSize(Vx, Vy); SweepT = -FMath::Clamp((Mx / float(Vx)) * 2.f - 1.f, -1.f, 1.f) * .42f; }
	}
}
