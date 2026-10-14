#pragma once

#include "CoreMinimal.h"
#include "PPRig.h"

class AActor;
class UPPPoseMesh;
class UPoseableMeshComponent;
class UStaticMesh;

namespace PP
{
	/**
	 * Carga a Matías (SK_Matias y las mallas que el importador haya separado: pelo, ojos, cejas...), lo viste con la textura
	 * BaseTex (T_Matias_BaseColor = jugador, T_Arquero_BaseColor = arquero) e inicializa el rig. El que llama ubica el componente.
	 * Devuelve true si el esqueleto quedó listo para posar.
	 */
	bool SetupHuman(AActor* Owner, UPPPoseMesh* Body, TArray<TObjectPtr<UPoseableMeshComponent>>& Extra, FPPRig& Rig, const TCHAR* BaseTex, const TCHAR* Who);

	/**
	 * Ejes de un .glb (glTF: +X costado, +Y arriba, +Z adelante) dentro de una malla importada por Unreal.
	 * Se averiguan con el palo (SM_Palo: caña hacia -Y, pala hacia +Z), así no depende de cómo convierta el importador.
	 */
	struct FGltfAxes
	{
		FVector X = FVector(1, 0, 0), Y = FVector(0, 0, 1), Z = FVector(0, 1, 0);
		float Scale = 1.f;   // unidades de la malla por cm reales
		void FromStick(const FVector& TipLocal, const FVector& ToeLocal, float StickScale)
		{
			Y = -TipLocal.GetSafeNormal(); Z = ToeLocal.GetSafeNormal();
			X = -FVector::CrossProduct(Y, Z);   // el paso de glTF a Unreal es un espejo (mano derecha -> izquierda)
			Scale = StickScale;
		}
		/** Transformación de mundo para que los ejes glTF Y/Z de la malla queden en Yw/Zw; SizeK escala cada eje glTF (x, y, z). */
		FTransform Make(const FVector& Pos, const FVector& Yw, const FVector& Zw, const FVector& SizeK = FVector(1)) const
		{
			const FVector Yn = Yw.GetSafeNormal();
			FVector Zn = (Zw - Yn * FVector::DotProduct(Zw, Yn)).GetSafeNormal();
			if (Zn.IsNearlyZero()) Zn = FVector::CrossProduct(Yn, FVector::UpVector).GetSafeNormal();
			const FVector Xn = -FVector::CrossProduct(Yn, Zn);
			// R(e_k) = Xn * X[k] + Yn * Y[k] + Zn * Z[k]
			FVector R[3];
			for (int k = 0; k < 3; k++) R[k] = Xn * X[k] + Yn * Y[k] + Zn * Z[k];
			const FMatrix M(FPlane(R[0], 0.0), FPlane(R[1], 0.0), FPlane(R[2], 0.0), FPlane(0.0, 0.0, 0.0, 1.0));
			// escala por eje local de la malla
			FVector S3;
			for (int k = 0; k < 3; k++) S3[k] = (FMath::Abs(X[k]) * SizeK.X + FMath::Abs(Y[k]) * SizeK.Y + FMath::Abs(Z[k]) * SizeK.Z) * Scale;
			return FTransform(M.ToQuat(), Pos, S3);
		}
	};
}
