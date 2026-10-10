#include "PPHUD.h"
#include "PPGameMode.h"
#include "PPShooter.h"
#include "PPBall.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"

namespace
{
	const FLinearColor Amber(1.f, .71f, .15f), Green(.24f, .86f, .52f), Red(1.f, .35f, .30f), Ink(.93f, .95f, .97f), Muted(.6f, .64f, .73f), PanelC(.04f, .06f, .10f, .85f);
}

void APPHUD::Text(const FString& S, float X, float Y, const FLinearColor& C, float Scale, bool bCenter)
{
	UFont* F = GEngine->GetLargeFont();
	float W = 0, H = 0;
	if (bCenter) { GetTextSize(S, W, H, F, Scale); X -= W * .5f; }
	DrawText(S, C, X, Y, F, Scale);
}

void APPHUD::Circle(const FVector2D& C, float R, const FLinearColor& Col, float Thick, int32 Seg)
{
	for (int32 i = 0; i < Seg; i++)
	{
		const float A0 = 2.f * PI * i / Seg, A1 = 2.f * PI * (i + 1) / Seg;
		DrawLine(C.X + R * FMath::Cos(A0), C.Y + R * FMath::Sin(A0), C.X + R * FMath::Cos(A1), C.Y + R * FMath::Sin(A1), Col, Thick);
	}
}

void APPHUD::Panel(float X, float Y, float W, float H) { DrawRect(PanelC, X, Y, W, H); }

void APPHUD::DrawMenu(APPGameMode* M)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	DrawRect(FLinearColor(.02f, .03f, .06f, .72f), 0, 0, W, H);
	const float Cx = W * .5f;
	Text(TEXT("HOCKEY SOBRE PATINES"), Cx, H * .12f, Amber, 1.f, true);
	Text(TEXT("PENAL"), Cx, H * .17f, Ink, 3.f, true);
	auto Opt = [&](const TCHAR* Label, bool bOn, float Y) { Text(FString::Printf(TEXT("%s %s"), bOn ? TEXT(">") : TEXT("  "), Label), Cx, Y, bOn ? Amber : Muted, 1.f, true); };
	float Y = H * .38f;
	Text(TEXT("Modo"), Cx, Y, Muted, .8f, true); Y += 28;
	Opt(TEXT("[1] Práctica — rebotes libres"), M->Format == EPPFormat::Practica, Y); Y += 28;
	Opt(TEXT("[2] Tanda contra un rival — 5 y 5, muerte súbita"), M->Format == EPPFormat::Tanda, Y); Y += 40;
	Text(TEXT("Tiro"), Cx, Y, Muted, .8f, true); Y += 28;
	Opt(TEXT("[3] Penal — 5,40 m"), M->ShotMode == EPPShotMode::Penal, Y); Y += 28;
	Opt(TEXT("[4] Libre directo — 7,40 m"), M->ShotMode == EPPShotMode::Libre, Y); Y += 40;
	Text(TEXT("Nivel del arquero"), Cx, Y, Muted, .8f, true); Y += 28;
	Opt(TEXT("[5] Infantiles"), M->Difficulty == EPPDifficulty::Infantiles, Y); Y += 28;
	Opt(TEXT("[6] Primera"), M->Difficulty == EPPDifficulty::Primera, Y); Y += 28;
	Opt(TEXT("[7] Mundial"), M->Difficulty == EPPDifficulty::Mundial, Y); Y += 40;
	static const TCHAR* Styles[4] = { TEXT("Variado"), TEXT("Atrevido"), TEXT("De línea"), TEXT("Se tira temprano") };
	Text(FString::Printf(TEXT("[8] Estilo del arquero: %s"), Styles[int32(M->GoalieStyle)]), Cx, Y, Ink, 1.f, true); Y += 50;
	Text(TEXT("ENTER para jugar"), Cx, Y, Green, 1.3f, true);
}

void APPHUD::DrawHUD()
{
	Super::DrawHUD();
	APPGameMode* M = GetWorld() ? GetWorld()->GetAuthGameMode<APPGameMode>() : nullptr;
	if (!M || !Canvas) return;
	const float W = Canvas->ClipX, H = Canvas->ClipY, Cx = W * .5f;

	if (M->State == EPPState::Menu) { DrawMenu(M); return; }

	// ---- marcador ----
	{
		Panel(Cx - 220, 10, 440, 70);
		if (M->Format == EPPFormat::Tanda)
		{
			Text(FString::FromInt(M->GoalsOf(M->Results)), Cx - 170, 22, Amber, 1.6f);
			Text(FString::FromInt(M->GoalsOfRival()), Cx + 150, 22, Amber, 1.6f);
			Text(FString::Printf(TEXT("%s %d"), M->ShotMode == EPPShotMode::Penal ? TEXT("Penal") : TEXT("Libre"), FMath::Min(M->Results.Num() + (M->State == EPPState::Aim || M->State == EPPState::Charge || M->State == EPPState::Swing ? 1 : 0), 99)), Cx, 18, Muted, .8f, true);
			const int32 N = FMath::Max(5, FMath::Max(M->Results.Num(), M->Rival.Num()));
			for (int32 i = 0; i < N; i++)
			{
				const float X = Cx - (N * 18) * .5f + i * 18 + 9;
				FLinearColor C = Muted; if (M->Results.IsValidIndex(i)) C = M->Results[i] == EPPOutcome::Goal ? Green : Red;
				DrawRect(C, X - 6, 48, 12, 6);
				FLinearColor C2 = Muted; if (M->Rival.IsValidIndex(i)) C2 = M->Rival[i] ? Green : Red;
				DrawRect(C2, X - 6, 58, 12, 6);
			}
		}
		else
		{
			int32 Sv = 0; for (EPPOutcome O : M->Results) if (O == EPPOutcome::Save) Sv++;
			Text(TEXT("GOLES"), Cx - 190, 14, Muted, .6f); Text(FString::FromInt(M->GoalsOf(M->Results)), Cx - 180, 34, Amber, 1.6f);
			Text(TEXT("ATAJADAS"), Cx + 110, 14, Muted, .6f); Text(FString::FromInt(Sv), Cx + 150, 34, Amber, 1.6f);
			Text(M->ShotMode == EPPShotMode::Penal ? TEXT("PRÁCTICA · PENAL") : TEXT("PRÁCTICA · LIBRE"), Cx, 30, Muted, .8f, true);
		}
	}

	APPShooter* S = M->Shooter;
	// ---- mira sobre el plano del arco ----
	if (S && (M->State == EPPState::Aim || M->State == EPPState::Charge))
	{
		const FVector P = Project(PP::ToUE(FPPVec(S->Aim.X, S->Aim.Y, .03f)));
		const bool bInside = FMath::Abs(S->Aim.X) < PP::PostHalf && S->Aim.Y < PP::BarH;
		const FLinearColor C = bInside ? Amber : Red;
		const float R = 11.f + FMath::Sin(M->Time * 6.f) * 1.5f;
		Circle(FVector2D(P.X, P.Y), R, C, 2.f);
		DrawLine(P.X - R - 6, P.Y, P.X - R + 4, P.Y, C, 2.f); DrawLine(P.X + R - 4, P.Y, P.X + R + 6, P.Y, C, 2.f);
		DrawLine(P.X, P.Y - R - 6, P.X, P.Y - R + 4, C, 2.f); DrawLine(P.X, P.Y + R - 4, P.X, P.Y + R + 6, C, 2.f);
		if (M->State == EPPState::Charge && S->Power > .04f)
		{
			// anillo de dispersión: hasta dónde se puede abrir el tiro con la fuerza actual
			const float Err = M->EstimatedError(S->Power) * (S->bLobHeld ? 2.6f : 2.f);
			const FVector Q = Project(PP::ToUE(FPPVec(S->Aim.X + Err, S->Aim.Y, .03f)));
			Circle(FVector2D(P.X, P.Y), FMath::Max(8.f, FMath::Abs(Q.X - P.X)), S->Power > .82f ? Red : S->Power > .55f ? Green : Ink, 1.5f, 36);
		}
	}

	// ---- medidor de fuerza ----
	if (S && (M->State == EPPState::Aim || M->State == EPPState::Charge))
	{
		const float Bw = FMath::Min(440.f, W - 32.f), X0 = Cx - Bw * .5f, Y0 = H - 100.f;
		Panel(X0 - 10, Y0 - 10, Bw + 20, 84);
		const bool bCh = M->State == EPPState::Charge;
		Text(bCh ? TEXT("FUERZA") : TEXT("FUERZA · tirá el mouse para atrás"), X0, Y0 - 6, Muted, .6f);
		const float Pw = S->Power;
		Text(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Pw * 100)), X0 + Bw - 50, Y0 - 8, Amber, .9f);
		if (bCh) Text(FString::Printf(TEXT("~%d km/h"), FMath::RoundToInt((8.f + Pw * 18.f) * 3.6f)), X0 + Bw * .5f - 30, Y0 - 6, Ink, .7f);
		DrawRect(FLinearColor(1, 1, 1, .08f), X0, Y0 + 20, Bw, 16);
		DrawRect(FLinearColor(.24f, .86f, .52f, .28f), X0 + Bw * .55f, Y0 + 20, Bw * .27f, 16);   // zona ideal
		DrawRect(Pw > .82f ? Red : Amber, X0, Y0 + 20, Bw * Pw, 16);
		Text(FString::Printf(TEXT("Picada [Shift] %s   Efecto [Ctrl] %s"), S->bLobHeld ? TEXT("●") : TEXT("○"), S->bCurveHeld ? TEXT("●") : TEXT("○")), X0, Y0 + 42, Muted, .6f);
		Text(FString::Printf(TEXT("Precisión ±%.0f cm"), M->EstimatedError(FMath::Max(Pw, .3f)) * 100.f), X0 + Bw - 150, Y0 + 42, Pw > .82f ? Red : Muted, .6f);
	}

	// ---- cartel del resultado ----
	if (!M->FlashBig.IsEmpty())
	{
		const FLinearColor C = M->FlashClass == 1 ? Green : M->FlashClass == 2 ? Red : Ink;
		Text(M->FlashBig, Cx, H * .22f, C, 3.f, true);
		Text(M->FlashSmall, Cx, H * .22f + 64, Ink, .9f, true);
	}
	if (!M->Hint.IsEmpty() && M->State != EPPState::Charge) Text(M->Hint, 16, H - 26, Muted, .55f);
	if (M->bPaused) { DrawRect(FLinearColor(0, 0, 0, .5f), 0, 0, W, H); Text(TEXT("PAUSA — Esc para seguir"), Cx, H * .45f, Ink, 2.f, true); }
}
