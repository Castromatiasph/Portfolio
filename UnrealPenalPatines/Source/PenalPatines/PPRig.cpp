#include "PPRig.h"
#include "PPPoseMesh.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"

int32 FPPRig::Find(const TCHAR* Name) const
{
	if (!Mesh || !Mesh->GetSkinnedAsset()) return INDEX_NONE;
	const FReferenceSkeleton& RS = Mesh->GetSkinnedAsset()->GetRefSkeleton();
	FString N(Name);
	int32 I = RS.FindBoneIndex(FName(*N));
	if (I == INDEX_NONE) { FString A = N.Replace(TEXT("."), TEXT("_")); I = RS.FindBoneIndex(FName(*A)); }
	if (I == INDEX_NONE) { FString A = N.Replace(TEXT("."), TEXT("")); I = RS.FindBoneIndex(FName(*A)); }
	return I;
}

void FPPRig::FK()
{
	for (int32 i = 0; i < L.Num(); i++) CS[i] = Parent[i] < 0 ? L[i] : L[i] * CS[Parent[i]];
}

void FPPRig::RotateCS(int32 B, const FQuat& Q)
{
	if (B < 0) return;
	const FQuat ParentRot = Parent[B] < 0 ? FQuat::Identity : CS[Parent[B]].GetRotation();
	const FQuat NewCs = Q * CS[B].GetRotation();
	L[B].SetRotation((ParentRot.Inverse() * NewCs).GetNormalized());
	FK();
}

void FPPRig::Aim(int32 B, int32 Child, const FVector& Target)
{
	if (B < 0 || Child < 0) return;
	const FVector Cur = Pos(Child) - Pos(B), Des = Target - Pos(B);
	if (Cur.SizeSquared() < 1e-6 || Des.SizeSquared() < 1e-6) return;
	RotateCS(B, FQuat::FindBetweenVectors(Cur, Des));
}

FVector FPPRig::ToComp(const FPPVec& G) const
{
	return Origin + (Right * G.X + Up * G.Y + Fwd * (-G.Z)) * (100.0 * S);
}

FVector FPPRig::Solve(const FVector& Sh, const FVector& Hd, float A, float B, const FVector& Pole) const
{
	FVector D = Hd - Sh; double Len = D.Size();
	if (Len < 1e-4) return Sh + Pole.GetSafeNormal() * A;
	const FVector U = D / Len;
	Len = FMath::Clamp(Len, 0.05 * A, double(A + B) - 0.001);
	const double Along = (double(A) * A - double(B) * B + Len * Len) / (2.0 * Len);
	const double Hh = FMath::Sqrt(FMath::Max(double(A) * A - Along * Along, 0.0));
	FVector Perp = Pole - U * FVector::DotProduct(Pole, U);
	Perp = Perp.GetSafeNormal();
	return Sh + U * Along + Perp * Hh;
}

bool FPPRig::Init(UPPPoseMesh* M)
{
	Mesh = M; bOk = false;
	if (!M || !M->GetSkinnedAsset()) return false;
	const FReferenceSkeleton& RS = M->GetSkinnedAsset()->GetRefSkeleton();
	const int32 N = RS.GetNum();
	RefLocal = RS.GetRefBonePose();
	if (RefLocal.Num() != N) return false;
	Parent.SetNum(N); for (int32 i = 0; i < N; i++) Parent[i] = RS.GetParentIndex(i);
	L = RefLocal; CS.SetNum(N);
	Root = Find(TEXT("root")); Sp05 = Find(TEXT("spine05")); Sp03 = Find(TEXT("spine03")); Sp01 = Find(TEXT("spine01"));
	Neck = Find(TEXT("neck01")); Head = Find(TEXT("head"));
	const TCHAR* Sd[2] = { TEXT("L"), TEXT("R") };
	for (int s = 0; s < 2; s++)
	{
		auto F = [&](const TCHAR* Base) { return Find(*FString::Printf(TEXT("%s.%s"), Base, Sd[s])); };
		UA[s] = F(TEXT("upperarm01")); LA[s] = F(TEXT("lowerarm01")); Wr[s] = F(TEXT("wrist"));
		UL[s] = F(TEXT("upperleg01")); LL[s] = F(TEXT("lowerleg01")); Ft[s] = F(TEXT("foot")); Toe[s] = F(TEXT("toe1-1"));
	}
	const int32 Need[] = { Root, Sp05, Sp03, Sp01, Neck, Head, UA[0], UA[1], LA[0], LA[1], Wr[0], Wr[1], UL[0], UL[1], LL[0], LL[1], Ft[0], Ft[1], Toe[0], Toe[1] };
	for (int32 B : Need) if (B == INDEX_NONE) { UE_LOG(LogTemp, Warning, TEXT("[PenalPatines] Rig: falta un hueso del esqueleto de Matías; se usa el cuerpo provisorio.")); return false; }
	FK();

	// ejes del modelo: arriba = pies -> cabeza; adelante = tobillo -> punta del pie; derecha = hueso izquierdo -> derecho
	Up = (Pos(Head) - (Pos(Ft[0]) + Pos(Ft[1])) * .5).GetSafeNormal();
	FVector F = (Pos(Toe[0]) - Pos(Ft[0])) + (Pos(Toe[1]) - Pos(Ft[1]));
	Fwd = (F - Up * FVector::DotProduct(F, Up)).GetSafeNormal();
	FVector R = Pos(Ft[1]) - Pos(Ft[0]);
	Right = (R - Up * FVector::DotProduct(R, Up) - Fwd * FVector::DotProduct(R, Fwd)).GetSafeNormal();
	const FVector Cross = FVector::CrossProduct(Up, Fwd);
	if (FVector::DotProduct(Cross, Right) < 0.0) UE_LOG(LogTemp, Warning, TEXT("[PenalPatines] Rig: el modelo parece espejado (izquierda/derecha invertidas)."));

	const double FootU = FMath::Min(FVector::DotProduct(Pos(Ft[0]), Up), FVector::DotProduct(Pos(Ft[1]), Up));
	double Ground = FootU - 8.0;                                      // provisorio: tobillo ~8 cm sobre la suela (unidades en cm)
	const double HeadH = FVector::DotProduct(Pos(Head), Up) - Ground;
	S = float(HeadH / 163.0);                                         // la cabeza (hueso) está a ~1,63 m en un modelo de 1,75 m
	if (S < 1e-3f) S = 1.f;
	Ground = FootU - 8.0 * S;
	const FVector RootP = Pos(Root);
	Origin = RootP - Up * (FVector::DotProduct(RootP, Up) - Ground);
	PelvisRestH = float((FVector::DotProduct(RootP, Up) - Ground) / (100.0 * S));
	for (int s = 0; s < 2; s++)
	{
		ArmA[s] = float((Pos(LA[s]) - Pos(UA[s])).Size()); ArmB[s] = float((Pos(Wr[s]) - Pos(LA[s])).Size());
		LegA[s] = float((Pos(LL[s]) - Pos(UL[s])).Size()); LegB[s] = float((Pos(Ft[s]) - Pos(LL[s])).Size());
		const FVector T = Pos(Toe[s]) - Pos(Ft[s]);
		FootLen[s] = float(T.Size());
		const double Tv = FVector::DotProduct(T, Up);
		FootDrop[s] = float(FMath::Atan2(-Tv, (T - Up * Tv).Size()));    // en reposo la punta del pie está más baja que el tobillo
	}

	// dedos y ejes de la mano (para cerrar el puño sobre el palo); si falta algún hueso, la mano queda abierta
	bHands = true;
	for (int s = 0; s < 2; s++)
		for (int f = 0; f < 5; f++)
			for (int k = 0; k < 3; k++)
			{
				Fing[s][f][k] = Find(*FString::Printf(TEXT("finger%d-%d.%s"), f + 1, k + 1, Sd[s]));
				if (Fing[s][f][k] == INDEX_NONE) bHands = false;
			}
	if (bHands)
		for (int s = 0; s < 2; s++)
		{
			const FVector Fd = (Pos(Fing[s][2][0]) - Pos(Wr[s])).GetSafeNormal();                 // muñeca -> nudillo del medio
			FVector Nrm = FVector::ZeroVector;                                                      // la palma es hacia donde se curvan los dedos en reposo
			for (int f = 1; f < 5; f++)
			{
				const FVector A1 = (Pos(Fing[s][f][1]) - Pos(Fing[s][f][0])).GetSafeNormal(), A2 = (Pos(Fing[s][f][2]) - Pos(Fing[s][f][1])).GetSafeNormal();
				Nrm += A2 - A1;
			}
			Nrm = (Nrm - Fd * FVector::DotProduct(Nrm, Fd)).GetSafeNormal();
			FVector Ac = (Pos(Fing[s][4][0]) - Pos(Fing[s][1][0]));
			Ac = (Ac - Fd * FVector::DotProduct(Ac, Fd) - Nrm * FVector::DotProduct(Ac, Nrm)).GetSafeNormal();
			if (Nrm.IsNearlyZero() || Ac.IsNearlyZero()) { bHands = false; break; }
			const FQuat Inv = CS[Wr[s]].GetRotation().Inverse();
			HandF[s] = Inv.RotateVector(Fd); HandN[s] = Inv.RotateVector(Nrm); HandA[s] = Inv.RotateVector(Ac);
		}
	GripAlong = 6.5f * S; GripOut = 2.2f * S;      // desde la muñeca: 6,5 cm hacia los dedos y 2,2 cm hacia la palma (unidades del modelo en cm)

	// transformación del componente: ejes del modelo -> ejes del actor (X adelante, Y derecha, Z arriba)
	const FQuat Basis = FRotationMatrix::MakeFromXZ(Fwd, Up).ToQuat();
	CompRot = Basis.Inverse();
	CompScale = 1.f / S;
	CompLoc = -(CompRot * (Origin * double(CompScale)));
	bOk = true;
	UE_LOG(LogTemp, Log, TEXT("[PenalPatines] Rig OK: huesos=%d escala=%.3f pelvis=%.2f m"), N, S, PelvisRestH);
	return true;
}

void FPPRig::Pose(const FPPRigTargets& T)
{
	if (!bOk || !Mesh) return;
	L = RefLocal; FK();
	// pelvis
	{
		const FVector Hip = Origin + Up * (T.HipHeight * 100.0 * S) + Right * (T.HipX * 100.0 * S);
		const FTransform ParentCs = Parent[Root] < 0 ? FTransform::Identity : CS[Parent[Root]];
		L[Root].SetLocation(ParentCs.InverseTransformPosition(Hip));
		FK();
	}
	// torso: inclinación hacia adelante repartida en la columna y cabeza que compensa para mirar al frente
	auto Tilt = [&](int32 B, float Ang) { RotateCS(B, FQuat::FindBetweenNormals(Up, (Up * FMath::Cos(Ang) + Fwd * FMath::Sin(Ang)).GetSafeNormal())); };
	const float A = T.Lean;
	Tilt(Sp05, A * .35f); Tilt(Sp03, A * .35f); Tilt(Sp01, A * .3f);
	Tilt(Neck, -A * .45f); Tilt(Head, -A * .4f);
	// giro de hombros
	auto Twist = [&](int32 B, float Ang) { RotateCS(B, FQuat::FindBetweenNormals(Right, (Right * FMath::Cos(Ang) + Fwd * FMath::Sin(Ang)).GetSafeNormal())); };
	Twist(Sp03, T.Twist * .5f); Twist(Sp01, T.Twist * .5f);
	// brazos: las manos agarran el palo (muñeca y puño calculados para que el palo pase por la palma)
	const FPPVec Hands[2] = { T.HandL, T.HandR };
	const FVector StickC = (Right * T.StickDir.X + Up * T.StickDir.Y + Fwd * (-T.StickDir.Z)).GetSafeNormal();
	for (int s = 0; s < 2; s++)
	{
		const double Side = s == 0 ? -1.0 : 1.0;
		Grip(s, ToComp(Hands[s]), StickC, Right * (Side * .7) - Up * .4 - Fwd * .6);
	}
	// piernas: rodillas hacia adelante; el pie queda horizontal (o con la punta hacia abajo al levantarlo)
	const FPPVec Ankles[2] = { T.AnkleL, T.AnkleR };
	const float Pitch[2] = { T.FootPitchL, T.FootPitchR };
	for (int s = 0; s < 2; s++)
	{
		const double Side = s == 0 ? -1.0 : 1.0;
		const FVector At = ToComp(Ankles[s]);
		const FVector K = Solve(Pos(UL[s]), At, LegA[s], LegB[s], Fwd + Right * (Side * .15));
		Aim(UL[s], LL[s], K);
		Aim(LL[s], Ft[s], At);
		const float Pa = Pitch[s] + FootDrop[s];
		const FVector ToeDir = (Fwd * FMath::Cos(Pa) - Up * FMath::Sin(Pa)).GetSafeNormal();
		Aim(Ft[s], Toe[s], Pos(Ft[s]) + ToeDir * double(FootLen[s]));
	}
	Mesh->SetLocalPose(L);
}

void FPPRig::Grip(int32 Side, const FVector& GripPoint, const FVector& StickDirC, const FVector& Pole)
{
	if (!bHands)
	{
		const FVector H = GripPoint;
		Aim(UA[Side], LA[Side], Solve(Pos(UA[Side]), H, ArmA[Side], ArmB[Side], Pole));
		Aim(LA[Side], Wr[Side], H);
		return;
	}
	for (int Iter = 0; Iter < 2; Iter++)
	{
		FQuat Rw = CS[Wr[Side]].GetRotation();
		const FVector Off = Rw.RotateVector(HandF[Side]) * GripAlong + Rw.RotateVector(HandN[Side]) * GripOut;
		const FVector WristT = GripPoint - Off;
		Aim(UA[Side], LA[Side], Solve(Pos(UA[Side]), WristT, ArmA[Side], ArmB[Side], Pole));
		Aim(LA[Side], Wr[Side], WristT);
		// la línea de los nudillos queda paralela al palo (el palo atraviesa el puño)
		Rw = CS[Wr[Side]].GetRotation();
		const FVector Ac = Rw.RotateVector(HandA[Side]);
		const FVector Want = StickDirC * (FVector::DotProduct(Ac, StickDirC) >= 0.0 ? 1.0 : -1.0);
		RotateCS(Wr[Side], FQuat::FindBetweenNormals(Ac, Want));
	}
	// cerrar los dedos hacia la palma
	const FQuat Rw = CS[Wr[Side]].GetRotation();
	FVector Nrm = Rw.RotateVector(HandN[Side]);
	static const float Curl[5][3] = { { .45f, .7f, .5f }, { 1.2f, 1.4f, 1.0f }, { 1.25f, 1.4f, 1.0f }, { 1.25f, 1.4f, 1.0f }, { 1.2f, 1.4f, 1.0f } };
	for (int f = 0; f < 5; f++)
	{
		FVector N = Nrm;
		for (int k = 0; k < 3; k++)
		{
			const int32 B = Fing[Side][f][k];
			const FVector Dv = (k < 2 ? Pos(Fing[Side][f][k + 1]) - Pos(B) : Pos(B) - Pos(Fing[Side][f][k - 1])).GetSafeNormal();
			FVector Ax = FVector::CrossProduct(Dv, N).GetSafeNormal();
			if (Ax.IsNearlyZero()) continue;
			const FQuat Q(Ax, Curl[f][k]);
			RotateCS(B, Q);
			N = Q.RotateVector(N);
		}
	}
}
