#include "PPShooter.h"
#include "PPGameMode.h"
#include "PPBall.h"
#include "PPMaterial.h"
#include "PPAssets.h"
#include "DrawDebugHelpers.h"
#include "PPPoseMesh.h"
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
	const FPPVec StickRestDir = FPPVec(-.34f, -.84f, -.42f).GetSafeNormal();
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
				// el patín real apoya las ruedas en su origen; la punta baja un poco cuando el pie se levanta
				SkateC[i]->SetRelativeLocation(PP::ToUE(Foot + FPPVec(0.f, -.09f, 0.f)));
				SkateC[i]->SetRelativeRotation(FRotator(-FMath::RadiansToDegrees(FMath::Clamp(Lift * 3.f, 0.f, .5f)), 0.f, 0.f));
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
	const FPPVec Hand(CurPose.H.X, HipY + CurPose.H.Y + .08f, CurPose.H.Z);   // +8 cm: el jugador está sobre los patines
	const float C = FMath::Cos(CurPose.Th), S = FMath::Sin(CurPose.Th);
	FPPVec D(StickRestDir.X, StickRestDir.Y * C - StickRestDir.Z * S, StickRestDir.Y * S + StickRestDir.Z * C);
	D.Normalize();
	const float Len = 1.18f;
	if (bRealStick)
	{
		const FVector DirUE = PP::ToUE(D).GetSafeNormal();
		StickMesh->SetRelativeLocation(PP::ToUE(Hand - D * .05f));
		StickMesh->SetRelativeRotation(FRotationMatrix::MakeFromZX(DirUE * double(StickShaftSign), FVector(double(StickToeSign), 0.0, 0.0)).Rotator());
		StickMesh->SetRelativeScale3D(FVector(double(StickScale)));
	}
	else Seg(StickMesh, Hand - D * .05f, Hand + D * Len, .028f);
	Seg(ArmC[0], ShL, Hand + D * .24f, .1f);    // mano de abajo (izquierda)
	Seg(ArmC[1], ShR, Hand + D * .05f, .1f);   // mano de arriba (derecha)

	if (bRig)
	{
		FPPRigTargets T;
		T.HipHeight = Rig.PelvisRestH + .08f - .10f - .05f * Amp - .04f * Brake;   // sobre los patines, con las rodillas flexionadas
		T.HipX = HipSway; T.Lean = A; T.Twist = Tw;
		T.HandL = Hand + D * .24f; T.HandR = Hand + D * .05f;
		T.AnkleL = Feet[0] + FPPVec(0.f, .08f, 0.f); T.AnkleR = Feet[1] + FPPVec(0.f, .08f, 0.f);
		T.FootPitchL = FMath::Clamp(Lifts[0] * 3.f, 0.f, .5f); T.FootPitchR = FMath::Clamp(Lifts[1] * 3.f, 0.f, .5f);
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
		const FBoxSphereBounds B = SK2->GetBounds();
		if (B.BoxExtent.X > 1.0)
		{
			for (auto& S : SkateC) if (S) { S->SetStaticMesh(SK2); S->EmptyOverrideMaterials(); }   // conserva los materiales importados
			SkateScale = float(30.0 / (2.0 * B.BoxExtent.X));
			bRealSkate = true;
		}
	}
	// palo real (SM_Palo: tapón en el origen, caña hacia abajo, pala hacia adelante); si falta, queda el palo provisorio
	UStaticMesh* SM = PP::FindAsset<UStaticMesh>(TEXT("/Game/Models"), TEXT("SM_Palo"));
	if (!SM) PP::ScreenWarn(TEXT("No encontré el palo (SM_Palo) en Content/Models. Corré Tools/setup_level.py o importá SourceAssets/Models/SM_Palo.glb."));
	if (SM)
	{
		const FBoxSphereBounds B = SM->GetBounds();
		if (B.BoxExtent.Z > 1.0)
		{
			StickMesh->SetStaticMesh(SM);
			StickMesh->SetMaterial(0, PP::ColorMID(this, FLinearColor(.75f, .55f, .3f), .4f));
			StickShaftSign = B.Origin.Z < 0.0 ? -1.f : 1.f;     // hacia dónde se extiende la caña desde el tapón
			StickToeSign = B.Origin.X >= 0.0 ? 1.f : -1.f;      // hacia dónde mira la pala
			StickScale = float(123.0 / (2.0 * B.BoxExtent.Z));  // largo final: 1,23 m aunque el importador escale distinto
			bRealStick = true;
		}
	}
	// ---- Matías (SK_Matias) posado por código ----
	USkeletalMesh* SK = PP::FindAsset<USkeletalMesh>(TEXT("/Game/Models"), TEXT("Matias"));
	if (!SK) PP::ScreenWarn(TEXT("No encontré a Matías (SK_Matias) en Content/Models. Corré Tools/setup_level.py o importá SourceAssets/Models/SK_Matias.glb."));
	if (SK)
	{
		BodyMesh->SetSkinnedAssetAndUpdate(SK, true);
		auto Tex = [](const TCHAR* N) -> UTexture* { return PP::FindAsset<UTexture2D>(TEXT("/Game/Textures"), N); };
		const TArray<FName> Slots = BodyMesh->GetMaterialSlotNames();
		for (int32 i = 0; i < Slots.Num(); i++)
		{
			const FString N = Slots[i].ToString().ToLower();
			UMaterialInterface* M = nullptr;
			if (N.Contains(TEXT("body"))) M = PP::ColorMID(this, FLinearColor::White, .55f, FLinearColor::Black, Tex(TEXT("T_Matias_BaseColor")));
			else if (N.Contains(TEXT("eyebrow"))) M = PP::CutoutMID(this, Tex(TEXT("T_Matias_Brow")), FLinearColor(.1f, .08f, .07f));
			else if (N.Contains(TEXT("eyelash"))) M = PP::CutoutMID(this, Tex(TEXT("T_Matias_Lash")), FLinearColor(.05f, .05f, .05f));
			else if (N.Contains(TEXT("short"))) M = PP::CutoutMID(this, Tex(TEXT("T_Matias_Hair")), FLinearColor::White, .45f);
			else if (N.Contains(TEXT("high-poly"))) M = PP::ColorMID(this, FLinearColor::White, .08f, FLinearColor::Black, Tex(TEXT("T_Matias_Eye")), 1.f, 1.f);
			else if (N.Contains(TEXT("teeth"))) M = PP::ColorMID(this, FLinearColor::White, .35f, FLinearColor::Black, Tex(TEXT("T_Matias_Teeth")));
			else if (N.Contains(TEXT("tongue"))) M = PP::ColorMID(this, FLinearColor::White, .5f, FLinearColor::Black, Tex(TEXT("T_Matias_Tongue")));
			if (M) BodyMesh->SetMaterial(i, M);
		}
		if (Rig.Init(BodyMesh))
		{
			bRig = true;
			PP::ScreenWarn(FString::Printf(TEXT("Matías: esqueleto OK (%d huesos), escala del componente %.3f, pelvis en reposo %.2f m"), SK->GetRefSkeleton().GetNum(), Rig.CompScale, Rig.PelvisRestH));
			BodyMesh->SetBoundsScale(4.f);
			BodyMesh->SetRelativeRotation(Rig.CompRot.Rotator());
			BodyMesh->SetRelativeLocation(Rig.CompLoc);
			BodyMesh->SetRelativeScale3D(FVector(Rig.CompScale));
			for (auto* C : {TorsoC.Get(), HeadC.Get(), ArmC[0].Get(), ArmC[1].Get(), LegC[0].Get(), LegC[1].Get()}) if (C) C->SetVisibility(false);
		}
		else BodyMesh->SetVisibility(false);
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
