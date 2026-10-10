#pragma once

#include "CoreMinimal.h"
#include "PPTypes.generated.h"

/** Vectores del espacio de juego: de precisión simple (FVector de UE5 es double y FMath::Clamp/Min no mezclan tipos). */
typedef FVector3f FPPVec;
typedef FVector2f FPPVec2;

/*
 * Todo el modelo de juego se simula en "espacio de juego" (el mismo del juego web, en metros):
 *   x = derecha, y = arriba, z = hacia el que patea (el arco está en z = 0 y mira hacia +z).
 * Recién al mover los actores se convierte a espacio de Unreal (cm, X adelante, Y derecha, Z arriba):
 *   UE.X = -z*100,  UE.Y = x*100,  UE.Z = y*100
 * El que patea mira hacia +X de Unreal y el arco queda en el origen del mundo.
 */
namespace PP
{
	constexpr float BallR = 0.0365f;
	constexpr float Grav = 9.8f;
	constexpr float SpotZ = 5.4f;      // penal
	constexpr float FreeZ = 7.4f;      // tiro libre directo
	constexpr float PostHalf = 0.85f;
	constexpr float BarH = 1.05f;
	constexpr float FrameR = 0.026f;
	constexpr float EndZ = -3.0f;
	constexpr float CornerR = 3.0f;
	constexpr float HalfW = 10.0f;
	constexpr float MidZ = EndZ + 20.0f;
	constexpr float PX = PostHalf + FrameR;
	constexpr float CY = BarH + FrameR;
	constexpr float GoalBack = -0.98f;
	constexpr float GoalSide = PX + FrameR;
	constexpr float MaxSpeed = 5.2f;   // ~19 km/h
	constexpr float Accel = 8.5f;
	constexpr float Glide = 3.4f;

	FORCEINLINE FVector ToUE(const FPPVec& G) { return FVector(-G.Z * 100.f, G.X * 100.f, G.Y * 100.f); }
	FORCEINLINE FPPVec FromUE(const FVector& U) { return FPPVec(float(U.Y / 100.), float(U.Z / 100.), float(-U.X / 100.)); }
	FORCEINLINE float Hypot(float A, float B) { return FMath::Sqrt(A * A + B * B); }
	/** Vallas con esquinas redondeadas de 3 m. Corrige P y devuelve true (con la normal) si tocó la valla. */
	inline bool InsideRink(FPPVec& P, float Margin, float Blk, FPPVec& OutN)
	{
		const float Hx = HalfW - Margin, Cz = EndZ + CornerR, Cx = HalfW - CornerR, R = CornerR - Margin;
		bool bHit = false;
		if (P.Z < Cz && FMath::Abs(P.X) > Cx)
		{
			const float Sx = P.X < 0.f ? -1.f : 1.f;
			const float Dx = P.X - Sx * Cx, Dz = P.Z - Cz, D = FMath::Sqrt(Dx * Dx + Dz * Dz);
			if (D > R) { P.X = Sx * Cx + Dx / D * R; P.Z = Cz + Dz / D * R; OutN = FPPVec(-Dx / D, 0.f, -Dz / D); bHit = true; }
		}
		else
		{
			if (FMath::Abs(P.X) > Hx) { const float Sx = P.X < 0.f ? -1.f : 1.f; P.X = Sx * Hx; OutN = FPPVec(-Sx, 0.f, 0.f); bHit = true; }
			if (P.Z < EndZ + Margin) { P.Z = EndZ + Margin; OutN = FPPVec(0.f, 0.f, 1.f); bHit = true; }
		}
		if (P.Z > MidZ + Blk) { P.Z = MidZ + Blk; OutN = FPPVec(0.f, 0.f, -1.f); bHit = true; }
		return bHit;
	}
	FORCEINLINE float Gauss()
	{
		float u = 0.f, v = 0.f;
		while (u <= KINDA_SMALL_NUMBER) u = FMath::FRand();
		while (v <= KINDA_SMALL_NUMBER) v = FMath::FRand();
		return FMath::Sqrt(-2.f * FMath::Loge(u)) * FMath::Cos(2.f * PI * v);
	}
	FORCEINLINE float Smooth(float t) { return t * t * (3.f - 2.f * t); }
	FORCEINLINE float PowDt(float base, float dt) { return 1.f - FMath::Pow(base, dt); } // lerp alpha independiente del framerate
}

UENUM(BlueprintType) enum class EPPFormat : uint8 { Practica, Tanda };
UENUM(BlueprintType) enum class EPPShotMode : uint8 { Penal, Libre };
UENUM(BlueprintType) enum class EPPDifficulty : uint8 { Infantiles, Primera, Mundial };
UENUM(BlueprintType) enum class EPPShotKind : uint8 { Golpe, Arrastre, Picada, Fuetazo, Reves };
UENUM(BlueprintType) enum class EPPGKStyle : uint8 { Variado, Atrevido, Linea, Temprano };
UENUM(BlueprintType) enum class EPPOutcome : uint8 { None, Goal, Save, Post, Miss };
UENUM(BlueprintType) enum class EPPState : uint8 { Menu, Aim, Charge, Swing, Flight, Loose, Rival, End };
UENUM(BlueprintType) enum class EPPCamera : uint8 { Jugador, TV, Primera };

/** Parámetros del arquero según el nivel (DIFF del juego web). */
USTRUCT(BlueprintType)
struct FPPDiff
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float React = 0.19f;   // tiempo hasta moverse
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Speed = 2.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Noise = 0.22f;   // error al leer el tiro
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Guess = 0.3f;    // prob. de adivinar un lado
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Bite = 1.0f;     // cuánto se come los amagues
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Ramp = 0.2f;     // tiempo en estirar los brazos
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Recover = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Anticip = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RivalGoalP = 0.66f; // prob. de gol del rival en la tanda

	static FPPDiff Make(EPPDifficulty D)
	{
		FPPDiff R;
		switch (D)
		{
		case EPPDifficulty::Infantiles: R.React=.27f; R.Speed=1.45f; R.Noise=.34f; R.Guess=.42f; R.Bite=1.7f; R.Ramp=.26f; R.Recover=.95f; R.Anticip=.3f; R.RivalGoalP=.55f; break;
		case EPPDifficulty::Mundial:    R.React=.12f; R.Speed=2.8f;  R.Noise=.11f; R.Guess=.16f; R.Bite=.45f; R.Ramp=.15f; R.Recover=.42f; R.Anticip=1.f;  R.RivalGoalP=.76f; break;
		default: break; // Primera
		}
		return R;
	}
};

/** Estilo del arquero (GK_STYLES del juego web). */
USTRUCT(BlueprintType)
struct FPPGKStyle
{
	GENERATED_BODY()
	float Out = 2.6f, Poke = 2.2f, Guess = 0.f, React = 1.f, Noise = 1.05f, Speed = 1.1f;
	bool bEarly = false;

	static FPPGKStyle Make(EPPGKStyle S)
	{
		FPPGKStyle R;
		switch (S)
		{
		case EPPGKStyle::Linea:    R.Out=0.f;  R.Poke=.25f; R.Guess=-.12f; R.React=.9f;  R.Noise=.85f; R.Speed=1.15f; R.bEarly=false; break;
		case EPPGKStyle::Temprano: R.Out=.6f;  R.Poke=.6f;  R.Guess=.3f;    R.React=.55f; R.Noise=1.2f; R.Speed=1.f;   R.bEarly=true;  break;
		default: break; // Atrevido
		}
		return R;
	}
};

/** Un tiro en vuelo: lo que el arquero "lee" y cómo termina. */
struct FPPShot
{
	EPPShotKind Kind = EPPShotKind::Golpe;
	float CurveAccel = 0.f;
	float T = 0.f;
	float Tx = 0.f, Ty = 0.f;         // punto al que apuntó (plano del arco)
	float Gx = 0.f, Gy = 0.f;         // por dónde cruzó la línea
	int32 Kmh = 0;
	float PredX = 0.f, PredY = 0.f, React = 0.2f;
	float GTarget = 0.f; int32 ReachSide = 0;
	float Ext = 0.f;
	float Ramp = 0.2f, ReachZ = 0.3f, Tilt = 1.f;
	FName Tech = TEXT("mano");        // mano | estirada | cuerpo | palo | mariposa | split | stack
	EPPOutcome Outcome = EPPOutcome::None;
	bool bGkChecked = false, bLineChecked = false, bInGoal = false, bNetHit = false, bCaught = false;
	float HoldT = 0.f, NoCatch = 0.f, NoPick = 0.f, PrevZ = 0.f;
	FString Detail;
};
