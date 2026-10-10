#include "PPArena.h"
#include "PPTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

using namespace PP;

APPArena::APPArena()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void APPArena::OnConstruction(const FTransform& Xf) { Super::OnConstruction(Xf); Build(); }

UMaterialInterface* APPArena::Tint(UMaterialInterface* Override, const FLinearColor& Col)
{
	if (Override) return Override;
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Base) return nullptr;
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
	M->SetVectorParameterValue(TEXT("Color"), Col);
	return M;
}

UStaticMeshComponent* APPArena::Box(const FVector& G, const FVector& S, float YawDeg, UMaterialInterface* Mat, const FLinearColor& Col, UStaticMesh* Mesh)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(RootComponent);
	C->RegisterComponent();
	UStaticMesh* M = Mesh ? Mesh : LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	C->SetStaticMesh(M);
	C->SetWorldLocation(ToUE(FPPVec(G.X, G.Y, G.Z)));
	C->SetWorldRotation(FRotator(0.f, YawDeg, 0.f));
	C->SetWorldScale3D(FVector(S.Z, S.X, S.Y));      // m -> escala del cubo de 100 cm (X adelante = z de juego)
	if (!Mesh) C->SetMaterial(0, Tint(Mat, Col));   // un mesh importado conserva sus materiales
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Built.Add(C);
	return C;
}

UStaticMeshComponent* APPArena::Cyl(const FVector& A, const FVector& B, float R, const FLinearColor& Col)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(RootComponent);
	C->RegisterComponent();
	C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	const FVector Ua = ToUE(FPPVec(A.X, A.Y, A.Z)), Ub = ToUE(FPPVec(B.X, B.Y, B.Z));
	C->SetWorldLocation((Ua + Ub) * .5);
	C->SetWorldRotation(FRotationMatrix::MakeFromZ(Ub - Ua).Rotator());
	C->SetWorldScale3D(FVector(R * 2.0, R * 2.0, (Ub - Ua).Size() / 100.0));
	C->SetMaterial(0, Tint(nullptr, Col));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Built.Add(C);
	return C;
}

void APPArena::Build()
{
	for (auto& C : Built) if (C) C->DestroyComponent();
	Built.Reset();

	// piso (parquet) — 20 m de ancho, de la valla del fondo (z = -3) a la mitad de la pista (z = 17)
	const float L = MidZ - EndZ;
	Box(FVector(0, -.025f, EndZ + L * .5f), FVector(HalfW * 2.f + .4f, .05f, L + .4f), 0.f, FloorMaterial, FLinearColor(.55f, .38f, .2f));

	// líneas de 8 cm: línea de gol, área de 9 × 5,40 m, semicírculo del arquero, puntos de penal y de libre
	const FLinearColor White(.95f, .96f, .98f);
	auto Line = [&](float X1, float Z1, float X2, float Z2)
	{
		const float Dx = X2 - X1, Dz = Z2 - Z1, Len = Hypot(Dx, Dz);
		// yaw de Unreal para un tramo en espacio de juego: el eje X de Unreal es -z de juego
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dx, -Dz));
		Box(FVector((X1 + X2) * .5f, .002f, (Z1 + Z2) * .5f), FVector(.08f, .004f, Len), Yaw - 0.f, LineMaterial, White);
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

	// vallas de 1 m con esquinas redondeadas de 3 m (tramos rectos aproximando el arco)
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
		Box(FVector((A.X + B.X) * .5f, .5f, (A.Y + B.Y) * .5f), FVector(.08f, 1.f, Len + .02f), Yaw, BoardMaterial, FLinearColor(.88f, .9f, .94f));
		Box(FVector((A.X + B.X) * .5f, 1.03f, (A.Y + B.Y) * .5f), FVector(.1f, .07f, Len + .03f), Yaw, BoardMaterial, FLinearColor(.06f, .1f, .2f));
	}

	// gradas de 4 niveles detrás de la valla del fondo y de las laterales
	const FLinearColor StandCol(.12f, .13f, .17f);
	for (int r = 0; r < 4; r++)
	{
		const float Off = .6f + r * 1.13f, H = .45f + r * .4f;
		Box(FVector(0, H * .5f, EndZ - Off - .55f), FVector(HalfW * 2.f + 6.f, H, 1.13f), 0.f, StandMaterial, StandCol);
		Box(FVector(-HalfW - Off - .55f, H * .5f, (EndZ + MidZ) * .5f), FVector(1.13f, H, L), 0.f, StandMaterial, StandCol);
		Box(FVector(HalfW + Off + .55f, H * .5f, (EndZ + MidZ) * .5f), FVector(1.13f, H, L), 0.f, StandMaterial, StandCol);
	}

	// techo y luminarias
	Box(FVector(0, 10.6f, (EndZ + MidZ) * .5f), FVector(HalfW * 2.f + 12.f, .3f, L + 8.f), 0.f, StandMaterial, FLinearColor(.05f, .05f, .06f));
	for (int i = -1; i <= 1; i++) for (int j = 0; j < 5; j++)
	{
		URectLightComponent* Rl = NewObject<URectLightComponent>(this);
		Rl->SetMobility(EComponentMobility::Movable);
		Rl->SetupAttachment(RootComponent);
		Rl->RegisterComponent();
		Rl->SetWorldLocation(ToUE(FPPVec(i * 7.f, 10.3f, -2.f + j * 7.f)));
		Rl->SetWorldRotation(FRotator(-90.f, 0.f, 0.f));
		Rl->SetSourceWidth(180.f); Rl->SetSourceHeight(60.f);
		Rl->SetIntensityUnits(ELightUnits::Lumens);
		Rl->SetIntensity(LightLumens);
		Rl->SetLightColor(FLinearColor(1.f, .93f, .84f));
		Rl->SetAttenuationRadius(2400.f);
		Rl->SetCastShadows(true);
		// luminaria visible
		Box(FVector(i * 7.f, 10.45f, -2.f + j * 7.f), FVector(1.8f, .05f, .6f), 0.f, nullptr, FLinearColor(8.f, 7.4f, 6.6f));
	}

	// arco reglamentario: 1,70 × 1,05 m, naranja fluorescente
	const FLinearColor Orange(1.f, .27f, .04f);
	if (GoalMesh)
	{
		UStaticMeshComponent* G = Box(FVector(0, 0, 0), FVector(1, 1, 1), GoalMeshYaw, nullptr, FLinearColor::White, GoalMesh);
		G->SetWorldScale3D(FVector(1.0));
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
	}

	if (bSpawnPostProcess && !FindComponentByClass<UPostProcessComponent>())
	{
		UPostProcessComponent* Pp = NewObject<UPostProcessComponent>(this, TEXT("PostProcess"));
		Pp->SetupAttachment(RootComponent);
		Pp->RegisterComponent();
		Pp->bUnbound = true;
		FPostProcessSettings& S = Pp->Settings;
		S.bOverride_DynamicGlobalIlluminationMethod = true; S.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
		S.bOverride_ReflectionMethod = true; S.ReflectionMethod = EReflectionMethod::Lumen;
		S.bOverride_LumenSceneLightingQuality = true; S.LumenSceneLightingQuality = 2.f;
		S.bOverride_LumenFinalGatherQuality = true; S.LumenFinalGatherQuality = 2.f;
		S.bOverride_LumenSceneDetail = true; S.LumenSceneDetail = 2.f;
		S.bOverride_AutoExposureMethod = true; S.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		S.bOverride_AutoExposureBias = true; S.AutoExposureBias = 11.5f;                 // EV100 manual: interior iluminado
		S.bOverride_BloomIntensity = true; S.BloomIntensity = .6f;
		S.bOverride_MotionBlurAmount = true; S.MotionBlurAmount = .35f;
		S.bOverride_VignetteIntensity = true; S.VignetteIntensity = .35f;
		S.bOverride_FilmGrainIntensity = true; S.FilmGrainIntensity = .12f;
		S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = .5f;
		S.bOverride_ScreenSpaceReflectionQuality = true; S.ScreenSpaceReflectionQuality = 100.f;
	}
	if (!FindComponentByClass<USkyLightComponent>())
	{
		USkyLightComponent* Sky = NewObject<USkyLightComponent>(this, TEXT("SkyLight"));
		Sky->SetMobility(EComponentMobility::Movable);
		Sky->SetupAttachment(RootComponent);
		Sky->RegisterComponent();
		Sky->bRealTimeCapture = true;
		Sky->SetIntensity(.35f);
	}
}
