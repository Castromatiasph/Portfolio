#include "PPGameMode.h"
#include "PPBall.h"
#include "PPGoalkeeper.h"
#include "PPShooter.h"
#include "PPHUD.h"
#include "PPArena.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

using namespace PP;

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
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	bool bHasArena = false;
	for (TActorIterator<APPArena> It(W); It; ++It) { bHasArena = true; break; }
	if (!bHasArena) W->SpawnActor<APPArena>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (!Ball) Ball = W->SpawnActor<APPBall>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (!Gk) Gk = W->SpawnActor<APPGoalkeeper>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture);
		PC->SetInputMode(Mode);
	}
	State = EPPState::Menu;
	Hint = TEXT("1 Práctica · 2 Tanda   3 Penal · 4 Libre   5 Infantiles · 6 Primera · 7 Mundial   8 Estilo del arquero   Enter: jugar");
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

void APPGameMode::Fire(const TCHAR* Name, float Param, const FPPVec* G)
{
	OnPPEvent(FName(Name), Param, G ? ToUE(*G) : (Ball ? ToUE(Ball->Pos) : FVector::ZeroVector));
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

void APPGameMode::SetFlash(const FString& Big, const FString& Small, int32 Cls) { FlashBig = Big; FlashSmall = Small; FlashClass = Cls; }

// ------------------------------------------------------------------ partido
void APPGameMode::StartMatch()
{
	ResolveActors();
	Results.Reset(); Rival.Reset(); BestKmh = 0; bPaused = false;
	Gk->StyleSel = GoalieStyle; Gk->PickStyle(); Gk->Diff = FPPDiff::Make(Difficulty);
	SetupPenalty();
}

void APPGameMode::ToMenu()
{
	State = EPPState::Menu; ClearFlash();
	if (Shooter) { Shooter->Place(StartPos()); BallToShooter(); }
	Hint = TEXT("1 Práctica · 2 Tanda   3 Penal · 4 Libre   5 Infantiles · 6 Primera · 7 Mundial   8 Estilo del arquero   Enter: jugar");
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
	ReboundCount = 0; bHasShot = false; bLaunched = false; ResultTimer = 0.f; LooseT = 0.f;
	Gk->StyleSel = GoalieStyle; Gk->Diff = FPPDiff::Make(Difficulty);
	Gk->ResetForShot(true);
	Shooter->Place(StartPos());
	BallToShooter();
	Shooter->Power = 0.f; Shooter->ChargeT = 0.f;
	State = EPPState::Aim;
	ClearFlash();
	Hint = TEXT("WASD patina · Q/E amagues · clic y tirar el mouse para atrás: golpe (Shift al soltar: picada, Ctrl: efecto) · clic derecho/F arrastre · V fuetazo · B revés · C cámara · R vuelve al punto");
	Fire(TEXT("whistle"));
}

void APPGameMode::ResetToSpot()
{
	if (Format == EPPFormat::Practica && State != EPPState::Menu && State != EPPState::End && State != EPPState::Rival) SetupPenalty();
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
	if (State == EPPState::Menu || State == EPPState::End) return;
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

	// el arquero lee el tiro
	const float Frac = (S.Z - Gk->PlaneZ()) / Dz;
	const float SinceDeke = Time - Shooter->LastDekeTime;
	const float Read = (.6f + .8f * P) * (K == EPPShotKind::Arrastre ? 1.6f : K == EPPShotKind::Fuetazo ? 1.45f : K == EPPShotKind::Reves ? 1.5f : 1.f)
		* (SinceDeke < .7f ? 1.7f : 1.f) * FMath::Clamp(1.25f - Dist / 20.f, .7f, 1.2f);
	const FPPGKStyle GS = FPPGKStyle::Make(Gk->Style);
	float PredX = (CurveA != 0.f ? S.X + Ball->Vel.X * Tt * Frac : S.X + (Tx - S.X) * Frac) + Gauss() * D.Noise * Read;
	float PredY = Ty + Gauss() * D.Noise * .8f * Read;
	float React = D.React * (.85f + FMath::FRand() * .3f) * (K == EPPShotKind::Arrastre ? 1.45f : K == EPPShotKind::Fuetazo ? 1.2f : K == EPPShotKind::Reves ? 1.25f : 1.f);
	React *= GS.React;
	PredX += Gauss() * D.Noise * (GS.Noise - 1.f);
	// el que se tira temprano ya eligió un lado cuando el atacante levantó el palo
	if (GS.bEarly && Gk->EarlySide && FMath::FRand() < .6f) { PredX = Gk->EarlySide * (.4f + FMath::FRand() * .3f); PredY = .15f + FMath::FRand() * .4f; React = .02f; }
	else if (FMath::FRand() < FMath::Max(0.f, D.Guess + GS.Guess) * (SinceDeke < .7f ? 1.6f : 1.f))
	{ PredX = (FMath::FRand() < .5f ? -1.f : 1.f) * (.35f + FMath::FRand() * .4f); PredY = .15f + FMath::FRand() * .85f; React = .06f; }
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
void APPGameMode::Settle(EPPOutcome Kind, const FString& Detail)
{
	if (Shot.Outcome != EPPOutcome::None) return;
	Shot.Outcome = Kind;
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
	Shot.Outcome = Kind; State = EPPState::Flight; Results.Add(Kind); ResultTimer = 2.8f;
	const FString Kn = ShotName();
	switch (Kind)
	{
	case EPPOutcome::Goal: SetFlash(ReboundCount > 0 ? TEXT("¡Gol de rebote!") : TEXT("¡Gol!"), FString::Printf(TEXT("%s · %d km/h · %s"), *Kn, Shot.Kmh, *ZoneName(Shot.Tx, Shot.Ty)), 1); Fire(TEXT("goal")); Shooter->Shake = 1.f; break;
	case EPPOutcome::Save: SetFlash(TEXT("¡Atajó!"), Shot.bCaught ? FString(TEXT("Se la quedó con el guante")) : FString::Printf(TEXT("La sacó con %s"), *Detail), 2); Fire(TEXT("save")); Fire(TEXT("groan")); break;
	case EPPOutcome::Post: SetFlash(TEXT("¡Palo!"), Detail, 2); Fire(TEXT("groan")); break;
	default: SetFlash(TEXT("Afuera"), Detail, 2); Fire(TEXT("groan")); break;
	}
}

void APPGameMode::DoSteal()
{
	bLaunched = true;
	FPPVec Dir = Ball->Pos - FPPVec(Gk->Pos.X, 0.f, Gk->Pos.Y); Dir.Y = 0.f; Dir = Dir.GetSafeNormal();
	const float Sp = 3.2f + FMath::FRand() * 1.6f;
	Shot = FPPShot();
	Shot.Kmh = FMath::RoundToInt(Sp * 3.6f); Shot.bGkChecked = true; Shot.bLineChecked = true; Shot.NoPick = .8f;
	bHasShot = true;
	Ball->Vel = FPPVec(Dir.X * Sp + (FMath::FRand() - .5f) * .8f, .15f, FMath::Max(.6f, Dir.Z) * Sp);
	Ball->Spin = FPPVec::ZeroVector;
	Gk->PokeAlpha = 0.f;
	Shot.Outcome = EPPOutcome::Save; Results.Add(EPPOutcome::Save);
	Fire(TEXT("save"));
	if (Format == EPPFormat::Practica)
	{
		State = EPPState::Loose; LooseT = 0.f; Shot.PrevZ = Ball->Pos.Z;
		SetFlash(TEXT("¡Te la sacó!"), TEXT("El arquero salió con el palo. ¡Andá a buscarla!"), 2);
		return;
	}
	State = EPPState::Flight; ResultTimer = 2.6f;
	SetFlash(TEXT("¡Te la sacó!"), TEXT("El arquero salió con el palo"), 2); Fire(TEXT("groan"));
}

// ------------------------------------------------------------------ tanda y fin
bool APPGameMode::Decided() const
{
	const int32 N = Results.Num(), M = Rival.Num(), A = GoalsOf(Results), B = GoalsOfRival();
	if (N <= 5 && M <= 5) { if (A + (5 - N) < B || B + (5 - M) < A) return true; if (N < 5 || M < 5) return false; return A != B; }
	return N == M && A != B;
}

void APPGameMode::NextTurn()
{
	if (Format == EPPFormat::Tanda)
	{
		if (Decided()) { EndMatch(); return; }
		if (Rival.Num() < Results.Num()) { RivalShot(); return; }
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
	State = EPPState::End;
	const int32 G = GoalsOf(Results);
	auto C = [&](EPPOutcome K) { int32 N = 0; for (EPPOutcome O : Results) if (O == K) N++; return N; };
	if (Format == EPPFormat::Tanda)
	{
		const int32 B = GoalsOfRival(); const bool bSd = Results.Num() > 5;
		SetFlash(FString::Printf(TEXT("%d a %d"), G, B),
			G > B ? (bSd ? TEXT("Ganaste en la muerte súbita. Nervios de acero.") : TEXT("Ganaste la tanda. El equipo te sale a abrazar."))
			      : (bSd ? TEXT("Perdiste en la muerte súbita. Por un tiro.") : TEXT("Perdiste la tanda. Toca revancha.")), G > B ? 1 : 2);
	}
	else SetFlash(FString::Printf(TEXT("%d goles"), G), TEXT(""), 1);
	Hint = FString::Printf(TEXT("Goles %d · Atajadas %d · Palos %d · Afuera %d%s · Tiro más fuerte: %d km/h · Enter: volver al menú"),
		G, C(EPPOutcome::Save), C(EPPOutcome::Post), C(EPPOutcome::Miss), TEXT(""), BestKmh);
}

// ------------------------------------------------------------------ menú
void APPGameMode::MenuInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;
	auto Hit = [&](const FKey& K) { return PC->WasInputKeyJustPressed(K); };
	if (Hit(EKeys::One)) Format = EPPFormat::Practica;
	if (Hit(EKeys::Two)) Format = EPPFormat::Tanda;
	if (Hit(EKeys::Three)) { ShotMode = EPPShotMode::Penal; if (Shooter) { Shooter->Place(StartPos()); BallToShooter(); } }
	if (Hit(EKeys::Four)) { ShotMode = EPPShotMode::Libre; if (Shooter) { Shooter->Place(StartPos()); BallToShooter(); } }
	if (Hit(EKeys::Five)) Difficulty = EPPDifficulty::Infantiles;
	if (Hit(EKeys::Six)) Difficulty = EPPDifficulty::Primera;
	if (Hit(EKeys::Seven)) Difficulty = EPPDifficulty::Mundial;
	if (Hit(EKeys::Eight)) GoalieStyle = EPPGKStyle((int32(GoalieStyle) + 1) % 4);
	if (State == EPPState::End) { if (Hit(EKeys::Enter) || Hit(EKeys::SpaceBar)) ToMenu(); return; }
	if (Hit(EKeys::Enter) || Hit(EKeys::SpaceBar) || Hit(EKeys::Gamepad_Special_Right)) StartMatch();
}

// ------------------------------------------------------------------ bucle principal
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
	if (!Shooter->bPullHeld) Shooter->Power = FMath::Min(1.f, Shooter->Power + Dt * .85f);   // teclado / joystick: carga por tiempo
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
		if (ResultTimer <= 0.f) NextTurn();
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
		Shot.bInGoal = true; Shot.Outcome = EPPOutcome::Goal; ResultTimer = 2.6f;
		if (Results.Num()) Results.Last() = EPPOutcome::Goal;
		SetFlash(TEXT("¡Gol!"), TEXT("Entró después del rebote"), 1); Fire(TEXT("goal"));
	}
	Shot.PrevZ = B.Z;
	if (Shot.Outcome == EPPOutcome::Goal) { ResultTimer -= Dt; if (ResultTimer <= 0.f) SetupPenalty(); return; }
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
		SetFlash(TEXT("¡La tenés!"), TEXT("Tirá de nuevo · R para volver al punto"), 1);
	}
}

void APPGameMode::Tick(float DtRaw)
{
	Super::Tick(DtRaw);
	ResolveActors();
	if (!Shooter || !Ball || !Gk) return;
	const float Dt = FMath::Clamp(DtRaw, .001f, .05f);
	if (State == EPPState::Menu || State == EPPState::End) MenuInput();
	if (bPaused) { Shooter->CamUpdate(Dt, State, Ball->Pos); return; }
	Time += Dt;

	const bool bCanMove = State == EPPState::Aim || State == EPPState::Charge || State == EPPState::Flight || State == EPPState::Loose;
	if (State != EPPState::Menu && State != EPPState::End) Shooter->SkateStep(Dt, State, bCanMove);
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
	default: break;
	}
	if (State == EPPState::Flight || State == EPPState::Loose) { /* el palo vuelve a la pose de conducción en StepFlight/StepLoose */ }

	// arquero de la máquina
	const bool bActive = State != EPPState::Menu && State != EPPState::End;
	Gk->Update(Dt, Time, bHasShot ? &Shot : nullptr, bLaunched, State, Ball, Shooter, StartPos());
	if (bActive && Gk->PokeAlpha > .1f && Stealable() && Gk->StickHits(Ball->Pos)) DoSteal();

	Shooter->ApplyVisual(Dt, State);
	Shooter->CamUpdate(Dt, State, Ball->Pos);
	Ball->SyncActor();
}
