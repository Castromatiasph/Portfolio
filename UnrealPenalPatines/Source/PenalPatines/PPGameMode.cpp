#include "PPGameMode.h"
#include "PPBall.h"
#include "PPGoalkeeper.h"
#include "PPShooter.h"
#include "PPHUD.h"
#include "PPArena.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "InputCoreTypes.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

using namespace PP;

const APPGameMode::FChallenge APPGameMode::Challenges[7] = {
	{ TEXT("Gol arriba"), TEXT("Meté un gol por arriba de la mitad del arco"), 3, 0 },
	{ TEXT("Gol de arrastre"), TEXT("Meté un gol de arrastre (clic derecho o F)"), 3, 0 },
	{ TEXT("Picada"), TEXT("Meté un gol de picada (Shift al soltar el tiro)"), 3, 0 },
	{ TEXT("Amague y gol"), TEXT("Amagá con Q o E y meté el gol en menos de un segundo"), 3, 0 },
	{ TEXT("Cañonazo"), TEXT("Meté un gol a más de 70 km/h"), 3, 0 },
	{ TEXT("Al ángulo"), TEXT("Meté un gol en uno de los ángulos de arriba"), 4, 0 },
	{ TEXT("Tres seguidos"), TEXT("Meté tres goles seguidos sin errar"), 6, 3 },
};

APPGameMode::APPGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = APPShooter::StaticClass();
	HUDClass = APPHUD::StaticClass();
}

void APPGameMode::BeginPlay()
{
	Super::BeginPlay();
	UWorld* W = GetWorld();
	for (TActorIterator<APPBall> It(W); It; ++It) { Ball = *It; break; }
	for (TActorIterator<APPGoalkeeper> It(W); It; ++It) { Gk = *It; break; }
	for (TActorIterator<APPArena> It(W); It; ++It) { Arena = *It; break; }
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (!Arena) Arena = W->SpawnActor<APPArena>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (!Ball) Ball = W->SpawnActor<APPBall>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (!Gk) Gk = W->SpawnActor<APPGoalkeeper>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture);
		PC->SetInputMode(Mode);
	}
	if (Arena) Arena->ApplyQuality(Quality);
	// ambiente de estadio en loop (si importaste Audio con setup_level.py)
	if (USoundBase* Amb = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/S_Ambiente.S_Ambiente"))) UGameplayStatics::SpawnSound2D(this, Amb, .35f);
	State = EPPState::Menu;
	Hint = TEXT("1-5 modo · 6 penal/libre · 7 nivel · 8 arquero · 9 calidad · Enter: jugar");
}

AActor* APPGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (AActor* S = Super::ChoosePlayerStart_Implementation(Player)) return S;
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return GetWorld()->SpawnActor<APlayerStart>(ToUE(StartPos()), FRotator::ZeroRotator, P);   // si el mapa no trae PlayerStart
}

void APPGameMode::ResolveActors()
{
	if (!Shooter) Shooter = Cast<APPShooter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

void APPGameMode::PlaySfx(const FString& Name, float Volume, const FVector& Loc)
{
	const FName Key(*Name);
	TObjectPtr<USoundBase>* F = Sounds.Find(Key);
	USoundBase* S = nullptr;
	if (F) S = F->Get();
	else
	{
		S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/%s.%s"), *Name, *Name));
		Sounds.Add(Key, S);
	}
	if (S) UGameplayStatics::PlaySoundAtLocation(this, S, Loc, FMath::Clamp(Volume, 0.f, 2.f));
}

void APPGameMode::Fire(const TCHAR* Name, float Param, const FPPVec* G)
{
	const FVector Loc = G ? ToUE(*G) : (Ball ? ToUE(Ball->Pos) : FVector::ZeroVector);
	const FString N(Name);
	if (N == TEXT("hit")) PlaySfx(TEXT("S_Hit"), .5f + .6f * Param, Loc);
	else if (N == TEXT("bounce")) PlaySfx(TEXT("S_Bounce"), FMath::Min(1.f, Param / 6.f), Loc);
	else if (N == TEXT("board")) PlaySfx(TEXT("S_Board"), FMath::Min(1.f, Param / 8.f), Loc);
	else if (N == TEXT("post")) PlaySfx(TEXT("S_Post"), 1.f, Loc);
	else if (N == TEXT("net")) PlaySfx(TEXT("S_Net"), .7f + Param, Loc);
	else if (N == TEXT("save")) PlaySfx(TEXT("S_Save"), 1.f, Loc);
	else if (N == TEXT("goal") || N == TEXT("cheer")) PlaySfx(TEXT("S_Cheer"), .9f, Loc);
	else if (N == TEXT("groan")) PlaySfx(TEXT("S_Groan"), .8f, Loc);
	else if (N == TEXT("whistle")) PlaySfx(TEXT("S_Whistle"), .7f, Loc);
	if (Arena)
	{
		if (N == TEXT("goal")) { Arena->Cheer(); Arena->Confetti(); }
		else if (N == TEXT("cheer")) Arena->Cheer(1.8f);
	}
	OnPPEvent(FName(Name), Param, Loc);
}

FPPVec APPGameMode::StartPos() const
{
	const FPPVec BL = Shooter ? Shooter->BallLocal : FPPVec(-.45f, BallR, -.8f);
	return FPPVec(-BL.X, 0.f, SpotZ() - BL.Z);   // la bocha arranca sobre el punto elegido
}

int32 APPGameMode::GoalsOf(const TArray<EPPOutcome>& A) const { int32 N = 0; for (EPPOutcome O : A) if (O == EPPOutcome::Goal) N++; return N; }
int32 APPGameMode::GoalsOfRival() const { int32 N = 0; for (bool B : Rival) if (B) N++; return N; }
float APPGameMode::EstimatedError(float P) const
{
	const FPPVec S = Ball ? Ball->Pos : FPPVec(0, 0, PP::SpotZ);
	return (.03f + P * P * P * .11f) * FMath::Clamp(Hypot(S.X, S.Z) / PP::SpotZ, .8f, 2.4f);
}

EPPCamera APPGameMode::NextCamera(EPPCamera Cur) const
{
	if (Format == EPPFormat::Arquero) return Cur == EPPCamera::Arquero ? EPPCamera::TV : Cur == EPPCamera::TV ? EPPCamera::Jugador : EPPCamera::Arquero;
	return Cur == EPPCamera::Jugador ? EPPCamera::TV : Cur == EPPCamera::TV ? EPPCamera::Primera : EPPCamera::Jugador;
}

void APPGameMode::CycleQuality()
{
	Quality = EPPQuality((int32(Quality) + 1) % 3);
	if (Arena) Arena->ApplyQuality(Quality);
}

FString APPGameMode::ShotName() const
{
	if (!Shooter) return FString();
	FString N;
	switch (Shooter->ShotKind)
	{
	case EPPShotKind::Golpe: N = TEXT("Golpe"); break; case EPPShotKind::Arrastre: N = TEXT("Arrastre"); break;
	case EPPShotKind::Picada: N = TEXT("Picada"); break; case EPPShotKind::Fuetazo: N = TEXT("Fuetazo"); break;
	case EPPShotKind::Reves: N = TEXT("Revés"); break;
	}
	if (bHasShot && Shot.CurveAccel != 0.f) N += TEXT(" con efecto");
	return N;
}

FString APPGameMode::ZoneName(float X, float Y) const
{
	return FString::Printf(TEXT("%s%s"), Y > .68f ? TEXT("arriba") : Y < .33f ? TEXT("abajo") : TEXT("a media altura"),
		X < -.3f ? TEXT(", a la izquierda") : X > .3f ? TEXT(", a la derecha") : TEXT(", al medio"));
}

void APPGameMode::SetFlash(const FString& Big, const FString& Small, int32 Cls, float Seconds) { FlashBig = Big; FlashSmall = Small; FlashClass = Cls; FlashT = Seconds; }

// ------------------------------------------------------------------ partido
void APPGameMode::StartMatch()
{
	ResolveActors();
	Results.Reset(); Rival.Reset(); DuelA.Reset(); DuelB.Reset(); bDuelA = true; BestKmh = 0; bPaused = false;
	ChI = 0; ChTries = 0; ChStreak = 0; ChTotal = 0; ChStars.Reset();
	Gk->bUser = (Format == EPPFormat::Arquero || Format == EPPFormat::Duelo);
	Gk->StyleSel = GoalieStyle; Gk->PickStyle(); Gk->Diff = FPPDiff::Make(Difficulty);
	Shooter->CamMode = Format == EPPFormat::Arquero ? EPPCamera::Arquero : EPPCamera::Jugador;
	Shooter->bCamInit = false;
	SetupPenalty();
}

void APPGameMode::ToMenu()
{
	State = EPPState::Menu; ClearFlash();
	if (Shooter) { Shooter->Place(StartPos()); BallToShooter(); Shooter->CamMode = EPPCamera::Jugador; Shooter->bCamInit = false; }
	Hint = TEXT("1-5 modo · 6 penal/libre · 7 nivel · 8 arquero · 9 calidad · Enter: jugar");
}

void APPGameMode::BallToShooter()
{
	Ball->Vel = FPPVec::ZeroVector; Ball->Spin = FPPVec::ZeroVector;
	Ball->SetGamePos(Shooter->BallWorld());
	Ball->SetActorHiddenInGame(false);
}

void APPGameMode::SetupPenalty()
{
	ResolveActors();
	ReboundCount = 0; bHasShot = false; bLaunched = false; ResultTimer = 0.f; LooseT = 0.f; Rec.Reset();
	Gk->StyleSel = GoalieStyle; Gk->Diff = FPPDiff::Make(Difficulty);
	Gk->bUser = (Format == EPPFormat::Arquero || Format == EPPFormat::Duelo);
	Gk->ResetForShot(!Gk->bUser);
	Gk->In = FPPGkInput(); Gk->bOuting = false; Gk->UserExt = 0.f;
	Shooter->Place(StartPos());
	BallToShooter();
	Shooter->Power = 0.f; Shooter->ChargeT = 0.f;
	bAiActive = (Format == EPPFormat::Arquero);
	if (bAiActive) { Ai = MakePlan(); AiT = 0.f; AiMove = FPPVec2::ZeroVector; }
	State = EPPState::Aim;
	ClearFlash();
	switch (Format)
	{
	case EPPFormat::Arquero: Hint = TEXT("Mové el mouse para ubicarte y apuntar las manos · clic o Espacio: tirarte · Shift: salir por el área · clic derecho o E: palo · C: cámara"); break;
	case EPPFormat::Duelo: Hint = FString::Printf(TEXT("Patea %s: WASD, mouse, Shift izq. picada, Ctrl izq. efecto · Ataja %s: flechas, Enter para tirarse, Shift der. salir, Ctrl der. palo"), *PlayerName(bDuelA), *PlayerName(!bDuelA)); break;
	case EPPFormat::Desafio:
	{
		const FChallenge& C = Challenges[FMath::Min(ChI, 6)];
		Hint = FString::Printf(TEXT("Desafío: %s."), C.Desc);
		SetFlash(C.Name, C.Desc, 0, 1.8f);
		break;
	}
	default: Hint = TEXT("WASD patina · Q/E amagues · clic y tirar el mouse para atrás: golpe (Shift al soltar: picada, Ctrl: efecto) · clic derecho/F arrastre · V fuetazo · B revés · C cámara · R vuelve al punto"); break;
	}
	Fire(TEXT("whistle"));
}

void APPGameMode::ResetToSpot()
{
	if (Format == EPPFormat::Practica && State != EPPState::Menu && State != EPPState::End && State != EPPState::Rival && State != EPPState::Replay) SetupPenalty();
}

void APPGameMode::TogglePause()
{
	if (State == EPPState::Menu || State == EPPState::End) return;
	bPaused = !bPaused;   // pausa propia: se congela la simulación pero el HUD y la cámara siguen
}

bool APPGameMode::Stealable() const
{
	return State == EPPState::Aim || State == EPPState::Charge || (State == EPPState::Swing && !bLaunched);
}

// ------------------------------------------------------------------ tiros
void APPGameMode::StartCharge()
{
	if (State != EPPState::Aim) return;
	State = EPPState::Charge;
	Shooter->ChargeT = 0.f; Shooter->Power = 0.f; Shooter->PowerVis = 0.f;
	Shooter->ContactPose = Shooter->CurPose;
	const FPPVec W = Shooter->BallWorld();
	const float Ch = FMath::Cos(Shooter->Heading), Sh = FMath::Sin(Shooter->Heading);
	const float Dx = W.X - Shooter->GamePos.X, Dz = W.Z - Shooter->GamePos.Z;
	Shooter->BallLocalPress = FPPVec(Dx * Ch - Dz * Sh, PP::BallR, Dx * Sh + Dz * Ch);
}

void APPGameMode::Release(bool bLob, bool bCurve)
{
	if (State != EPPState::Charge) return;
	Shooter->bPullHeld = false;
	const float P = Shooter->Power;
	if (P < .08f) { State = EPPState::Aim; Shooter->Power = 0.f; return; }
	const EPPShotKind K = bLob ? EPPShotKind::Picada : EPPShotKind::Golpe;
	bShotCurve = bCurve && !bLob;
	State = EPPState::Swing; bLaunched = false;
	Shooter->SwingStart = Shooter->CurPose;
	Shooter->BeginSwing(K, .16f - .06f * P);
}

void APPGameMode::PushShot()
{
	if (State != EPPState::Aim) return;
	bShotCurve = false;
	StartCharge(); Shooter->Power = .45f; State = EPPState::Swing; bLaunched = false;
	Shooter->SwingStart = Shooter->CurPose;
	Shooter->BeginSwing(EPPShotKind::Arrastre, .07f);
}

void APPGameMode::WristShot()
{
	if (State != EPPState::Aim) return;
	bShotCurve = false;
	StartCharge(); Shooter->Power = .55f; State = EPPState::Swing; bLaunched = false;
	Shooter->SwingStart = FPPPose::Lerp(Shooter->CurPose, APPShooter::Back(), .32f);
	Shooter->BeginSwing(EPPShotKind::Fuetazo, .085f);
}

void APPGameMode::BackhandShot()
{
	if (State != EPPState::Aim) return;
	bShotCurve = false;
	StartCharge(); Shooter->Power = .5f; State = EPPState::Swing; bLaunched = false;
	Shooter->SwingStart = APPShooter::Backhand();
	Shooter->BeginSwing(EPPShotKind::Reves, .13f);
}

void APPGameMode::Launch()
{
	const FPPDiff& D = Gk->Diff;
	const float P = Shooter->Power;
	const EPPShotKind K = Shooter->ShotKind;
	const FPPVec S = Ball->Pos;
	const float Dist = Hypot(S.X, S.Z);
	const float DistK = FMath::Clamp(Dist / PP::SpotZ, .8f, 2.4f);
	float V = 8.f + P * 18.f, Err = (.03f + P * P * P * .11f) * DistK, Lift = P * P * P * P * .18f, VyCap = 4.2f;
	switch (K)
	{
	case EPPShotKind::Arrastre: V = 11.f + FMath::FRand() * 3.f; Err = .022f * DistK; Lift = 0.f; VyCap = 3.4f; break;
	case EPPShotKind::Picada:   V = 6.5f + P * 7.f; Lift = .25f; VyCap = 7.5f; Err *= 1.3f; break;
	case EPPShotKind::Fuetazo:  V = 12.f + FMath::FRand() * 4.5f; Err = .032f * DistK; Lift = .04f; VyCap = 4.f; break;
	case EPPShotKind::Reves:    V = 9.5f + FMath::FRand() * 3.5f; Err = .045f * DistK; Lift = .08f; VyCap = 4.5f; break;
	default: break;
	}
	const FVector2f Aim(Shooter->Aim.X, K == EPPShotKind::Arrastre ? FMath::Min(Shooter->Aim.Y, .55f) : Shooter->Aim.Y);
	const float Tx = Aim.X + Gauss() * Err;
	const float Ty = FMath::Max(.02f, Aim.Y + Gauss() * Err * .8f + Lift);
	const float Dz = S.Z, Dd = Hypot(Tx - S.X, Dz), Tt = Dd / V;
	// con efecto: la bocha sale abierta y dobla hacia el palo elegido (aceleración lateral constante)
	const float CurveA = bShotCurve ? -(Tx < 0.f ? -1.f : 1.f) * (5.f + P * 4.f) : 0.f;
	Ball->Vel = FPPVec((Tx - S.X - .5f * CurveA * Tt * Tt) / Tt, FMath::Min(VyCap, (Ty - S.Y + .5f * Grav * Tt * Tt) / Tt), -Dz / Tt);
	Ball->Spin = FPPVec(Ball->Vel.Z, 0.f, -Ball->Vel.X).GetSafeNormal() * (Ball->Vel.Size() / BallR * (.6f + FMath::FRand() * .3f));
	const int32 Kmh = FMath::RoundToInt(Ball->Vel.Size() * 3.6f);
	BestKmh = FMath::Max(BestKmh, Kmh);

	// el arquero de la máquina lee el tiro
	const float Frac = (S.Z - Gk->PlaneZ()) / Dz;
	const float SinceDeke = Time - Shooter->LastDekeTime;
	const float Read = (.6f + .8f * P) * (K == EPPShotKind::Arrastre ? 1.6f : K == EPPShotKind::Fuetazo ? 1.45f : K == EPPShotKind::Reves ? 1.5f : 1.f)
		* (SinceDeke < .7f ? 1.7f : 1.f) * FMath::Clamp(1.25f - Dist / 20.f, .7f, 1.2f);
	const FPPGKStyle GS = FPPGKStyle::Make(Gk->Style);
	float PredX = (CurveA != 0.f ? S.X + Ball->Vel.X * Tt * Frac : S.X + (Tx - S.X) * Frac) + Gauss() * D.Noise * Read;
	float PredY = Ty + Gauss() * D.Noise * .8f * Read;
	float React = D.React * (.85f + FMath::FRand() * .3f) * (K == EPPShotKind::Arrastre ? 1.45f : K == EPPShotKind::Fuetazo ? 1.2f : K == EPPShotKind::Reves ? 1.25f : 1.f);
	if (!Gk->bUser)
	{
		React *= GS.React;
		PredX += Gauss() * D.Noise * (GS.Noise - 1.f);
		// el que se tira temprano ya eligió un lado cuando el atacante levantó el palo
		if (GS.bEarly && Gk->EarlySide && FMath::FRand() < .6f) { PredX = Gk->EarlySide * (.4f + FMath::FRand() * .3f); PredY = .15f + FMath::FRand() * .4f; React = .02f; }
		else if (FMath::FRand() < FMath::Max(0.f, D.Guess + GS.Guess) * (SinceDeke < .7f ? 1.6f : 1.f))
		{ PredX = (FMath::FRand() < .5f ? -1.f : 1.f) * (.35f + FMath::FRand() * .4f); PredY = .15f + FMath::FRand() * .85f; React = .06f; }
	}
	const float GTarget = FMath::Clamp(PredX * .55f, -.4f, .4f);
	const int32 ReachSide = FMath::Abs(PredX - GTarget) < .18f ? 0 : (PredX - GTarget > 0.f ? 1 : -1);
	// cada atajada sale distinta: velocidad para estirarse, cuánto adelanta las manos, cuánto se tumba y la técnica
	auto R = []() { return FMath::FRand(); };
	const bool bLowS = PredY < .42f, bFarS = FMath::Abs(PredX - GTarget) > .3f;
	FName Tech;
	if (bLowS) Tech = bFarS ? (R() < .65f ? FName("split") : FName("stack")) : (R() < .55f ? FName("mariposa") : FName("palo"));
	else Tech = bFarS ? (R() < .7f ? FName("mano") : FName("estirada")) : (R() < .5f ? FName("mano") : FName("cuerpo"));

	Shot = FPPShot();
	Shot.Kind = K; Shot.CurveAccel = CurveA; Shot.Tx = Tx; Shot.Ty = Ty; Shot.Kmh = Kmh;
	Shot.PredX = PredX; Shot.PredY = PredY; Shot.React = React; Shot.GTarget = GTarget; Shot.ReachSide = ReachSide;
	Shot.Ramp = D.Ramp * (.8f + R() * .45f); Shot.ReachZ = .26f + R() * .14f; Shot.Tilt = .75f + R() * .55f; Shot.Tech = Tech;
	Shot.LaunchT = Time; Shot.bDeke = SinceDeke < 1.2f;
	Shot.bGkEarly = (Format == EPPFormat::Arquero && Gk->Kneel > .6f);
	bHasShot = true;
	State = EPPState::Flight;
	Fire(TEXT("hit"), P);
}

// ------------------------------------------------------------------ física de la bocha
void APPGameMode::StepBall(float Dt)
{
	if (bHasShot && Shot.bCaught) { Ball->Pos = Gk->GlovePos(); Ball->Vel = FPPVec::ZeroVector; return; }
	const int32 N = 5; const float H = Dt / N;
	FPPVec& P = Ball->Pos; FPPVec& V = Ball->Vel;
	for (int32 i = 0; i < N; i++)
	{
		const float Pz = P.Z;
		V.Y -= Grav * H;
		if (bHasShot && Shot.CurveAccel != 0.f && Shot.Outcome == EPPOutcome::None && P.Z > -.2f) V.X += Shot.CurveAccel * H;
		P += V * H;
		// ¿la frena el arquero?
		if (bHasShot && !Shot.bGkChecked && Pz > Gk->PlaneZ() && P.Z <= Gk->PlaneZ())
		{
			Shot.bGkChecked = true;
			if (Shot.Outcome == EPPOutcome::None)
			{
				const FString Part = Gk->BlocksBall(P);
				if (!Part.IsEmpty()) { Gk->Deflect(Part, P, V, Shot); P.Z = Gk->PlaneZ() + .01f; Settle(EPPOutcome::Save, Part); }
			}
		}
		// línea de gol
		if (bHasShot && !Shot.bLineChecked && Pz > 0.f && P.Z <= 0.f)
		{
			Shot.bLineChecked = true;
			if (Shot.Outcome == EPPOutcome::None)
			{
				const float Ax = FMath::Abs(P.X);
				if (Ax < PostHalf - BallR && P.Y < BarH - BallR) { Shot.bInGoal = true; Shot.Gx = P.X; Shot.Gy = P.Y; Settle(EPPOutcome::Goal, FString()); }
				else if (Ax < PostHalf + 2.f * FrameR + BallR && P.Y < BarH + 2.f * FrameR + BallR)
				{
					const bool bBar = P.Y >= BarH - BallR && Ax < PostHalf;
					V.Z = FMath::Abs(V.Z) * .35f; P.Z = .01f;
					if (bBar) V.Y = -FMath::Abs(V.Y) * .2f - .6f; else V.X += (P.X < 0.f ? -1.f : 1.f) * (.6f + FMath::FRand() * 1.2f);
					Fire(TEXT("post"));
					Settle(EPPOutcome::Post, bBar ? TEXT("Pegó en el travesaño") : TEXT("Pegó en el palo"));
				}
				else Settle(EPPOutcome::Miss, P.Y > BarH ? TEXT("Se fue por arriba del travesaño") : TEXT("Se fue desviada"));
			}
		}
		// dentro del arco: red de atrás y laterales
		if (bHasShot && Shot.bInGoal)
		{
			const float Zb = -.92f + .42f * FMath::Clamp(P.Y / CY, 0.f, 1.f);
			if (P.Z < Zb + BallR)
			{
				P.Z = Zb + BallR; V.Z = FMath::Abs(V.Z) * .06f; V.X *= .5f; V.Y *= .5f;
				if (!Shot.bNetHit) { Shot.bNetHit = true; Fire(TEXT("net"), FMath::Min(.26f, .05f + Shot.Kmh * .0028f), &P); }
			}
			if (FMath::Abs(P.X) > PostHalf - BallR) { P.X = (P.X < 0.f ? -1.f : 1.f) * (PostHalf - BallR); V.X *= -.2f; }
			if (P.Y > BarH - BallR) { P.Y = BarH - BallR; V.Y = -FMath::Abs(V.Y) * .2f; }
			if (P.Z > -BallR) { P.Z = -BallR; V.Z = -FMath::Abs(V.Z) * .3f; }
		}
		// piso: rebote y rodadura sobre el parquet
		if (P.Y < BallR + .0005f)
		{
			P.Y = FMath::Max(P.Y, BallR);
			if (V.Y < 0.f) { if (V.Y < -1.2f) Fire(TEXT("bounce"), -V.Y); V.Y = -V.Y * .3f; if (V.Y < .45f) V.Y = 0.f; }
			const float Vh = Hypot(V.X, V.Z);
			if (Vh > 1e-4f) { const float Nv = FMath::Max(0.f, Vh - (.55f + .16f * Vh) * H); V.X *= Nv / Vh; V.Z *= Nv / Vh; }
		}
		else { const float Sv = V.Size(); if (Sv > 1e-3f) V *= FMath::Max(0.f, 1.f - .0045f * Sv * H); }   // resistencia del aire
		// vallas
		if (P.Y < 1.f)
		{
			FPPVec Nn;
			if (InsideRink(P, BallR, 8.f, Nn)) { const float Vn = FPPVec::DotProduct(V, Nn); if (Vn < 0.f) { if (Vn < -.6f) Fire(TEXT("board"), -Vn); V += Nn * (-Vn * 1.35f); } }
		}
		// la bocha rebota contra el arco por detrás o por los costados
		if (bHasShot && !Shot.bInGoal && P.Y <= CY + .04f && P.Z < 0.f && P.Z > GoalBack - BallR && FMath::Abs(P.X) < GoalSide + BallR)
		{
			if (Pz <= GoalBack - BallR) { P.Z = GoalBack - BallR; V.Z = -FMath::Abs(V.Z) * .25f; V.X *= .6f; }
			else if (Pz < 0.f) { P.X = (P.X < 0.f ? -1.f : 1.f) * (GoalSide + BallR); V.X = (P.X < 0.f ? -1.f : 1.f) * FMath::Abs(V.X) * .3f; }
		}
	}
	Ball->SetActorHiddenInGame(P.Z < EndZ - 1.5f || P.Y < -1.f);
	// giro: en el piso tiende a rodar sin deslizar, en el aire conserva el efecto
	if (P.Y <= BallR + .003f) { const FPPVec Target = FPPVec(V.Z, 0.f, -V.X) / BallR; Ball->Spin = FMath::Lerp(Ball->Spin, Target, PowDt(.002f, Dt)); }
	else Ball->Spin *= FMath::Pow(.8f, Dt);
	Ball->ApplySpin(Dt);
}

// ------------------------------------------------------------------ resolución del tiro
bool APPGameMode::ChallengeOk(int32 I, float Gx, float Gy, EPPShotKind K, int32 Kmh, bool bDeke) const
{
	switch (I)
	{
	case 0: return Gy > .55f;
	case 1: return K == EPPShotKind::Arrastre;
	case 2: return K == EPPShotKind::Picada;
	case 3: return bDeke;
	case 4: return Kmh > 70;
	case 5: return Gy > .7f && FMath::Abs(Gx) > .5f;
	default: return true;
	}
}

void APPGameMode::Settle(EPPOutcome Kind, const FString& Detail)
{
	if (Shot.Outcome != EPPOutcome::None) return;
	Shot.Outcome = Kind;
	if (Kind == EPPOutcome::Goal) Shot.GoalT = Time;
	if (Format == EPPFormat::Practica)
	{
		// práctica: se cuenta el tiro y la bocha sigue viva hasta que el usuario aprieta R
		Results.Add(Kind); State = EPPState::Loose; LooseT = 0.f; Shot.PrevZ = Ball->Pos.Z;
		const FString Kn = ShotName();
		switch (Kind)
		{
		case EPPOutcome::Goal: ResultTimer = 2.6f; SetFlash(ReboundCount > 0 ? TEXT("¡Gol de rebote!") : TEXT("¡Gol!"), FString::Printf(TEXT("%s · %d km/h · %s"), *Kn, Shot.Kmh, *ZoneName(Shot.Tx, Shot.Ty)), 1); Fire(TEXT("goal")); Shooter->Shake = 1.f; break;
		case EPPOutcome::Save: SetFlash(Shot.bCaught ? TEXT("¡Atajó!") : TEXT("¡Rebote!"), Shot.bCaught ? FString(TEXT("Se la quedó con el guante")) : FString::Printf(TEXT("La sacó con %s. ¡Andá a buscarla!"), *Detail), 2); Fire(TEXT("save")); break;
		case EPPOutcome::Post: SetFlash(TEXT("¡Palo!"), Detail + TEXT(". ¡Andá a buscarla!"), 2); break;
		default: SetFlash(TEXT("Afuera"), Detail + TEXT(". ¡Andá a buscarla!"), 2); Fire(TEXT("groan")); break;
		}
		return;
	}
	Finalize(Kind, Detail);
}

void APPGameMode::Finalize(EPPOutcome Kind, const FString& Detail)
{
	Shot.Outcome = Kind; if (Kind == EPPOutcome::Goal && Shot.GoalT <= 0.f) Shot.GoalT = Time;
	State = EPPState::Flight; Results.Add(Kind); ResultTimer = 2.8f;
	const FString Kn = ShotName();
	const FString Full = FString::Printf(TEXT("%s · %d km/h · %s"), *Kn, Shot.Kmh, *ZoneName(Shot.Tx, Shot.Ty));

	if (Format == EPPFormat::Duelo)
	{
		(bDuelA ? DuelA : DuelB).Add(Kind);
		const FString Sh = PlayerName(bDuelA), GkN = PlayerName(!bDuelA);
		switch (Kind)
		{
		case EPPOutcome::Goal: SetFlash(FString::Printf(TEXT("¡Gol del %s!"), *Sh), Full, 1); Fire(TEXT("goal")); Shooter->Shake = 1.f; break;
		case EPPOutcome::Save: SetFlash(FString::Printf(TEXT("¡Atajó el %s!"), *GkN), Shot.bCaught ? FString(TEXT("Se la quedó con el guante")) : FString::Printf(TEXT("Con %s"), *Detail), 2); Fire(TEXT("save")); Fire(TEXT("groan")); break;
		case EPPOutcome::Post: SetFlash(TEXT("¡Palo!"), Detail, 2); Fire(TEXT("groan")); break;
		default: SetFlash(TEXT("Afuera"), Detail, 2); Fire(TEXT("groan")); break;
		}
		return;
	}
	if (Format == EPPFormat::Desafio)
	{
		const int32 Ci = FMath::Min(ChI, 6);
		const FChallenge& C = Challenges[Ci];
		ChTotal++;
		const bool bGood = Kind == EPPOutcome::Goal && ChallengeOk(Ci, Shot.Gx, Shot.Gy, Shot.Kind, Shot.Kmh, Shot.bDeke);
		if (C.Streak) { if (Kind == EPPOutcome::Goal) ChStreak++; else ChStreak = 0; }
		ChTries++;
		if (bGood && (!C.Streak || ChStreak >= C.Streak))
		{
			const int32 St = ChTries <= FMath::CeilToInt(C.Tries / 3.f) ? 3 : ChTries <= FMath::CeilToInt(C.Tries * 2.f / 3.f) ? 2 : 1;
			ChStars.Add(St); ChI++; ChTries = 0; ChStreak = 0; Shot.bChDone = true;
			FString Stars; for (int s = 0; s < 3; s++) Stars += s < St ? TEXT("*") : TEXT("-");
			SetFlash(TEXT("¡Desafío cumplido!"), FString::Printf(TEXT("%s · %s"), C.Name, *Stars), 1); Fire(TEXT("goal"));
		}
		else if (Kind == EPPOutcome::Goal && C.Streak) { SetFlash(TEXT("¡Gol!"), FString::Printf(TEXT("Llevás %d de %d seguidos"), ChStreak, C.Streak), 1); Fire(TEXT("cheer")); }
		else if (ChTries >= C.Tries)
		{
			ChTries = 0; ChStreak = 0;
			SetFlash(Kind == EPPOutcome::Goal ? TEXT("Gol, pero no vale") : TEXT("No lo lograste"), FString::Printf(TEXT("Se te acabaron los intentos. Volvés a empezar «%s»"), C.Name), 2); Fire(TEXT("groan"));
		}
		else
		{
			const TCHAR* Big = Kind == EPPOutcome::Goal ? TEXT("Gol, pero no vale") : Kind == EPPOutcome::Save ? TEXT("¡Atajó!") : Kind == EPPOutcome::Post ? TEXT("¡Palo!") : TEXT("Afuera");
			SetFlash(Big, FString::Printf(TEXT("%s. Te quedan %d"), C.Desc, C.Tries - ChTries), 2);
			Fire(Kind == EPPOutcome::Goal ? TEXT("cheer") : TEXT("groan"));
		}
		return;
	}
	if (Format == EPPFormat::Arquero)
	{
		switch (Kind)
		{
		case EPPOutcome::Goal: SetFlash(Shot.bGkEarly ? TEXT("Te tiraste antes") : TEXT("Gol del rival"), Shot.bGkEarly ? FString::Printf(TEXT("Te ganó el palo de arriba · %s"), *Kn) : Full, 2); Fire(TEXT("groan")); break;
		case EPPOutcome::Save: SetFlash(TEXT("¡Atajaste!"), Shot.bCaught ? FString::Printf(TEXT("Te la quedaste con el guante · %d km/h"), Shot.Kmh) : FString::Printf(TEXT("Con %s · %d km/h"), *Detail, Shot.Kmh), 1); Fire(TEXT("save")); Fire(TEXT("cheer")); break;
		case EPPOutcome::Post: SetFlash(TEXT("¡Palo!"), TEXT("Te salvó el palo"), 1); Fire(TEXT("cheer")); break;
		default: SetFlash(TEXT("Afuera"), TEXT("El rival la tiró desviada"), 1); Fire(TEXT("cheer")); break;
		}
		return;
	}
	switch (Kind)
	{
	case EPPOutcome::Goal: SetFlash(ReboundCount > 0 ? TEXT("¡Gol de rebote!") : TEXT("¡Gol!"), Full, 1); Fire(TEXT("goal")); Shooter->Shake = 1.f; break;
	case EPPOutcome::Save: SetFlash(TEXT("¡Atajó!"), Shot.bCaught ? FString(TEXT("Se la quedó con el guante")) : FString::Printf(TEXT("La sacó con %s"), *Detail), 2); Fire(TEXT("save")); Fire(TEXT("groan")); break;
	case EPPOutcome::Post: SetFlash(TEXT("¡Palo!"), Detail, 2); Fire(TEXT("groan")); break;
	default: SetFlash(TEXT("Afuera"), Detail, 2); Fire(TEXT("groan")); break;
	}
}

void APPGameMode::DoSteal(bool bByUser)
{
	bLaunched = true;
	FPPVec Dir = Ball->Pos - FPPVec(Gk->Pos.X, 0.f, Gk->Pos.Y); Dir.Y = 0.f; Dir = Dir.GetSafeNormal();
	const float Sp = 3.2f + FMath::FRand() * 1.6f;
	Shot = FPPShot();
	Shot.Kmh = FMath::RoundToInt(Sp * 3.6f); Shot.bGkChecked = true; Shot.bLineChecked = true; Shot.NoPick = .8f; Shot.LaunchT = Time;
	bHasShot = true;
	Ball->Vel = FPPVec(Dir.X * Sp + (FMath::FRand() - .5f) * .8f, .15f, FMath::Max(.6f, Dir.Z) * Sp);
	Ball->Spin = FPPVec::ZeroVector;
	Gk->PokeAlpha = 0.f;
	Fire(TEXT("save"));
	if (Format == EPPFormat::Duelo || Format == EPPFormat::Desafio) { Finalize(EPPOutcome::Save, TEXT("el palo")); return; }
	Shot.Outcome = EPPOutcome::Save; Results.Add(EPPOutcome::Save);
	if (Format == EPPFormat::Practica)
	{
		State = EPPState::Loose; LooseT = 0.f; Shot.PrevZ = Ball->Pos.Z;
		SetFlash(TEXT("¡Te la sacó!"), TEXT("El arquero salió con el palo. ¡Andá a buscarla!"), 2);
		return;
	}
	State = EPPState::Flight; ResultTimer = 2.6f;
	if (Format == EPPFormat::Arquero) { SetFlash(TEXT("¡Se la sacaste!"), TEXT("Le robaste la bocha con el palo"), 1); Fire(TEXT("cheer")); }
	else { SetFlash(TEXT("¡Te la sacó!"), TEXT("El arquero salió con el palo"), 2); Fire(TEXT("groan")); }
}

// ------------------------------------------------------------------ tanda, duelo y fin
bool APPGameMode::Decided() const
{
	const int32 N = Results.Num(), M = Rival.Num(), A = GoalsOf(Results), B = GoalsOfRival();
	if (N <= 5 && M <= 5) { if (A + (5 - N) < B || B + (5 - M) < A) return true; if (N < 5 || M < 5) return false; return A != B; }
	return N == M && A != B;
}

bool APPGameMode::DuelDecided() const
{
	const int32 N = DuelA.Num(), M = DuelB.Num(), A = GoalsOf(DuelA), B = GoalsOf(DuelB);
	if (N <= 5 && M <= 5) { if (A + (5 - N) < B || B + (5 - M) < A) return true; if (N < 5 || M < 5) return false; return A != B; }
	return N == M && A != B;
}

void APPGameMode::NextTurn()
{
	switch (Format)
	{
	case EPPFormat::Tanda:
		if (Decided()) { EndMatch(); return; }
		if (Rival.Num() < Results.Num()) { RivalShot(); return; }
		break;
	case EPPFormat::Arquero:
		if (Results.Num() >= 5) { EndMatch(); return; }
		break;
	case EPPFormat::Desafio:
		if (ChI >= 7) { EndMatch(); return; }
		break;
	case EPPFormat::Duelo:
	{
		if (DuelDecided()) { EndMatch(); return; }
		const bool bNextA = DuelA.Num() <= DuelB.Num();
		if (bNextA != bDuelA)
		{
			bDuelA = bNextA; State = EPPState::Swap; SwapT = 2.4f;
			SetFlash(FString::Printf(TEXT("Patea el %s"), *PlayerName(bDuelA)), TEXT("Cambien de lugar: mouse y WASD para patear, flechas y Enter para atajar"), 0);
			Fire(TEXT("whistle"));
			return;
		}
		break;
	}
	default: break;
	}
	SetupPenalty();   // práctica: siempre vuelve al punto
}

void APPGameMode::RivalShot() { State = EPPState::Rival; RivalT = 0.f; bRivalDone = false; SetFlash(TEXT("Patea el rival"), TEXT("En el otro arco…"), 0); }

void APPGameMode::StepRival(float Dt)
{
	RivalT += Dt;
	if (!bRivalDone && RivalT > 1.5f)
	{
		bRivalDone = true;
		const bool bGol = FMath::FRand() < Gk->Diff.RivalGoalP;
		Rival.Add(bGol);
		if (bGol)
		{
			static const TCHAR* Msg[4] = { TEXT("La clavó arriba"), TEXT("Engañó a tu arquero"), TEXT("Arrastre al palo"), TEXT("La picó") };
			SetFlash(TEXT("Gol del rival"), Msg[FMath::RandRange(0, 3)], 2); Fire(TEXT("groan"));
		}
		else
		{
			const float R = FMath::FRand();
			SetFlash(R < .6f ? TEXT("¡Atajó tu arquero!") : R < .8f ? TEXT("¡Palo del rival!") : TEXT("¡Afuera!"), R < .6f ? TEXT("Seguimos") : R < .8f ? TEXT("Se salvó tu equipo") : TEXT("La tiró desviada"), 1); Fire(TEXT("cheer"));
		}
	}
	if (bRivalDone && RivalT > 3.3f) NextTurn();
}

void APPGameMode::EndMatch()
{
	State = EPPState::End; bAiActive = false;
	const int32 G = GoalsOf(Results);
	auto C = [&](EPPOutcome K) { int32 N = 0; for (EPPOutcome O : Results) if (O == K) N++; return N; };
	FString Big, Small; int32 Cls = 1;
	switch (Format)
	{
	case EPPFormat::Tanda:
	{
		const int32 B = GoalsOfRival(); const bool bSd = Results.Num() > 5;
		Big = FString::Printf(TEXT("%d a %d"), G, B);
		Small = G > B ? (bSd ? TEXT("Ganaste en la muerte súbita. Nervios de acero.") : TEXT("Ganaste la tanda. El equipo te sale a abrazar."))
		              : (bSd ? TEXT("Perdiste en la muerte súbita. Por un tiro.") : TEXT("Perdiste la tanda. Toca revancha."));
		Cls = G > B ? 1 : 2; break;
	}
	case EPPFormat::Arquero:
	{
		const int32 Sv = 5 - G;
		static const TCHAR* M[6] = { TEXT("Te hicieron todos. Toca revancha bajo los palos."), TEXT("Una atajada. El rival ganó el duelo."), TEXT("Dos atajadas. Se te escaparon varias."), TEXT("Tres de cinco: sostuviste al equipo."), TEXT("Cuatro atajadas. Sos la figura del partido."), TEXT("Cinco de cinco. Valla invicta.") };
		Big = FString::Printf(TEXT("%d de 5 atajadas"), Sv); Small = M[FMath::Clamp(Sv, 0, 5)]; Cls = Sv >= 3 ? 1 : 2; break;
	}
	case EPPFormat::Desafio:
	{
		int32 St = 0; for (int32 S : ChStars) St += S;
		Big = FString::Printf(TEXT("%d de 21 estrellas"), St);
		Small = St >= 21 ? TEXT("Todos con tres estrellas. Pateás como un crack.") : St >= 14 ? TEXT("Muy bien. Probá de nuevo para sacar más estrellas.") : FString::Printf(TEXT("Los completaste en %d tiros. Con práctica salen más estrellas."), ChTotal);
		break;
	}
	case EPPFormat::Duelo:
	{
		const int32 Ga = GoalsOf(DuelA), Gb = GoalsOf(DuelB);
		Big = FString::Printf(TEXT("%d a %d"), Ga, Gb);
		Small = Ga > Gb ? TEXT("Ganó el Jugador 1. Revancha cambiando de lugar.") : TEXT("Ganó el Jugador 2. Revancha cambiando de lugar.");
		break;
	}
	default: Big = FString::Printf(TEXT("%d goles"), G); break;
	}
	SetFlash(Big, Small, Cls);
	Hint = FString::Printf(TEXT("Goles %d · Atajadas %d · Palos %d · Afuera %d · Tiro más fuerte: %d km/h · Enter: volver al menú"),
		G, C(EPPOutcome::Save), C(EPPOutcome::Post), C(EPPOutcome::Miss), BestKmh);
}

// ------------------------------------------------------------------ menú
void APPGameMode::MenuInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;
	auto Hit = [&](const FKey& K) { return PC->WasInputKeyJustPressed(K); };
	if (State == EPPState::End) { if (Hit(EKeys::Enter) || Hit(EKeys::SpaceBar)) ToMenu(); return; }
	if (Hit(EKeys::One)) Format = EPPFormat::Practica;
	if (Hit(EKeys::Two)) Format = EPPFormat::Tanda;
	if (Hit(EKeys::Three)) Format = EPPFormat::Arquero;
	if (Hit(EKeys::Four)) Format = EPPFormat::Desafio;
	if (Hit(EKeys::Five)) Format = EPPFormat::Duelo;
	if (Hit(EKeys::Six)) { ShotMode = ShotMode == EPPShotMode::Penal ? EPPShotMode::Libre : EPPShotMode::Penal; if (Shooter) { Shooter->Place(StartPos()); BallToShooter(); } }
	if (Hit(EKeys::Seven)) Difficulty = EPPDifficulty((int32(Difficulty) + 1) % 3);
	if (Hit(EKeys::Eight)) GoalieStyle = EPPGKStyle((int32(GoalieStyle) + 1) % 4);
	if (Hit(EKeys::Nine)) CycleQuality();
	if (Hit(EKeys::Enter) || Hit(EKeys::SpaceBar) || Hit(EKeys::Gamepad_Special_Right)) StartMatch();
}

// ------------------------------------------------------------------ pasos por estado
void APPGameMode::StepAim(float Dt)
{
	// amague lateral del palo
	DekeX = 0.f;
	if (Shooter->DekeT > 0.f) { Shooter->DekeT = FMath::Max(0.f, Shooter->DekeT - Dt); DekeX = Shooter->DekeDir * .6f * FMath::Sin(PI * (1.f - Shooter->DekeT / DekeDuration)); }
	// conducción: la bocha va de un lado al otro de la pala al patinar
	Shooter->DribA = FMath::Lerp(Shooter->DribA, FMath::Clamp(Shooter->Speed / 3.5f, 0.f, 1.f) * .34f + .05f, PowDt(.05f, Dt));
	Shooter->DribP += Dt * (4.4f + Shooter->Speed * .95f);
	Shooter->DribVal = Shooter->DribA * FMath::Sin(Shooter->DribP);
	FPPPose Carry = APPShooter::Rest();
	Carry.Tw = Shooter->Sweep + Shooter->DribVal;
	Carry.Ln = APPShooter::Rest().Ln - .1f * Shooter->Amp - FMath::Min(.08f, Shooter->Speed * .01f);
	Shooter->SetPose(FPPPose::Lerp(Shooter->CurPose, Carry, PowDt(.00005f, Dt)));
	// la bocha va pegada a la pala y rueda con ella
	const FPPVec Prev = Ball->Pos;
	const FPPVec Next = Shooter->BallWorld();
	Ball->Pos = Next;
	Ball->RollBy(Next - Prev);
}

void APPGameMode::StepCharge(float Dt)
{
	Shooter->ChargeT += Dt;
	if (!Shooter->bPullHeld && !bAiActive) Shooter->Power = FMath::Min(1.f, Shooter->Power + Dt * .85f);   // teclado / joystick: carga por tiempo
	Shooter->PowerVis = FMath::Lerp(Shooter->PowerVis, Shooter->Power, PowDt(.0004f, Dt));
	Shooter->SetPose(FPPPose::Lerp(Shooter->ContactPose, APPShooter::Back(), Smooth(Shooter->PowerVis)));
	const FPPVec Prev = Ball->Pos; Ball->Pos = Shooter->BallAtPress(); Ball->RollBy(Ball->Pos - Prev);
}

void APPGameMode::StepSwing(float Dt)
{
	Shooter->SwingT += Dt;
	Shooter->SetPose(Shooter->SwingPose(Shooter->SwingT, Shooter->Power));
	if (!bLaunched)
	{
		const FPPVec Prev = Ball->Pos; Ball->Pos = Shooter->BallAtPress(); Ball->RollBy(Ball->Pos - Prev);
		if (Shooter->SwingT >= Shooter->Contact) { bLaunched = true; Launch(); }
	}
}

void APPGameMode::StepFlight(float Dt)
{
	if (Shooter->SwingT < Shooter->SwingEnd) { Shooter->SwingT += Dt; Shooter->SetPose(Shooter->SwingPose(Shooter->SwingT, Shooter->Power)); }
	else { FPPPose Carry = APPShooter::Rest(); Shooter->SetPose(FPPPose::Lerp(Shooter->CurPose, Carry, PowDt(.08f, Dt))); }
	StepBall(Dt);
	if (Shot.Outcome == EPPOutcome::None && (Shot.T > 4.f || (Shot.T > .4f && Ball->Vel.Size() < .25f)))
		Settle(EPPOutcome::Miss, Ball->Pos.Z > .5f ? TEXT("Se quedó corta") : TEXT("Se fue desviada"));
	if (Shot.Outcome != EPPOutcome::None && State == EPPState::Flight)
	{
		ResultTimer -= Dt;
		if (ResultTimer <= 0.f)
		{
			const bool bWant = (Format == EPPFormat::Desafio) ? Shot.bChDone : (Shot.Outcome == (Format == EPPFormat::Arquero ? EPPOutcome::Save : EPPOutcome::Goal));
			if (!(bWant && StartReplay())) NextTurn();
		}
	}
}

void APPGameMode::StepLoose(float Dt)
{
	LooseT += Dt;
	if (Shooter->SwingT < Shooter->SwingEnd) { Shooter->SwingT += Dt; Shooter->SetPose(Shooter->SwingPose(Shooter->SwingT, Shooter->Power)); }
	else Shooter->SetPose(FPPPose::Lerp(Shooter->CurPose, APPShooter::Rest(), PowDt(.02f, Dt)));
	// si el arquero la atrapó, a los segundos la devuelve rodando hacia un costado
	if (Shot.bCaught)
	{
		Shot.HoldT += Dt;
		if (Shot.HoldT > 1.4f)
		{
			Shot.bCaught = false; Shot.NoCatch = 1.5f;
			Ball->Pos = Gk->GlovePos(); Ball->Pos.Y = FMath::Max(BallR, Ball->Pos.Y * .3f);
			const float Sd = FMath::FRand() < .5f ? -1.f : 1.f;
			Ball->Vel = FPPVec(Sd * (2.2f + FMath::FRand() * 1.5f), 0.f, 3.f + FMath::FRand() * 1.5f);
			SetFlash(TEXT("El arquero la devuelve"), TEXT("¡Andá a buscarla!"), 0);
		}
		else { StepBall(Dt); return; }
	}
	if (Shot.NoCatch > 0.f) Shot.NoCatch -= Dt;
	StepBall(Dt);
	const FPPVec B = Ball->Pos; const float Sp = Ball->Vel.Size();
	// si la bocha cruza la línea entre los palos después del rebote, es gol
	if (Shot.PrevZ > 0.f && B.Z <= 0.f && FMath::Abs(B.X) < PostHalf - BallR && B.Y < BarH - BallR && Shot.Outcome != EPPOutcome::Goal)
	{
		Shot.bInGoal = true; Shot.Outcome = EPPOutcome::Goal; Shot.GoalT = Time; ResultTimer = 2.6f;
		if (Results.Num()) Results.Last() = EPPOutcome::Goal;
		SetFlash(TEXT("¡Gol!"), TEXT("Entró después del rebote"), 1); Fire(TEXT("goal"));
	}
	Shot.PrevZ = B.Z;
	if (Shot.Outcome == EPPOutcome::Goal) { ResultTimer -= Dt; if (ResultTimer <= 0.f && !StartReplay()) SetupPenalty(); return; }
	const bool bInNet = B.Z < 0.f && B.Z > -1.f && FMath::Abs(B.X) < PostHalf + .05f;
	// el arquero se tira encima de la bocha si queda cerca y lenta
	if (!bInNet && !(Shot.NoCatch > 0.f) && Sp < 1.6f && B.Y < .2f && FMath::Abs(B.X - Gk->Pos.X) < .5f && B.Z < Gk->Pos.Y + .5f && B.Z > -.05f)
	{ Ball->Vel = FPPVec::ZeroVector; Shot.bCaught = true; Shot.HoldT = 0.f; SetFlash(TEXT("¡Atajó!"), TEXT("Se tiró encima del rebote"), 2); Fire(TEXT("save")); return; }
	// al pasar cerca de la bocha el jugador la recupera al instante
	if (Shot.NoPick > 0.f) Shot.NoPick -= Dt;
	const FPPVec M = Shooter->BallWorld();
	if (!(Shot.NoPick > 0.f) && !bInNet && B.Y < .32f && (Hypot(M.X - B.X, M.Z - B.Z) < .4f || Hypot(Shooter->GamePos.X - B.X, Shooter->GamePos.Z - B.Z) < .85f))
	{
		ReboundCount++; State = EPPState::Aim; bHasShot = false; bLaunched = false;
		Ball->Vel = FPPVec::ZeroVector; Ball->Spin = FPPVec::ZeroVector;
		SetFlash(TEXT("¡La tenés!"), TEXT("Tirá de nuevo · R para volver al punto"), 1, .9f);
	}
}

// ------------------------------------------------------------------ rival de la máquina (Atajá vos)
FPPAiPlan APPGameMode::MakePlan() const
{
	struct FL { float P0, P1, Run, Deke, Hi; };
	FL L{ .55f, .85f, .5f, .35f, .45f };
	if (Difficulty == EPPDifficulty::Infantiles) L = FL{ .35f, .6f, .25f, .1f, .3f };
	else if (Difficulty == EPPDifficulty::Mundial) L = FL{ .72f, 1.f, .75f, .6f, .55f };
	FPPAiPlan P;
	const float R = FMath::FRand();
	P.Kind = R < .2f ? EPPShotKind::Arrastre : R < .3f ? EPPShotKind::Picada : R < .45f ? EPPShotKind::Fuetazo : R < .55f ? EPPShotKind::Reves : EPPShotKind::Golpe;
	const float Side = FMath::FRand() < .5f ? -1.f : 1.f;
	const bool bHi = FMath::FRand() < L.Hi;
	P.bRun = FMath::FRand() < L.Run;
	const float StartZ = StartPos().Z;
	P.Wait = .9f + FMath::FRand() * 1.3f;
	P.StopZ = P.bRun ? StartZ - (1.f + FMath::FRand() * (ShotMode == EPPShotMode::Libre ? 3.f : 1.6f)) : StartZ;
	P.bDeke = FMath::FRand() < L.Deke; P.DekeAt = .4f + FMath::FRand() * .5f; P.Weave = FMath::FRand() * 6.f;
	P.Tx = Side * (.3f + FMath::FRand() * .5f);
	P.Ty = P.Kind == EPPShotKind::Picada ? .85f + FMath::FRand() * .15f : bHi ? .65f + FMath::FRand() * .3f : .08f + FMath::FRand() * .35f;
	P.Power = L.P0 + FMath::FRand() * (L.P1 - L.P0);
	P.bCurve = FMath::FRand() < .25f; P.bFake = FMath::FRand() < L.Deke * .7f;
	return P;
}

void APPGameMode::StepAI(float Dt)
{
	AiT += Dt; AiMove = FPPVec2::ZeroVector;
	FPPAiPlan& Pl = Ai;
	const FPPVec P = Shooter->GamePos;
	if (State == EPPState::Aim)
	{
		Shooter->SweepT = FMath::Sin(AiT * 2.2f + Pl.Weave) * .25f;
		if (AiT < Pl.Wait) return;
		if (Pl.bRun && P.Z > Pl.StopZ)
		{
			AiMove.Y = 1.f; AiMove.X = FMath::Sin(AiT * 2.6f + Pl.Weave) * .55f;
			if (Pl.bDeke && !Pl.bDekeDone && (StartPos().Z - P.Z) > (StartPos().Z - Pl.StopZ) * Pl.DekeAt)
			{ Pl.bDekeDone = true; Shooter->DekeDir = FMath::FRand() < .5f ? 1 : -1; Shooter->DekeT = DekeDuration; Shooter->LastDekeTime = Time; }
			return;
		}
		// si el arquero se tiró antes de tiempo, espera un instante y la pone arriba del otro lado
		if (Gk->In.bDown && !Pl.bPunished)
		{
			Pl.bPunished = true; Pl.HoldT = .3f + FMath::FRand() * .25f; Pl.Tx = (Gk->Pos.X > 0.f ? -1.f : 1.f) * (.55f + FMath::FRand() * .2f); Pl.Ty = .72f + FMath::FRand() * .2f;
			if (Pl.Kind == EPPShotKind::Arrastre || Pl.Kind == EPPShotKind::Reves) Pl.Kind = EPPShotKind::Golpe;
		}
		if (Pl.HoldT > 0.f) { Pl.HoldT -= Dt; return; }
		if (!Pl.bLobChecked && Gk->Pos.Y > 1.6f)
		{
			Pl.bLobChecked = true;
			const float R = FMath::FRand();
			if (R < .4f) { Pl.Kind = EPPShotKind::Picada; Pl.Ty = .9f; Pl.Tx = (Gk->Pos.X > 0.f ? -1.f : 1.f) * (.3f + FMath::FRand() * .4f); }
			else if (R < .85f && Hypot(P.X - Gk->Pos.X, P.Z - Gk->Pos.Y) < 3.2f)
			{
				Pl.bDribble = true; Pl.DribT = 0.f; Pl.DribDir = (P.X >= Gk->Pos.X ? 1.f : -1.f) * (FMath::FRand() < .3f ? -1.f : 1.f);
				Shooter->DekeDir = Pl.DribDir > 0.f ? -1 : 1; Shooter->DekeT = DekeDuration; Shooter->LastDekeTime = Time;
			}
		}
		// gambeta: se abre hacia un costado del arquero y define con el arco libre
		if (Pl.bDribble)
		{
			Pl.DribT += Dt; AiMove.X = Pl.DribDir; AiMove.Y = .55f; Shooter->SweepT = -Pl.DribDir * .3f;
			const bool bPassed = FMath::Abs(P.X - Gk->Pos.X) > 1.05f || P.Z < Gk->Pos.Y + .5f;
			if (!(bPassed || Pl.DribT > 1.1f)) return;
			Pl.bDribble = false; Pl.Kind = FMath::FRand() < .5f ? EPPShotKind::Arrastre : EPPShotKind::Golpe; Pl.bCurve = false; Pl.Power = .5f;
			const float V = Gk->Pos.X - P.X * .3f; const float Sg = V >= 0.f ? 1.f : -1.f;
			Pl.Tx = FMath::Clamp(-Sg * (.45f + FMath::FRand() * .25f), -.75f, .75f); Pl.Ty = .15f + FMath::FRand() * .3f;
			if (FMath::Abs(Gk->Pos.X - Pl.Tx) < .5f) Pl.Tx = -Pl.Tx;
		}
		Shooter->Aim = FPPVec(Pl.Tx, Pl.Ty, 0.f);
		switch (Pl.Kind)
		{
		case EPPShotKind::Arrastre: PushShot(); break;
		case EPPShotKind::Fuetazo: WristShot(); break;
		case EPPShotKind::Reves: BackhandShot(); break;
		default: StartCharge(); break;
		}
		return;
	}
	if (State == EPPState::Charge)
	{
		// amague de tiro: levanta el palo y no le pega, para que el arquero se tire
		if (Pl.bFake && !Pl.bFakeDone && Shooter->ChargeT > .22f) { Pl.bFakeDone = true; State = EPPState::Aim; Shooter->Power = 0.f; AiT = Pl.Wait - .55f; return; }
		if (Gk->In.bDown && !Pl.bPunished) { Pl.bPunished = true; Pl.Tx = (Gk->Pos.X > 0.f ? -1.f : 1.f) * (.55f + FMath::FRand() * .2f); Pl.Ty = .75f + FMath::FRand() * .2f; }
		Shooter->Aim = FPPVec(Pl.Tx, Pl.Ty, 0.f);
		Shooter->Power = FMath::Min(Pl.Power, Shooter->Power + Dt * 2.4f);
		if (Shooter->Power >= Pl.Power && Shooter->ChargeT > .3f) Release(Pl.Kind == EPPShotKind::Picada, Pl.bCurve && Pl.Kind == EPPShotKind::Golpe);
	}
}

// ------------------------------------------------------------------ arquero manejado por una persona
void APPGameMode::GoalieInput(float Dt)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;
	FPPGkInput& In = Gk->In;
	if (Format == EPPFormat::Arquero)
	{
		FVector O, D;
		if (PC->DeprojectMousePositionToWorld(O, D))
		{
			const double Xp = -(Gk->Pos.Y + .3) * 100.0;              // plano de las manos (z de juego -> X de Unreal)
			if (FMath::Abs(D.X) > 1e-4) { const double T = (Xp - O.X) / D.X; if (T > 0.0) { const FVector H = O + D * T; In.X = FMath::Clamp(float(H.Y / 100.0), -4.6f, 4.6f); In.Y = FMath::Clamp(float(H.Z / 100.0), .05f, 1.4f); } }
			if (FMath::Abs(D.Z) > 1e-4) { const double T = (0.0 - O.Z) / D.Z; if (T > 0.0) { const FVector H = O + D * T; In.Fx = float(H.Y / 100.0); In.Fz = float(-H.X / 100.0); } }
		}
		In.bDown = PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::SpaceBar);
		In.bOut = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);
		if (PC->WasInputKeyJustPressed(EKeys::RightMouseButton) || PC->WasInputKeyJustPressed(EKeys::E)) Gk->TriggerPoke();
	}
	else if (Format == EPPFormat::Duelo)
	{
		const float H = (PC->IsInputKeyDown(EKeys::Right) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::Left) ? 1.f : 0.f);
		const float V = (PC->IsInputKeyDown(EKeys::Up) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::Down) ? 1.f : 0.f);
		if (PC->IsInputKeyDown(EKeys::RightShift))
		{
			In.Fx = FMath::Clamp(In.Fx + H * 2.8f * Dt, -4.4f, 4.4f); In.Fz = FMath::Clamp(In.Fz + V * 2.8f * Dt, .42f, 5.3f); In.X = Gk->Pos.X + H * .4f;
		}
		else
		{
			In.X = FMath::Clamp(In.X + H * 1.7f * Dt, Gk->Pos.X - 1.1f, Gk->Pos.X + 1.1f);
			In.Y = FMath::Clamp(In.Y + V * 1.5f * Dt, .05f, 1.3f);
		}
		In.bDown = PC->IsInputKeyDown(EKeys::Enter);
		In.bOut = PC->IsInputKeyDown(EKeys::RightShift);
		if (PC->WasInputKeyJustPressed(EKeys::RightControl)) Gk->TriggerPoke();
	}
}

// ------------------------------------------------------------------ repetición del gol
void APPGameMode::Record()
{
	if (State != EPPState::Aim && State != EPPState::Charge && State != EPPState::Swing && State != EPPState::Flight && State != EPPState::Loose) return;
	FPPFrame F;
	F.T = Time; F.SPos = Shooter->GamePos; F.SHead = Shooter->Heading; F.SAmp = Shooter->Amp; F.SPhase = Shooter->StridePhase; F.SLean = Shooter->LeanRoll; F.SBrake = Shooter->Brake; F.SSweep = Shooter->Sweep; F.SPin = Shooter->LastPin;
	F.Pose = Shooter->CurPose;
	F.BallPos = Ball->Pos; F.BallQ = Ball->GetActorQuat(); F.bHide = Ball->IsHidden();
	F.GPos = Gk->Pos; F.GRoll = Gk->Roll; F.GYaw = Gk->Yaw; F.GKneel = Gk->Kneel; F.GPlay = Gk->Play; F.GGlove = Gk->GloveLocal; F.GBlock = Gk->BlockerLocal; F.GPad = Gk->PadExt;
	Rec.Add(F);
	if (Rec.Num() > 420) Rec.RemoveAt(0);
}

bool APPGameMode::StartReplay()
{
	if (Rec.Num() < 10 || !bHasShot || Shot.LaunchT <= 0.f) return false;
	const float From = Shot.LaunchT - 1.1f, To = FMath::Max(Shot.LaunchT + 1.7f, Shot.GoalT + .9f);
	Rp.Frames.Reset();
	for (const FPPFrame& F : Rec) if (F.T >= From && F.T <= To) Rp.Frames.Add(F);
	if (Rp.Frames.Num() < 10) return false;
	Rp.T0 = Rp.Frames[0].T; Rp.T1 = Rp.Frames.Last().T; Rp.Tt = Rp.T0; Rp.Speed = .38f;
	State = EPPState::Replay; ClearFlash();
	Shooter->bCamInit = false;
	return true;
}

void APPGameMode::StepReplay(float Dt)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC && (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Enter) || PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))) Rp.Tt = Rp.T1 + 1.f;
	Rp.Tt += Dt * Rp.Speed;
	if (Rp.Tt >= Rp.T1 + .25f) { Shooter->bCamInit = false; NextTurn(); return; }
	int32 I = 0;
	for (int32 k = 0; k < Rp.Frames.Num(); k++) { if (Rp.Frames[k].T > Rp.Tt) break; I = k; }
	const FPPFrame& F = Rp.Frames[I];
	Shooter->GamePos = F.SPos; Shooter->Heading = F.SHead; Shooter->Amp = F.SAmp; Shooter->StridePhase = F.SPhase; Shooter->LeanRoll = F.SLean; Shooter->Brake = F.SBrake; Shooter->Sweep = F.SSweep;
	Shooter->CurPose = F.Pose; Shooter->ReplayPin = F.SPin;
	Ball->Pos = F.BallPos; Ball->SetActorRotation(F.BallQ); Ball->SetActorHiddenInGame(F.bHide); Ball->SyncActor();
	Shooter->ApplyVisual(0.f, EPPState::Replay);
	Gk->Pos = F.GPos; Gk->Roll = F.GRoll; Gk->Yaw = F.GYaw; Gk->Kneel = F.GKneel; Gk->Play = F.GPlay; Gk->GloveLocal = F.GGlove; Gk->BlockerLocal = F.GBlock; Gk->PadExt = F.GPad;
	Gk->ApplyVisual();
	Shooter->CamReplay(Dt, F.BallPos);
}

// ------------------------------------------------------------------ bucle principal
void APPGameMode::Tick(float DtRaw)
{
	Super::Tick(DtRaw);
	ResolveActors();
	if (!Shooter || !Ball || !Gk) return;
	const float Dt = FMath::Clamp(DtRaw, .001f, .05f);
	if (FlashT > 0.f) { FlashT -= Dt; if (FlashT <= 0.f) ClearFlash(); }
	if (State == EPPState::Menu || State == EPPState::End) MenuInput();
	if (bPaused) { Shooter->CamUpdate(Dt, State, Ball->Pos, Gk->Pos.X); return; }
	Time += Dt;
	if (State == EPPState::Replay) { StepReplay(Dt); return; }
	if (bHasShot && State == EPPState::Flight) Shot.T += Dt;

	const bool bActive = State != EPPState::Menu && State != EPPState::End;
	if (bActive && Gk->bUser) GoalieInput(Dt);
	if (bActive && bAiActive) StepAI(Dt);
	const bool bCanMove = State == EPPState::Aim || State == EPPState::Charge || State == EPPState::Flight || State == EPPState::Loose;
	if (bActive) Shooter->SkateStep(Dt, State, bCanMove, bAiActive ? &AiMove : nullptr);
	// barrido del palo (mouse) + amague
	Shooter->Sweep = FMath::Lerp(Shooter->Sweep, Shooter->SweepT + DekeX, PowDt(Shooter->DekeT > 0.f ? .00001f : .0005f, Dt));

	switch (State)
	{
	case EPPState::Aim: StepAim(Dt); break;
	case EPPState::Charge: StepCharge(Dt); break;
	case EPPState::Swing: StepSwing(Dt); break;
	case EPPState::Flight: StepFlight(Dt); break;
	case EPPState::Loose: StepLoose(Dt); break;
	case EPPState::Rival: StepRival(Dt); break;
	case EPPState::Swap: SwapT -= Dt; if (SwapT <= 0.f) SetupPenalty(); break;
	default: break;
	}

	// arquero
	if (Gk->bUser)
	{
		if (!Gk->bInPlayLatched && State == EPPState::Aim && FPPVec::Dist(Shooter->GamePos, StartPos()) > .3f) Gk->bInPlayLatched = true;
		Gk->Play = FMath::Lerp(Gk->Play, Gk->bInPlayLatched ? 1.f : 0.f, PowDt(.0005f, Dt));
		Gk->UserUpdate(Dt, Time, State, Ball, Format == EPPFormat::Duelo);
	}
	else Gk->Update(Dt, Time, bHasShot ? &Shot : nullptr, bLaunched, State, Ball, Shooter, StartPos());
	if (bActive && Gk->PokeAlpha > .1f && Stealable() && Gk->StickHits(Ball->Pos)) DoSteal(Gk->bUser);

	Record();
	Shooter->ApplyVisual(Dt, State);
	Shooter->CamUpdate(Dt, State, Ball->Pos, Gk->Pos.X);
	Ball->SyncActor();
}
