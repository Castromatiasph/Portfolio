#include "PPArena.h"
#include "PPTypes.h"
#include "PPMaterial.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

using namespace PP;

APPArena::APPArena()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void APPArena::OnConstruction(const FTransform& Xf) { Super::OnConstruction(Xf); Build(); }

template <class T> T* APPArena::Mk()
{
	T* C = NewObject<T>(this, NAME_None, RF_Transient);
	C->SetupAttachment(RootComponent);
	return C;
}

UInstancedStaticMeshComponent* APPArena::MkISM(UStaticMesh* Mesh, UMaterialInterface* Mat, bool bShadow)
{
	UInstancedStaticMeshComponent* C = Mk<UInstancedStaticMeshComponent>();
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	if (Mat) C->SetMaterial(0, Mat);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(bShadow);
	C->RegisterComponent();
	return C;
}

UMaterialInterface* APPArena::Tint(UMaterialInterface* Override, const FLinearColor& Col)
{
	if (Override) return Override;
	const bool bEmit = Col.R > 1.f || Col.G > 1.f || Col.B > 1.f;   // las luminarias se dibujan como emisivas
	const uint32 H = GetTypeHash(Col.ToFColor(true)) ^ (bEmit ? 0x9E3779B9u : 0u);
	if (TObjectPtr<UMaterialInstanceDynamic>* F = MatCache.Find(H)) return F->Get();
	UMaterialInstanceDynamic* M = PP::ColorMID(this, bEmit ? FLinearColor::Black : Col, .5f, bEmit ? Col : FLinearColor::Black);
	MatCache.Add(H, M);
	return M;
}

UStaticMeshComponent* APPArena::Box(const FVector& G, const FVector& S, float YawDeg, UMaterialInterface* Mat, const FLinearColor& Col, UStaticMesh* Mesh)
{
	UStaticMeshComponent* C = Mk<UStaticMeshComponent>();
	UStaticMesh* M = Mesh ? Mesh : LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	C->SetStaticMesh(M);
	C->SetRelativeLocation(ToUE(FPPVec(G.X, G.Y, G.Z)));
	C->SetRelativeRotation(FRotator(0.f, YawDeg, 0.f));
	C->SetRelativeScale3D(FVector(S.Z, S.X, S.Y));      // m -> escala del cubo de 100 cm (X adelante = z de juego)
	if (!Mesh) C->SetMaterial(0, Tint(Mat, Col));       // un mesh importado conserva sus materiales
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->RegisterComponent();
	Built.Add(C);
	return C;
}

UStaticMeshComponent* APPArena::Cyl(const FVector& A, const FVector& B, float R, const FLinearColor& Col)
{
	UStaticMeshComponent* C = Mk<UStaticMeshComponent>();
	C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	const FVector Ua = ToUE(FPPVec(A.X, A.Y, A.Z)), Ub = ToUE(FPPVec(B.X, B.Y, B.Z));
	C->SetRelativeLocation((Ua + Ub) * .5);
	C->SetRelativeRotation(FRotationMatrix::MakeFromZ(Ub - Ua).Rotator());
	C->SetRelativeScale3D(FVector(R * 2.0, R * 2.0, (Ub - Ua).Size() / 100.0));
	C->SetMaterial(0, Tint(nullptr, Col));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->RegisterComponent();
	Built.Add(C);
	return C;
}

void APPArena::NetLine(const FVector& A, const FVector& B)
{
	if (!NetISM) return;
	const FVector Ua = ToUE(FPPVec(A.X, A.Y, A.Z)), Ub = ToUE(FPPVec(B.X, B.Y, B.Z));
	FTransform T;
	T.SetLocation((Ua + Ub) * .5);
	T.SetRotation(FRotationMatrix::MakeFromZ(Ub - Ua).ToQuat());
	T.SetScale3D(FVector(.005, .005, (Ub - Ua).Size() / 100.0));
	NetISM->AddInstance(T);
}

void APPArena::BuildNet()
{
	UStaticMesh* CylM = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	NetISM = MkISM(CylM, PP::ColorMID(this, FLinearColor(.9f, .9f, .9f), .8f), false);
	const float Zf = 0.f, Zb = -.92f, Zt = -.5f, Y0 = .025f;
	// red de atrás (inclinada): verticales y horizontales
	for (int i = 0; i <= 12; i++) { const float X = -PX + 2.f * PX * i / 12.f; NetLine(FVector(X, Y0, Zb), FVector(X, CY, Zt)); }
	for (int k = 0; k <= 8; k++) { const float F = k / 8.f; NetLine(FVector(-PX, Y0 + (CY - Y0) * F, Zb + (Zt - Zb) * F), FVector(PX, Y0 + (CY - Y0) * F, Zb + (Zt - Zb) * F)); }
	// techo de la red
	for (int i = 0; i <= 12; i++) { const float X = -PX + 2.f * PX * i / 12.f; NetLine(FVector(X, CY, Zf), FVector(X, CY, Zt)); }
	for (int k = 1; k <= 3; k++) { const float Z = Zt * k / 3.f; NetLine(FVector(-PX, CY, Z), FVector(PX, CY, Z)); }
	// laterales
	for (int s = -1; s <= 1; s += 2)
	{
		for (int k = 0; k <= 8; k++) { const float F = k / 8.f; const float Y = Y0 + (CY - Y0) * F; const float Zb2 = Zb + (Zt - Zb) * F; NetLine(FVector(s * PX, Y, Zf), FVector(s * PX, Y, Zb2)); }
		for (int i = 0; i <= 6; i++) { const float F = i / 6.f; NetLine(FVector(s * PX, Y0, Zb * F), FVector(s * PX, CY, Zt * F)); }
	}
}

void APPArena::BuildCrowd()
{
	if (CrowdDensity <= 0) return;
	UStaticMesh* CubeM = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* SphereM = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static const FLinearColor Cols[6] = { FLinearColor(.55f, .04f, .09f), FLinearColor(.8f, .82f, .85f), FLinearColor(.05f, .08f, .2f), FLinearColor(.8f, .6f, .1f), FLinearColor(.1f, .22f, .5f), FLinearColor(.12f, .13f, .15f) };
	for (int g = 0; g < 6; g++) CrowdTorso.Add(MkISM(CubeM, PP::ColorMID(this, Cols[g], .8f), false));
	CrowdHead = MkISM(SphereM, PP::ColorMID(this, FLinearColor(.45f, .27f, .18f), .6f), false);
	FRandomStream Rng(1234);
	auto AddFan = [&](float X, float H, float Z)
	{
		FFan F; F.Base = ToUE(FPPVec(X, H, Z)); F.Phase = Rng.FRand() * 6.28f; F.Group = Rng.RandRange(0, 5); F.Idx = 0;
		Fans.Add(F);
	};
	const float L = MidZ - EndZ;
	for (int r = 0; r < 4; r++)
	{
		const float Off = .6f + r * 1.13f, H = .45f + r * .4f;
		for (float X = -(HalfW + 4.f); X <= HalfW + 4.f; X += .55f) if (Rng.FRand() > .12f) AddFan(X, H, EndZ - Off - .55f);
		for (float Z = EndZ + 1.f; Z <= MidZ; Z += .55f)
		{
			if (Rng.FRand() > .12f) AddFan(-HalfW - Off - .55f, H, Z);
			if (Rng.FRand() > .12f) AddFan(HalfW + Off + .55f, H, Z);
		}
	}
	(void)L;
	TArray<int32> Count; Count.Init(0, 6);
	for (FFan& F : Fans)
	{
		F.Idx = Count[F.Group]++;
		FTransform Tt; Tt.SetLocation(F.Base + FVector(0, 0, 30)); Tt.SetScale3D(FVector(.24, .36, .44));
		CrowdTorso[F.Group]->AddInstance(Tt);
		FTransform Th; Th.SetLocation(F.Base + FVector(0, 0, 66)); Th.SetScale3D(FVector(.2));
		CrowdHead->AddInstance(Th);
	}
}

void APPArena::PlaceFans(float T, float Jump)
{
	for (int32 i = 0; i < Fans.Num(); i++)
	{
		const FFan& F = Fans[i];
		const float Up = Jump > 0.f ? FMath::Abs(FMath::Sin(T * 9.f + F.Phase)) * Jump * 22.f : 0.f;
		FTransform Tt; Tt.SetLocation(F.Base + FVector(0, 0, 30 + Up)); Tt.SetScale3D(FVector(.24, .36, .44));
		CrowdTorso[F.Group]->UpdateInstanceTransform(F.Idx, Tt, false, false, true);
		FTransform Th; Th.SetLocation(F.Base + FVector(0, 0, 66 + Up)); Th.SetScale3D(FVector(.2));
		CrowdHead->UpdateInstanceTransform(i, Th, false, false, true);
	}
	for (auto& C : CrowdTorso) if (C) C->MarkRenderStateDirty();
	if (CrowdHead) CrowdHead->MarkRenderStateDirty();
}

void APPArena::Cheer(float Seconds)
{
	if (Fans.Num() == 0) return;
	CheerT = Seconds; SetActorTickEnabled(true);
}

void APPArena::Confetti()
{
	if (ConfISM.Num() == 0) return;
	Conf.Reset();
	FRandomStream R(FMath::Rand());
	int32 N0 = 0, N1 = 0;
	for (int i = 0; i < 260; i++)
	{
		FConf C; C.Group = i % 2; C.bOn = true;
		C.P = FVector((R.FRand() - .5) * 24.0, 4.0 + R.FRand() * 3.0, -4.5 - R.FRand() * 3.5);
		C.V = FVector((R.FRand() - .5) * 4.0, 2.0 + R.FRand() * 3.0, 1.0 + R.FRand() * 3.0);
		C.W = FVector(R.FRand() * 8, R.FRand() * 8, R.FRand() * 8);
		C.R = FRotator(R.FRand() * 360, R.FRand() * 360, R.FRand() * 360);
		C.Idx = C.Group == 0 ? N0++ : N1++;
		Conf.Add(C);
	}
	// instancias nuevas
	for (auto& I : ConfISM) I->ClearInstances();
	for (const FConf& C : Conf)
	{
		FTransform T; T.SetLocation(ToUE(FPPVec(float(C.P.X), float(C.P.Y), float(C.P.Z)))); T.SetScale3D(FVector(.05, .035, .002));
		ConfISM[C.Group]->AddInstance(T);
	}
	ConfT = 9.f; SetActorTickEnabled(true);
}

void APPArena::Tick(float Dt)
{
	Super::Tick(Dt);
	bool bAny = false;
	if (CheerT > 0.f)
	{
		bAny = true; CheerT -= Dt; CheerAcc += Dt;
		if (CheerAcc > .033f)
		{
			CheerAcc = 0.f;
			PlaceFans(GetWorld()->GetTimeSeconds(), FMath::Clamp(CheerT, 0.f, 1.f));
		}
		if (CheerT <= 0.f) PlaceFans(0.f, 0.f);
	}
	if (ConfT > 0.f)
	{
		bAny = true; ConfT -= Dt;
		const float T = GetWorld()->GetTimeSeconds();
		for (FConf& C : Conf)
		{
			if (!C.bOn) continue;
			C.V.Y -= 1.6 * Dt; C.V.Y = FMath::Max(C.V.Y, -.9); C.V.X *= (1.0 - .6 * Dt); C.V.Z *= (1.0 - .6 * Dt);   // cae flotando
			C.P += C.V * Dt;
			C.P.X += FMath::Sin(T * 3.0 + C.Idx) * .4 * Dt;
			C.R += FRotator(C.W.X, C.W.Y, C.W.Z) * Dt * 40.f;
			FTransform Tr; Tr.SetRotation(C.R.Quaternion());
			if (C.P.Y <= .02) { C.bOn = false; C.P.Y = .02; Tr.SetScale3D(FVector(.0001)); }   // llegó al piso: desaparece
			else Tr.SetScale3D(FVector(.05, .035, .002));
			Tr.SetLocation(ToUE(FPPVec(float(C.P.X), float(C.P.Y), float(C.P.Z))));
			ConfISM[C.Group]->UpdateInstanceTransform(C.Idx, Tr, false, false, true);
		}
		for (auto& I : ConfISM) I->MarkRenderStateDirty();
		if (ConfT <= 0.f) for (auto& I : ConfISM) I->ClearInstances();
	}
	if (!bAny) SetActorTickEnabled(false);
}

void APPArena::ApplyQuality(EPPQuality Q)
{
	CurQ = Q;
	UWorld* W = GetWorld();
	auto Cmd = [&](const TCHAR* C) { if (GEngine && W) GEngine->Exec(W, C); };
	switch (Q)
	{
	case EPPQuality::Baja:
		Cmd(TEXT("r.ScreenPercentage 60")); Cmd(TEXT("r.AntiAliasingMethod 1"));
		Cmd(TEXT("r.Shadow.Virtual.SMRT.RayCountLocal 1")); Cmd(TEXT("r.Shadow.Virtual.SMRT.SamplesPerRayLocal 2")); break;
	case EPPQuality::Media:
		Cmd(TEXT("r.ScreenPercentage 80")); Cmd(TEXT("r.AntiAliasingMethod 4"));
		Cmd(TEXT("r.Shadow.Virtual.SMRT.RayCountLocal 2")); Cmd(TEXT("r.Shadow.Virtual.SMRT.SamplesPerRayLocal 4")); break;
	default:
		Cmd(TEXT("r.ScreenPercentage 100")); Cmd(TEXT("r.AntiAliasingMethod 4"));
		Cmd(TEXT("r.Shadow.Virtual.SMRT.RayCountLocal 4")); Cmd(TEXT("r.Shadow.Virtual.SMRT.SamplesPerRayLocal 4")); break;
	}
	for (int32 i = 0; i < Lights.Num(); i++)
		if (Lights[i]) Lights[i]->SetCastShadows(Q == EPPQuality::Alta || (Q == EPPQuality::Media && (i == 1 || i == 4)));
	if (Fill) Fill->SetVisibility(Q == EPPQuality::Baja);   // sin GI, una luz de relleno suave
	if (Pp)
	{
		FPostProcessSettings& S = Pp->Settings;
		S.bOverride_DynamicGlobalIlluminationMethod = true;
		S.DynamicGlobalIlluminationMethod = Q == EPPQuality::Baja ? EDynamicGlobalIlluminationMethod::None : EDynamicGlobalIlluminationMethod::Lumen;
		S.bOverride_ReflectionMethod = true;
		S.ReflectionMethod = Q == EPPQuality::Alta ? EReflectionMethod::Lumen : EReflectionMethod::ScreenSpace;
		const float Lq = Q == EPPQuality::Alta ? 1.5f : .6f;
		S.bOverride_LumenSceneLightingQuality = true; S.LumenSceneLightingQuality = Lq;
		S.bOverride_LumenFinalGatherQuality = true; S.LumenFinalGatherQuality = Lq;
		S.bOverride_LumenSceneDetail = true; S.LumenSceneDetail = Q == EPPQuality::Alta ? 1.5f : 1.f;
		S.bOverride_BloomIntensity = true; S.BloomIntensity = Q == EPPQuality::Baja ? .1f : .25f;
		S.bOverride_MotionBlurAmount = true; S.MotionBlurAmount = Q == EPPQuality::Alta ? .3f : 0.f;
		S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = Q == EPPQuality::Baja ? 0.f : .4f;
		S.bOverride_FilmGrainIntensity = true; S.FilmGrainIntensity = Q == EPPQuality::Alta ? .1f : 0.f;
	}
}

void APPArena::Build()
{
	for (auto& C : Built) if (C) C->DestroyComponent();
	Built.Reset();
	for (auto& C : Lights) if (C) C->DestroyComponent();
	Lights.Reset();
	if (Fill) { Fill->DestroyComponent(); Fill = nullptr; }
	if (Pp) { Pp->DestroyComponent(); Pp = nullptr; }
	if (NetISM) { NetISM->DestroyComponent(); NetISM = nullptr; }
	for (auto& C : CrowdTorso) if (C) C->DestroyComponent();
	CrowdTorso.Reset();
	if (CrowdHead) { CrowdHead->DestroyComponent(); CrowdHead = nullptr; }
	for (auto& C : ConfISM) if (C) C->DestroyComponent();
	ConfISM.Reset(); Fans.Reset(); Conf.Reset();

	// ---- piso: parquet barnizado (textura + clear coat) ----
	const float L = MidZ - EndZ;
	UMaterialInterface* FloorMat = FloorMaterial;
	if (!FloorMat)
	{
		UTexture* Parquet = LoadObject<UTexture>(nullptr, TEXT("/Game/Textures/T_Parquet_BaseColor.T_Parquet_BaseColor"));
		FloorMat = PP::ColorMID(this, Parquet ? FLinearColor::White : FLinearColor(.55f, .38f, .2f), .3f, FLinearColor::Black, Parquet, 7.f, 1.f);
	}
	Box(FVector(0, -.025f, EndZ + L * .5f), FVector(HalfW * 2.f + .4f, .05f, L + .4f), 0.f, FloorMat, FLinearColor::White);

	// ---- líneas de 8 cm ----
	const FLinearColor White(.95f, .96f, .98f);
	auto Line = [&](float X1, float Z1, float X2, float Z2)
	{
		const float Dx = X2 - X1, Dz = Z2 - Z1, Len = Hypot(Dx, Dz);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dx, -Dz));   // el eje X de Unreal es -z de juego
		Box(FVector((X1 + X2) * .5f, .002f, (Z1 + Z2) * .5f), FVector(.08f, .004f, Len), Yaw, LineMaterial, White);
	};
	Line(-4.5f, 0.f, 4.5f, 0.f);
	Line(-4.5f, 0.f, -4.5f, 5.4f); Line(4.5f, 0.f, 4.5f, 5.4f); Line(-4.5f, 5.4f, 4.5f, 5.4f);
	for (int i = 0; i < 16; i++)
	{
		const float A0 = PI * i / 16.f, A1 = PI * (i + 1) / 16.f;
		Line(PostHalf * FMath::Cos(A0), PostHalf * FMath::Sin(A0), PostHalf * FMath::Cos(A1), PostHalf * FMath::Sin(A1));
	}
	Box(FVector(0, .003f, SpotZ), FVector(.14f, .004f, .14f), 0.f, LineMaterial, White);
	Box(FVector(0, .003f, FreeZ), FVector(.14f, .004f, .14f), 0.f, LineMaterial, White);

	// ---- vallas de 1 m con esquinas de 3 m ----
	TArray<FVector2f> Path;
	Path.Add(FVector2f(-HalfW, MidZ));
	Path.Add(FVector2f(-HalfW, EndZ + CornerR));
	const int Seg = 8;
	for (int i = 1; i <= Seg; i++) { const float A = HALF_PI * i / Seg; Path.Add(FVector2f(-HalfW + CornerR - CornerR * FMath::Cos(A), EndZ + CornerR - CornerR * FMath::Sin(A))); }
	for (int i = 0; i <= Seg; i++) { const float A = HALF_PI * i / Seg; Path.Add(FVector2f(HalfW - CornerR + CornerR * FMath::Sin(A), EndZ + CornerR - CornerR * FMath::Cos(A))); }
	Path.Add(FVector2f(HalfW, MidZ));
	Path.Add(FVector2f(-HalfW, MidZ));
	for (int i = 0; i + 1 < Path.Num(); i++)
	{
		const FVector2f A = Path[i], B = Path[i + 1];
		const float Dx = B.X - A.X, Dz = B.Y - A.Y, Len = Hypot(Dx, Dz);
		if (Len < .01f) continue;
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dx, -Dz));
		Box(FVector((A.X + B.X) * .5f, .5f, (A.Y + B.Y) * .5f), FVector(.08f, 1.f, Len + .02f), Yaw, BoardMaterial, FLinearColor(.85f, .87f, .9f));
		Box(FVector((A.X + B.X) * .5f, 1.03f, (A.Y + B.Y) * .5f), FVector(.1f, .07f, Len + .03f), Yaw, BoardMaterial, FLinearColor(.04f, .08f, .2f));
	}
	// publicidad en la valla del fondo (si está la textura)
	if (UTexture* Ad = LoadObject<UTexture>(nullptr, TEXT("/Game/Textures/T_Publicidad.T_Publicidad")))
	{
		UMaterialInterface* AdM = PP::ColorMID(this, FLinearColor::White, .5f, FLinearColor::Black, Ad, 1.f, 0.f);
		Box(FVector(0, .62f, EndZ + .052f), FVector(13.6f, .6f, .01f), 0.f, AdM, FLinearColor::White);
	}

	// ---- paredes del recinto (nada de vacío negro) ----
	const FLinearColor WallC(.16f, .17f, .2f);
	Box(FVector(0, 5.f, EndZ - 9.f), FVector(HalfW * 2.f + 24.f, 11.f, .3f), 0.f, StandMaterial, WallC);
	Box(FVector(-HalfW - 8.f, 5.f, (EndZ + MidZ) * .5f), FVector(.3f, 11.f, L + 24.f), 0.f, StandMaterial, WallC);
	Box(FVector(HalfW + 8.f, 5.f, (EndZ + MidZ) * .5f), FVector(.3f, 11.f, L + 24.f), 0.f, StandMaterial, WallC);
	Box(FVector(0, 5.f, MidZ + 6.f), FVector(HalfW * 2.f + 24.f, 11.f, .3f), 0.f, StandMaterial, WallC);

	// ---- gradas ----
	const FLinearColor StandCol(.22f, .23f, .28f);
	for (int r = 0; r < 4; r++)
	{
		const float Off = .6f + r * 1.13f, H = .45f + r * .4f;
		Box(FVector(0, H * .5f, EndZ - Off - .55f), FVector(HalfW * 2.f + 8.f, H, 1.13f), 0.f, StandMaterial, StandCol);
		Box(FVector(-HalfW - Off - .55f, H * .5f, (EndZ + MidZ) * .5f), FVector(1.13f, H, L), 0.f, StandMaterial, StandCol);
		Box(FVector(HalfW + Off + .55f, H * .5f, (EndZ + MidZ) * .5f), FVector(1.13f, H, L), 0.f, StandMaterial, StandCol);
	}
	BuildCrowd();

	// ---- techo y luminarias (6 luces rectangulares: pocas y fuertes) ----
	Box(FVector(0, 10.6f, (EndZ + MidZ) * .5f), FVector(HalfW * 2.f + 24.f, .3f, L + 24.f), 0.f, StandMaterial, FLinearColor(.1f, .1f, .12f));
	for (int i = 0; i < 2; i++) for (int j = 0; j < 3; j++)
	{
		const float X = i == 0 ? -5.f : 5.f, Z = j * 7.f;
		URectLightComponent* Rl = Mk<URectLightComponent>();
		Rl->SetMobility(EComponentMobility::Movable);
		Rl->SetRelativeLocation(ToUE(FPPVec(X, 10.3f, Z)));
		Rl->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
		Rl->SetSourceWidth(180.f); Rl->SetSourceHeight(60.f);
		Rl->SetIntensityUnits(ELightUnits::Lumens);
		Rl->SetIntensity(LightLumens);
		Rl->SetLightColor(FLinearColor(1.f, .93f, .84f));
		Rl->SetAttenuationRadius(2600.f);
		Rl->RegisterComponent();
		Lights.Add(Rl);
		Box(FVector(X, 10.45f, Z), FVector(1.8f, .05f, .6f), 0.f, nullptr, FLinearColor(3.f, 2.8f, 2.5f));
	}
	Fill = Mk<UPointLightComponent>();
	Fill->SetMobility(EComponentMobility::Movable);
	Fill->SetRelativeLocation(ToUE(FPPVec(0.f, 6.f, 6.f)));
	Fill->SetIntensityUnits(ELightUnits::Lumens);
	Fill->SetIntensity(9000.f); Fill->SetAttenuationRadius(3500.f); Fill->SetCastShadows(false);
	Fill->SetLightColor(FLinearColor(1.f, .95f, .88f));
	Fill->RegisterComponent(); Fill->SetVisibility(false);

	// ---- arco reglamentario: 1,70 × 1,05 m, naranja fluorescente, con red ----
	const FLinearColor Orange(1.f, .27f, .04f);
	if (GoalMesh)
	{
		UStaticMeshComponent* G = Box(FVector(0, 0, 0), FVector(1, 1, 1), GoalMeshYaw, nullptr, FLinearColor::White, GoalMesh);
		G->SetRelativeScale3D(FVector(1.0));
	}
	else
	{
		for (int s = -1; s <= 1; s += 2)
		{
			Cyl(FVector(s * PX, 0, 0), FVector(s * PX, CY, 0), FrameR, Orange);
			Cyl(FVector(s * PX, CY, 0), FVector(s * PX, CY, -.5f), FrameR * .8f, Orange);
			Cyl(FVector(s * PX, .025f, 0), FVector(s * PX, .025f, -.92f), FrameR * .8f, Orange);
			Cyl(FVector(s * PX, .025f, -.92f), FVector(s * PX, CY, -.5f), FrameR * .8f, Orange);
		}
		Cyl(FVector(-PX - FrameR, CY, 0), FVector(PX + FrameR, CY, 0), FrameR, Orange);
		Cyl(FVector(-PX, .025f, -.92f), FVector(PX, .025f, -.92f), FrameR * .8f, Orange);
		Cyl(FVector(-PX, CY, -.5f), FVector(PX, CY, -.5f), FrameR * .8f, Orange);
		BuildNet();
	}

	// ---- papelitos ----
	{
		UStaticMesh* CubeM = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		ConfISM.Add(MkISM(CubeM, PP::ColorMID(this, FLinearColor(.95f, .95f, .95f), .8f), false));
		ConfISM.Add(MkISM(CubeM, PP::ColorMID(this, FLinearColor(.3f, .6f, .9f), .8f), false));
	}

	// ---- post proceso (Lumen etc.) ----
	if (bSpawnPostProcess)
	{
		Pp = Mk<UPostProcessComponent>();
		Pp->bUnbound = true;
		FPostProcessSettings& S = Pp->Settings;
		S.bOverride_AutoExposureMethod = true; S.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		S.bOverride_AutoExposureBias = true; S.AutoExposureBias = 0.f;
		S.bOverride_AutoExposureMinBrightness = true; S.AutoExposureMinBrightness = 4.f;     // EV100 (rango extendido, ver DefaultEngine.ini)
		S.bOverride_AutoExposureMaxBrightness = true; S.AutoExposureMaxBrightness = 12.f;
		S.bOverride_VignetteIntensity = true; S.VignetteIntensity = .3f;
		Pp->RegisterComponent();
	}
	ApplyQuality(CurQ);
}
