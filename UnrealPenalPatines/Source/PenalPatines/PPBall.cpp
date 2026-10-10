#include "PPBall.h"
#include "PPTypes.h"
#include "PPMaterial.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

APPBall::APPBall()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // la física es propia (ver PPGameMode::StepBall)
	Mesh->SetCastShadow(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) Mesh->SetStaticMesh(Sphere.Object);
	// la esfera básica mide 100 cm de diámetro; la bocha, 7,3 cm
	Mesh->SetWorldScale3D(FVector(PP::BallR * 2.f));
}

void APPBall::BeginPlay()
{
	Super::BeginPlay();
	Mesh->SetMaterial(0, PP::ColorMID(this, FLinearColor(.02f, .02f, .025f), .25f));   // bocha negra y brillante
}

void APPBall::SyncActor()
{
	SetActorLocation(PP::ToUE(Pos));
}

void APPBall::RollBy(const FPPVec& Delta)
{
	if (Delta.SizeSquared() < 1e-9f) return;
	const FPPVec Axis = FPPVec(Delta.Z, 0.f, -Delta.X).GetSafeNormal();     // eje en espacio de juego
	const float Ang = PP::Hypot(Delta.X, Delta.Z) / PP::BallR;
	const FVector AxisUE = PP::ToUE(Axis) / 100.0;                              // misma dirección, ya en Unreal
	AddActorWorldRotation(FQuat(AxisUE.GetSafeNormal(), Ang));
}

void APPBall::ApplySpin(float Dt)
{
	const float W = Spin.Size();
	if (W < 1e-3f) return;
	const FVector AxisUE = (PP::ToUE(Spin / W) / 100.0).GetSafeNormal();
	AddActorWorldRotation(FQuat(AxisUE, W * Dt));
}
