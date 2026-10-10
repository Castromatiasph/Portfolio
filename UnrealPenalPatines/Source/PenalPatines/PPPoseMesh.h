#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "PPPoseMesh.generated.h"

/** PoseableMesh que acepta una pose completa en espacio local (relativa al padre) calculada por código. */
UCLASS()
class PENALPATINES_API UPPPoseMesh : public UPoseableMeshComponent
{
	GENERATED_BODY()
public:
	void SetLocalPose(const TArray<FTransform>& Local)
	{
		for (const FTransform& T : Local) if (T.ContainsNaN()) return;   // pose inválida: se conserva la anterior
		BoneSpaceTransforms = Local;
		MarkRefreshTransformDirty();
		RefreshBoneTransforms(nullptr);
	}
	/** Posición en el mundo de un hueso (espacio de componente -> mundo). */
	FVector BoneWorld(int32 BoneIndex) const
	{
		const TArray<FTransform>& CS = GetComponentSpaceTransforms();
		return CS.IsValidIndex(BoneIndex) ? GetComponentTransform().TransformPosition(CS[BoneIndex].GetLocation()) : FVector::ZeroVector;
	}
};
