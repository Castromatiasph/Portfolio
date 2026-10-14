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
	FPPVec HandL = FPPVec::ZeroVector, HandR = FPPVec::ZeroVector;   // puntos del palo donde agarran la mano izquierda (abajo) y la derecha (arriba)
	FPPVec StickDir = FPPVec(0.f, -1.f, 0.f);                          // dirección del palo; la mano cierra el puño alrededor de él
	FPPVec AnkleL = FPPVec::ZeroVector, AnkleR = FPPVec::ZeroVector;
	float FootPitchL = 0.f, FootPitchR = 0.f;   // punta hacia abajo (rad)
	// ---- opcionales (arquero) ----
	float HipZ = 0.f;                           // pelvis hacia atrás (+) o adelante (-)
	FPPVec KneePole[2] = { FPPVec::ZeroVector, FPPVec::ZeroVector };    // hacia dónde apunta cada rodilla (cero = adelante)
	FPPVec ToeDir[2] = { FPPVec::ZeroVector, FPPVec::ZeroVector };      // hacia dónde apunta cada pie (cero = adelante)
	int32 HandMode[2] = { 0, 0 };               // 0 = agarra el palo, 1 = mano abierta (guante)
	FPPVec PalmDir[2] = { FPPVec::ZeroVector, FPPVec::ZeroVector };     // mano abierta: hacia dónde mira la palma
	FPPVec FingerDir[2] = { FPPVec::ZeroVector, FPPVec::ZeroVector };   // mano abierta: hacia dónde apuntan los dedos
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
	bool bHands = false;                 // se encontraron los huesos de los dedos
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
	int32 BoneKnee(int32 Side) const { return LL[Side]; }
	int32 BoneToe(int32 Side) const { return Toe[Side]; }
	FVector ModelFwd() const { return Fwd; }
	FVector ModelUp() const { return Up; }
	float ModelScale() const { return S; }            // unidades del modelo por cm
	/** Posición y ejes (adelante/arriba del modelo, girados con el hueso) de un hueso, en espacio del componente. */
	void BoneFrame(int32 B, FVector& OutPos, FVector& OutFwd, FVector& OutUp) const;
	/** Centro de la palma y ejes de la mano (dedos, palma, nudillos), en espacio del componente. */
	void HandFrame(int32 Side, FVector& OutPalm, FVector& OutF, FVector& OutN, FVector& OutA) const;

private:
	UPPPoseMesh* Mesh = nullptr;
	TArray<FTransform> RefLocal, L, CS;
	TArray<int32> Parent;
	int32 Root = -1, Sp05 = -1, Sp03 = -1, Sp01 = -1, Neck = -1, Head = -1;
	int32 Fing[2][5][3];                 // dedos: [lado][pulgar..meñique][falange]
	FVector HandF[2], HandN[2], HandA[2]; // ejes de la mano en el espacio local de la muñeca: dedos, palma, nudillos
	float GripAlong = 6.f, GripOut = 2.2f;
	int32 UA[2] = { -1, -1 }, LA[2] = { -1, -1 }, Wr[2] = { -1, -1 }, UL[2] = { -1, -1 }, LL[2] = { -1, -1 }, Ft[2] = { -1, -1 }, Toe[2] = { -1, -1 };
	FVector Fwd = FVector::XAxisVector, Right = FVector::YAxisVector, Up = FVector::ZAxisVector, Origin = FVector::ZeroVector;
	float S = 1.f;                       // unidades del modelo por metro / 100
	float ArmA[2] = {}, ArmB[2] = {}, LegA[2] = {}, LegB[2] = {}, FootLen[2] = {}, FootDrop[2] = {};

	int32 Find(const TCHAR* Name) const;
	void FK();
	void Grip(int32 Side, const FVector& GripPoint, const FVector& StickDirC, const FVector& Pole);
	void Open(int32 Side, const FVector& PalmPoint, const FVector& PalmC, const FVector& FingerC, const FVector& Pole);
	FVector DirComp(const FPPVec& G) const { return (Right * G.X + Up * G.Y + Fwd * (-G.Z)).GetSafeNormal(); }
	TArray<FVector> RestF, RestU;        // adelante/arriba del modelo en el espacio local de cada hueso
	FVector Pos(int32 B) const { return CS[B].GetLocation(); }
	void RotateCS(int32 B, const FQuat& Q);
	void Aim(int32 B, int32 Child, const FVector& TargetChildPos);
	FVector ToComp(const FPPVec& G) const;
	FVector Solve(const FVector& Sh, const FVector& Hd, float A, float B, const FVector& Pole) const;
};
