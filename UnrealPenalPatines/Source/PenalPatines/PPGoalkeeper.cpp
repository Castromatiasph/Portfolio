#include "PPGoalkeeper.h"
#include "PPBall.h"
#include "PPShooter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

// Tres posturas que se mezclan (GK_POSES del juego web):
// SQ = antes de que la bocha esté en juego · WT = bocha en juego · KN = reacción al tiro (de rodillas)
static const float HP = HALF_PI;
APPGoalkeeper::FPose APPGoalkeeper::PoseAt(float K, float W)
{
	static const FPose SQ = { {0,-.2f,.03f,.3f,0}, {{-.17f,.33f,.14f,0,0,HP},{.17f,.33f,.14f,0,0,-HP}}, {{-.17f,.62f,.17f},{.17f,.62f,.17f}}, {{-.13f,.33f,.08f},{.13f,.33f,.08f}}, FPPVec(.5f,.85f,.3f), FPPVec(-.42f,.2f,.46f) };
	static const FPose WT = { {0,-.1f,0,.2f,.15f}, {{-.33f,.15f,.05f,0,-.28f,0},{.4f,.33f,.08f,0,.15f,-.75f}}, {{-.12f,.15f,0},{.17f,.55f,.05f}}, {{-.13f,.42f,-.06f},{.13f,.42f,-.06f}}, FPPVec(.6f,1.08f,.25f), FPPVec(-.45f,.2f,.46f) };
	static const FPose KN = { {0,0,0,0,0}, {{-.33f,.15f,.03f,0,-.22f,0},{.33f,.15f,.03f,0,.22f,0}}, {{-.1f,.15f,0},{.1f,.15f,0}}, {{-.13f,.5f,-.06f},{.13f,.5f,-.06f}}, FPPVec(.52f,.62f,.2f), FPPVec(-.5f,.22f,.36f) };
	FPose R;
	auto M = [](float a, float b, float t) { return FMath::Lerp(a, b, t); };
	for (int i = 0; i < 5; i++) R.Up[i] = M(M(SQ.Up[i], WT.Up[i], W), KN.Up[i], K);
	for (int i = 0; i < 2; i++) for (int j = 0; j < 6; j++) R.Pads[i][j] = M(M(SQ.Pads[i][j], WT.Pads[i][j], W), KN.Pads[i][j], K);
	for (int i = 0; i < 2; i++) for (int j = 0; j < 3; j++) { R.Knee[i][j] = M(M(SQ.Knee[i][j], WT.Knee[i][j], W), KN.Knee[i][j], K); R.Hip[i][j] = M(M(SQ.Hip[i][j], WT.Hip[i][j], W), KN.Hip[i][j], K); }
	R.Glove = FMath::Lerp(FMath::Lerp(SQ.Glove, WT.Glove, W), KN.Glove, K);
	R.Block = FMath::Lerp(FMath::Lerp(SQ.Block, WT.Block, W), KN.Block, K);
	return R;
}

APPGoalkeeper::APPGoalkeeper()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	auto Make = [&](const TCHAR* Name, UStaticMesh* M)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (M) C->SetStaticMesh(M);
		return C;
	};
	UStaticMesh* CubeM = Cube.Succeeded() ? Cube.Object : nullptr;
	UStaticMesh* SphereM = Sphere.Succeeded() ? Sphere.Object : nullptr;
	Torso = Make(TEXT("Torso"), CubeM);
	Head = Make(TEXT("Head"), SphereM);
	Pads.Add(Make(TEXT("PadL"), CubeM));
	Pads.Add(Make(TEXT("PadR"), CubeM));
	Glove = Make(TEXT("Glove"), SphereM);
	Blocker = Make(TEXT("Blocker"), CubeM);
	Stick = Make(TEXT("Stick"), CubeM);

	BodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Root);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Diff = FPPDiff::Make(EPPDifficulty::Primera);
}

void APPGoalkeeper::PickStyle()
{
	if (StyleSel == EPPGKStyle::Variado)
	{
		const EPPGKStyle Opts[3] = { EPPGKStyle::Atrevido, EPPGKStyle::Linea, EPPGKStyle::Temprano };
		Style = Opts[FMath::RandRange(0, 2)];
	}
	else Style = StyleSel;
}

void APPGoalkeeper::ResetForShot(bool bPickStyle)
{
	Vel = FPPVec2::ZeroVector; Lean = 0.f; PokeAlpha = 0.f; PokeCd = 1.2f; EarlySide = 0; EarlyT = 0.f; bEarlyUsed = false;
	if (bPickStyle && (StyleSel == EPPGKStyle::Variado)) PickStyle();
	Pos = FPPVec2(0.f, 0.42f); Roll = 0.f; Yaw = 0.f; Kneel = 0.f; Play = 0.f; bInPlayLatched = false; BallVx = 0.f; LastBx = 0.f; PadExt = FPPVec2::ZeroVector;
	bHasH0 = false;
	const FPose P = PoseAt(0.f, 0.f);
	GloveLocal = P.Glove; BlockerLocal = P.Block;
	ApplyVisual();
}

void APPGoalkeeper::Move(FPPVec2 T, float VMax, float Dt, float Acc)
{
	if (Acc <= 0.f) Acc = 16.f;
	const FPPVec2 D = T - Pos;
	const float Dist = D.Size();
	const float Want = FMath::Min(VMax, FMath::Sqrt(2.f * Acc * .8f * Dist));
	const FPPVec2 W = Dist > 1e-4f ? D / Dist * Want : FPPVec2::ZeroVector;
	FPPVec2 A = W - Vel;
	const float Al = A.Size(), Lim = Acc * Dt;
	const float K = Al > Lim ? Lim / Al : 1.f;
	const float PVx = Vel.X;
	Vel += A * K;
	if (Dist < .012f && Vel.Size() < .12f) Vel *= .5f;   // se planta, sin temblequeo
	Pos += Vel * Dt;
	// se inclina hacia donde va y, al frenar, carga el peso sobre el patín de afuera
	const float Axw = (Vel.X - PVx) / FMath::Max(Dt, 1e-3f);
	AccX = FMath::Lerp(AccX, Axw, PP::PowDt(.002f, Dt));
	Lean = FMath::Lerp(Lean, FMath::Clamp(-Vel.X * .05f - AccX * .004f, -.14f, .14f), PP::PowDt(.01f, Dt));
}

void APPGoalkeeper::Stance(float K)
{
	const FPose P = PoseAt(PP::Smooth(K), Play);
	(void)P; // las piezas se ubican en ApplyVisual()
}

void APPGoalkeeper::IdleArms(float K, float Dt)
{
	const FPose P = PoseAt(K, Play);
	if (Dt > 0.f)
	{
		const float A = PP::PowDt(.004f, Dt);
		GloveLocal = FMath::Lerp(GloveLocal, P.Glove, A);
		BlockerLocal = FMath::Lerp(BlockerLocal, P.Block, A);
	}
	else { GloveLocal = P.Glove; BlockerLocal = P.Block; }
}

FPPVec APPGoalkeeper::GlovePos() const
{
	const float C = FMath::Cos(Roll), S = FMath::Sin(Roll);
	return FPPVec(Pos.X + GloveLocal.X * C - GloveLocal.Y * S, GloveLocal.X * S + GloveLocal.Y * C, Pos.Y + GloveLocal.Z);
}

FPPVec APPGoalkeeper::StickTip() const
{
	// el palo queda acostado en el piso, agarrado por el blocker, con la pala hacia adelante
	return FPPVec(Pos.X + BlockerLocal.X - .02f, .01f, Pos.Y + BlockerLocal.Z + .55f + PokeAlpha * .35f);
}

bool APPGoalkeeper::StickHits(const FPPVec& B) const
{
	if (B.Y > .16f) return false;
	const FPPVec Tip = StickTip();
	const FPPVec Hand(Pos.X + BlockerLocal.X, 0.f, Pos.Y + BlockerLocal.Z);
	const FPPVec A = FMath::Lerp(Hand, Tip, .3f);
	FPPVec AB = Tip - A; AB.Y = 0.f;
	const float L2 = AB.SizeSquared();
	if (L2 < 1e-6f) return false;
	const float T = FMath::Clamp(((B.X - A.X) * AB.X + (B.Z - A.Z) * AB.Z) / L2, 0.f, 1.f);
	const float Cx = A.X + AB.X * T, Cz = A.Z + AB.Z * T;
	return PP::Hypot(B.X - Cx, B.Z - Cz) < .1f + PP::BallR;
}

FString APPGoalkeeper::BlocksBall(const FPPVec& B) const
{
	const float R = PP::BallR;
	const FPose P = PoseAt(PP::Smooth(Kneel), Play);
	const float TorsoX = Pos.X - FMath::Sin(Roll) * .7f;
	if (FMath::Abs(B.X - TorsoX) < .27f + R && B.Y > .3f && B.Y < 1.07f + R) return TEXT("el cuerpo");
	const float HeadY = 1.12f + P.Up[1];
	if (PP::Hypot(B.X - TorsoX, B.Y - HeadY) < .15f + R) return TEXT("la careta");
	for (int i = 0; i < 2; i++)
		if (FMath::Abs(B.X - (Pos.X + P.Pads[i][0] + PadExt[i])) < .23f + R && B.Y < .31f + R) return TEXT("las polainas");
	const FPPVec G = GlovePos();
	if (PP::Hypot(B.X - G.X, B.Y - G.Y) < .16f + R) return TEXT("el guante");
	const float C = FMath::Cos(Roll), S = FMath::Sin(Roll);
	const float Bx = Pos.X + BlockerLocal.X * C - BlockerLocal.Y * S, By = BlockerLocal.X * S + BlockerLocal.Y * C;
	if (PP::Hypot(B.X - Bx, B.Y - By) < .15f + R) return TEXT("el blocker");
	const FPPVec Tip = StickTip();
	if (B.Y < .1f + R && FMath::Abs(B.X - Tip.X) < .13f + R) return TEXT("el palo");
	return FString();
}

void APPGoalkeeper::Deflect(const FString& Part, const FPPVec& P, FPPVec& V, FPPShot& Shot) const
{
	const float Vz = FMath::Abs(V.Z);
	const float Side = FMath::Sign(P.X - Pos.X) == 0.f ? 1.f : FMath::Sign(P.X - Pos.X);
	if (Part == TEXT("el guante")) { Shot.bCaught = true; V = FPPVec::ZeroVector; return; }
	if (Part == TEXT("el palo")) { V = FPPVec(Side * (1.6f + FMath::FRand() * 1.2f), .04f, .5f + Vz * .05f); return; }
	if (Part == TEXT("el blocker")) { V = FPPVec(Side * (2.f + FMath::FRand() * 1.5f), .6f + FMath::FRand() * .5f, 1.f + Vz * .06f); return; }
	if (Part == TEXT("las polainas")) { V = FPPVec((FMath::FRand() - .5f) * 1.5f + Side * .7f, .1f + FMath::FRand() * .2f, Vz * .07f + .7f); return; }
	if (Part == TEXT("la careta")) { V = FPPVec((FMath::FRand() - .5f) * 1.8f, 1.2f + FMath::FRand() * .8f, Vz * .1f + .6f); return; }
	V = FPPVec((FMath::FRand() - .5f) * 1.f, .2f + FMath::FRand() * .3f, .4f + Vz * .04f);   // el cuerpo la amortigua
}

void APPGoalkeeper::Update(float Dt, float Time, FPPShot* S, bool bLaunched, EPPState State,
                           APPBall* Ball, APPShooter* Shooter, FPPVec StartPos)
{
	const FPPGKStyle GS = FPPGKStyle::Make(Style);
	const FPPDiff& D = Diff;
	const bool bHasShot = S && State != EPPState::Loose && State != EPPState::Rival;
	FPPShot* Sh = bHasShot ? S : nullptr;

	// el que se tira temprano elige un lado cuando el atacante levanta el palo
	if (GS.bEarly)
	{
		if (State == EPPState::Charge || (State == EPPState::Swing && !bLaunched))
		{
			EarlyT += Dt;
			if (!bEarlyUsed) { bEarlyUsed = true; EarlySide = FMath::FRand() < .5f ? -1 : 1; }
			if (EarlySide && EarlyT > D.Recover) EarlySide = 0;
		}
		else if (State == EPPState::Aim) { EarlyT = 0.f; bEarlyUsed = false; EarlySide = 0; }
	}
	const float KT = ((Sh && Sh->T > Sh->React * .5f) || (GS.bEarly && EarlySide)) ? 1.f : 0.f;
	// la bocha entra en juego cuando el atacante la toca y la conduce
	if (!bInPlayLatched && State == EPPState::Aim && Shooter && FPPVec::Dist(Shooter->GamePos, StartPos) > .3f) bInPlayLatched = true;
	Play = FMath::Lerp(Play, bInPlayLatched ? 1.f : 0.f, PP::PowDt(.0005f, Dt));
	Kneel = FMath::Lerp(Kneel, KT, PP::PowDt(KT > Kneel ? 1e-7f : (State == EPPState::Loose ? .35f : .05f), Dt));
	if (!Sh)
	{
		PadExt.X = FMath::Lerp(PadExt.X, 0.f, PP::PowDt(.02f, Dt));
		PadExt.Y = FMath::Lerp(PadExt.Y, 0.f, PP::PowDt(.02f, Dt));
		Roll = FMath::Lerp(Roll, Lean, PP::PowDt(.02f, Dt));
		IdleArms(Kneel, Dt);
	}
	if (!Sh)
	{
		const FPPVec Bp = Ball ? Ball->Pos : FPPVec(0.f, 0.f, PP::SpotZ);
		const float Bz = FMath::Max(Bp.Z, .8f);
		// se desplaza de costado acompañando la bocha; los amagues rápidos lo hacen pasarse de largo
		const float Bx = Bp.X;
		BallVx = FMath::Lerp(BallVx, (Bx - LastBx) / FMath::Max(Dt, 1e-3f), PP::PowDt(.001f, Dt)); LastBx = Bx;
		const float Bite = FMath::Clamp(BallVx * .11f * D.Bite, -.32f, .32f);
		const float Lim = .45f - .18f * Play;
		const float Tgt = FMath::Clamp(Bx / Bz * .9f + Bite, -Lim, Lim) + FMath::Sin(Time * 1.3f) * .02f;
		// sale a achicar cuando el atacante se acerca de frente
		const float Front = FMath::Clamp(1.f - FMath::Abs(Bx) / (Bp.Z + 1.f), 0.f, 1.f);
		const float Out = FMath::Min(1.25f, Play * Front * FMath::Clamp((6.2f - Bp.Z) / 2.4f, 0.f, 1.f) * FMath::Clamp((Bp.Z - 1.6f) / 1.4f, 0.f, 1.f) * .32f * GS.Out);
		Move(FPPVec2((EarlySide && State != EPPState::Aim) ? EarlySide * .3f : Tgt, .42f + Out), D.Speed * .75f * GS.Speed, Dt, 12.f);
		Yaw = FMath::Lerp(Yaw, FMath::Clamp(FMath::Atan2(Bp.X - Pos.X, Bz), -.6f, .6f) * .6f, PP::PowDt(.02f, Dt));

		// estocada con el palo cuando el atacante se le mete encima
		if (PokeAlpha > 0.f)
		{
			PokeT += Dt;
			PokeAlpha = PokeT < .22f ? PokeT / .22f : FMath::Max(0.f, 1.f - (PokeT - .22f) / .3f);
			if (Ball) BlockerLocal = FMath::Lerp(BlockerLocal, FPPVec(FMath::Clamp(Bp.X - Pos.X, -.75f, .2f), .16f, FMath::Clamp(Bp.Z - Pos.Y, .2f, .75f)), PokeAlpha);
		}
		else
		{
			PokeCd = FMath::Max(0.f, PokeCd - Dt);
			const float Dz = Bp.Z - Pos.Y, Dx = Bp.X - Pos.X;
			const bool bStealable = State == EPPState::Aim || State == EPPState::Charge || (State == EPPState::Swing && !bLaunched);
			if (bStealable && PokeCd <= 0.f && Dz > .15f && PP::Hypot(Dx, Dz) < 1.05f)
			{
				const float Rate = (D.React < .15f ? 1.8f : D.React < .25f ? 1.1f : .5f) * GS.Poke;
				if (FMath::FRand() < Rate * Dt) { PokeAlpha = .01f; PokeT = 0.f; PokeCd = 1.6f; }
			}
		}
		ApplyVisual();
		return;
	}

	// ---- reacción al tiro ----
	Sh->T += Dt;
	const FPose Base = PoseAt(Kneel, Play);
	if (Sh->T <= Sh->React)
	{
		const float L = FMath::Clamp(Sh->PredX - Pos.X, -.6f, .6f) * D.Anticip;   // carga el peso hacia el lado que intuye
		Roll = FMath::Lerp(Roll, -L * .08f, PP::PowDt(.001f, Dt));
		Vel.X += L * .8f * Dt;
	}
	if (Sh->T > Sh->React)
	{
		Move(FPPVec2(Sh->GTarget, Pos.Y), D.Speed * (Sh->Kind == EPPShotKind::Arrastre ? .7f : 1.f), Dt, 30.f);
		Sh->Ext = FMath::Min(1.f, (Sh->T - Sh->React) / Sh->Ramp);
	}
	const float E = PP::Smooth(Sh->Ext);
	const bool bLow = Sh->PredY < .42f;
	if (!bHasH0) { ReachH0Glove = GloveLocal; ReachH0Block = BlockerLocal; bHasH0 = true; }
	const float Rx = FMath::Clamp(Sh->PredX - Pos.X, -.95f, .95f);
	FPPVec Reach(Rx, FMath::Clamp(Sh->PredY, .08f, 1.25f), Sh->ReachZ);
	bool bOther = false; FPPVec Other;
	const FName Tech = Sh->Tech;
	if (Tech == TEXT("cuerpo")) { Reach.X *= .55f; Reach.Z = .22f; Other = FPPVec(-Reach.X * .2f, FMath::Clamp(Sh->PredY - .15f, .3f, 1.f), .24f); bOther = true; }
	if (Tech == TEXT("estirada")) { Reach.X = FMath::Clamp(Rx * 1.12f, -1.02f, 1.02f); Reach.Y = FMath::Clamp(Sh->PredY + .04f, .08f, 1.3f); }
	if (Tech == TEXT("palo") && bLow) Reach = FPPVec(FMath::Clamp(Rx, -.7f, .7f), .12f, .4f);
	const int32 Side = (Tech == TEXT("palo") && bLow) ? -1 : Sh->ReachSide;
	GloveLocal = FMath::Lerp(ReachH0Glove, Side > 0 ? Reach : (bOther ? Other : Base.Glove), Side > 0 ? E : E * .6f);
	BlockerLocal = FMath::Lerp(ReachH0Block, Side < 0 ? Reach : (bOther ? Other : Base.Block), Side < 0 ? E : E * .6f);

	// split: estira la polaina del lado del tiro · stack: tumba las dos hacia el lado · mariposa: cierra las dos al centro
	const bool bFar = FMath::Abs(Sh->PredX - Pos.X) > .3f;
	const float Dir = FMath::Sign(Sh->PredX - Pos.X) == 0.f ? 1.f : FMath::Sign(Sh->PredX - Pos.X);
	for (int i = 0; i < 2; i++)
	{
		const float Sd = i == 0 ? -1.f : 1.f; float X = 0.f;
		if (bLow)
		{
			if (Tech == TEXT("stack")) X = Dir * .14f * E;
			else if (Tech == TEXT("mariposa")) X = -Sd * .05f * E;
			else X = Dir == Sd ? Sd * (bFar ? .2f : .1f) * E : (!bFar ? -Sd * .03f * E : 0.f);
		}
		PadExt[i] = X;
	}
	const float Tilt = (bLow && bFar ? .28f : .15f) * Sh->Tilt * (Tech == TEXT("estirada") ? 1.45f : Tech == TEXT("stack") ? 1.3f : Tech == TEXT("cuerpo") ? .5f : 1.f);
	Roll = FMath::Lerp(Roll, -Sh->ReachSide * Tilt * E, PP::PowDt(.0001f, Dt));
	ApplyVisual();
}

void APPGoalkeeper::ApplyVisual()
{
	const FPose P = PoseAt(PP::Smooth(Kneel), Play);
	// el actor se ubica en el mundo; las piezas usan coordenadas locales en cm (X adelante hacia el tirador = -z de juego)
	SetActorLocation(PP::ToUE(FPPVec(Pos.X, 0.f, Pos.Y)));
	SetActorRotation(FRotator(0.f, FMath::RadiansToDegrees(-Yaw), FMath::RadiansToDegrees(-Roll)));   // yaw alrededor de Z, roll sobre X (adelante)

	auto Loc = [&](float gx, float gy, float gz) { return FVector(-gz * 100.f, gx * 100.f, gy * 100.f); };
	auto Place = [&](UStaticMeshComponent* C, const FPPVec& GLoc, const FPPVec& SizeM)
	{
		if (!C) return;
		C->SetRelativeLocation(Loc(GLoc.X, GLoc.Y, GLoc.Z));
		C->SetRelativeScale3D(FVector(SizeM.Z, SizeM.X, SizeM.Y));   // escala base de 100 cm: m -> escala directa
	};
	const float UpY = P.Up[1];
	Place(Torso, FPPVec(0, .68f + UpY, .02f), FPPVec(.5f, .76f, .3f));
	Place(Head, FPPVec(0, 1.12f + UpY, .02f), FPPVec(.28f, .30f, .28f));
	for (int i = 0; i < 2; i++)
	{
		Place(Pads[i], FPPVec(P.Pads[i][0] + PadExt[i], .16f + (1.f - Kneel) * .17f, P.Pads[i][2]), FPPVec(.34f, .5f, .22f));
	}
	Place(Glove, GloveLocal, FPPVec(.26f, .26f, .12f));
	Place(Blocker, BlockerLocal, FPPVec(.24f, .3f, .08f));
	Place(Stick, FPPVec(BlockerLocal.X, .01f, BlockerLocal.Z + .28f), FPPVec(.06f, .02f, .62f));
}
