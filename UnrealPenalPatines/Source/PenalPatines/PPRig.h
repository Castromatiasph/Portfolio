#pragma once

#include "CoreMinimal.h"
#include "PPTypes.h"

class UPPPoseMesh;

/** Objetivos de la pose, en el espacio del juego (metros; x derecha, y arriba, z atrás) relativos al piso bajo el jugador. */
struct FPPRigTargets
{
	float HipHeight = 1.f;            // altura de la pelvis
	float HipX = 0.f;
	float Lean = 0.f;                 // inclinación del torso hacia adelante (rad)
	float Twist = 0.f;                // giro de hombros (rad, positivo = hombro derecho adelante)
	FPPVec HandL = FPPVec::ZeroVector, HandR = FPPVec::ZeroVector;
	FPPVec AnkleL = FPPVec::ZeroVector, AnkleR = FPPVec::ZeroVector;
	float FootPitchL = 0.f, FootPitchR = 0.f;   // punta hacia abajo (rad)
};

/**
 * Lleva el esqueleto de MakeHuman del jugador (SK_Matias) a una pose calculada por código: pelvis, inclinación y giro
 * del torso, brazos y piernas por IK de dos huesos. No necesita AnimBP ni animaciones: es el mismo método que usaba el
 * juego web (aimBone + ik). Detecta solo los ejes del modelo (adelante, arriba, derecha) y su escala, así que funciona
 * aunque el importador de Unreal oriente o escale distinto el archivo.
 */
class FPPRig
{
public:
	bool bOk = false;
	FQuat CompRot = FQuat::Identity;     // rotación del componente para que el modelo mire a +X del actor
	FVector CompLoc = FVector::ZeroVector;
	float CompScale = 1.f;
	float PelvisRestH = .93f;            // altura de la pelvis del modelo en reposo (m)

	bool Init(UPPPoseMesh* M);
	int32 BoneHead() const { return Head; }
	int32 BoneWrist(int32 Side) const { return Wr[Side]; }
	int32 BoneFoot(int32 Side) const { return Ft[Side]; }
	int32 BoneRoot() const { return Root; }
	void Pose(const FPPRigTargets& T);

private:
	UPPPoseMesh* Mesh = nullptr;
	TArray<FTransform> RefLocal, L, CS;
	TArray<int32> Parent;
	int32 Root = -1, Sp05 = -1, Sp03 = -1, Sp01 = -1, Neck = -1, Head = -1;
	int32 UA[2] = { -1, -1 }, LA[2] = { -1, -1 }, Wr[2] = { -1, -1 }, UL[2] = { -1, -1 }, LL[2] = { -1, -1 }, Ft[2] = { -1, -1 }, Toe[2] = { -1, -1 };
	FVector Fwd = FVector::XAxisVector, Right = FVector::YAxisVector, Up = FVector::ZAxisVector, Origin = FVector::ZeroVector;
	float S = 1.f;                       // unidades del modelo por metro / 100
	float ArmA[2] = {}, ArmB[2] = {}, LegA[2] = {}, LegB[2] = {}, FootLen[2] = {};

	int32 Find(const TCHAR* Name) const;
	void FK();
	FVector Pos(int32 B) const { return CS[B].GetLocation(); }
	void RotateCS(int32 B, const FQuat& Q);
	void Aim(int32 B, int32 Child, const FVector& TargetChildPos);
	FVector ToComp(const FPPVec& G) const;
	FVector Solve(const FVector& Sh, const FVector& Hd, float A, float B, const FVector& Pole) const;
};
