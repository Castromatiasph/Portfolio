#include "PPHuman.h"
#include "PPPoseMesh.h"
#include "PPAssets.h"
#include "PPMaterial.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Components/PoseableMeshComponent.h"
#include "GameFramework/Actor.h"

bool PP::SetupHuman(AActor* Owner, UPPPoseMesh* Body, TArray<TObjectPtr<UPoseableMeshComponent>>& Extra, FPPRig& Rig, const TCHAR* BaseTex, const TCHAR* Who)
{
	if (!Owner || !Body) return false;
	// El importador puede separar a Matías en varias mallas (cuerpo, pelo, ojos...) que comparten esqueleto: la más grande es el cuerpo y el resto la sigue.
	TArray<USkeletalMesh*> Parts = PP::FindAssets<USkeletalMesh>(TEXT("/Game/Models"));
	Parts.RemoveAll([](USkeletalMesh* M) { return !M || M->GetName().Contains(TEXT("Jugador")); });
	USkeletalMesh* SK = nullptr;
	for (USkeletalMesh* M : Parts) if (!SK || M->GetBounds().BoxExtent.Z > SK->GetBounds().BoxExtent.Z) SK = M;
	if (!SK)
	{
		PP::ScreenWarn(TEXT("No encontré a Matías (SK_Matias) en Content/Models. Corré Tools/setup_level.py o importá SourceAssets/Models/SK_Matias.glb."));
		Body->SetVisibility(false);
		return false;
	}
	auto Tex = [](const TCHAR* N) -> UTexture* { return PP::FindAsset<UTexture2D>(TEXT("/Game/Textures"), N); };
	UTexture* BodyTex = Tex(BaseTex);
	if (!BodyTex) BodyTex = Tex(TEXT("T_Matias_BaseColor"));
	auto Dress = [&](UPoseableMeshComponent* C, USkeletalMesh* Asset)
	{
		C->SetSkinnedAssetAndUpdate(Asset, true);
		const TArray<FName> Slots = C->GetMaterialSlotNames();
		for (int32 i = 0; i < Slots.Num(); i++)
		{
			const FString N = (Slots[i].ToString() + TEXT(" ") + Asset->GetName()).ToLower();
			UMaterialInterface* M = nullptr;
			if (N.Contains(TEXT("eyebrow"))) M = PP::CutoutMID(Owner, Tex(TEXT("T_Matias_Brow")), FLinearColor(.1f, .08f, .07f));
			else if (N.Contains(TEXT("eyelash"))) M = PP::CutoutMID(Owner, Tex(TEXT("T_Matias_Lash")), FLinearColor(.05f, .05f, .05f));
			else if (N.Contains(TEXT("short"))) M = PP::CutoutMID(Owner, Tex(TEXT("T_Matias_Hair")), FLinearColor::White, .45f);
			else if (N.Contains(TEXT("high-poly")) || N.Contains(TEXT("high_poly")) || N.Contains(TEXT("highpoly"))) M = PP::ColorMID(Owner, FLinearColor::White, .08f, FLinearColor::Black, Tex(TEXT("T_Matias_Eye")), 1.f, 1.f);
			else if (N.Contains(TEXT("teeth"))) M = PP::ColorMID(Owner, FLinearColor::White, .35f, FLinearColor::Black, Tex(TEXT("T_Matias_Teeth")));
			else if (N.Contains(TEXT("tongue"))) M = PP::ColorMID(Owner, FLinearColor::White, .5f, FLinearColor::Black, Tex(TEXT("T_Matias_Tongue")));
			else M = PP::ColorMID(Owner, FLinearColor::White, .55f, FLinearColor::Black, BodyTex);   // cuerpo ("body" / "base")
			if (M) C->SetMaterial(i, M);
		}
	};
	Dress(Body, SK);
	for (USkeletalMesh* M : Parts)
	{
		if (M == SK) continue;
		UPoseableMeshComponent* X = NewObject<UPoseableMeshComponent>(Owner);
		X->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		X->RegisterComponent();
		X->AttachToComponent(Body, FAttachmentTransformRules::KeepRelativeTransform);
		Dress(X, M);
		X->SetLeaderPoseComponent(Body);
		X->SetBoundsScale(4.f);
		Extra.Add(X);
	}
	if (!Rig.Init(Body))
	{
		Body->SetVisibility(false);
		for (auto& X : Extra) if (X) X->SetVisibility(false);
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[PenalPatines] %s: %d malla(s), cuerpo = %s, %d huesos"), Who, Parts.Num(), *SK->GetName(), SK->GetRefSkeleton().GetNum());
	Body->SetBoundsScale(4.f);
	return true;
}
