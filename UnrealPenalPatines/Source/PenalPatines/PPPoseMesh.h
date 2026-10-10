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
		BoneSpaceTransforms = Local;
		MarkRefreshTransformDirty();
		RefreshBoneTransforms(nullptr);
	}
};
