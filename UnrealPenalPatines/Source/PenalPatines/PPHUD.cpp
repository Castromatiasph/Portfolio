#include "PPHUD.h"
#include "PPGameMode.h"
#include "PPShooter.h"
#include "PPGoalkeeper.h"
#include "PPBall.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

namespace
{
	const FLinearColor Amber(1.f, .71f, .15f), Green(.24f, .86f, .52f), Red(1.f, .35f, .30f), Ink(.93f, .95f, .97f), Muted(.62f, .66f, .75f), PanelC(.04f, .06f, .10f, .88f);
}

void APPHUD::Text(const FString& S, float X0, float Y0, const FLinearColor& C, float Scale, bool bCenter)
{
	UFont* F = GEngine->GetLargeFont();
	const float Sc = Scale * Ui * 1.3f;
	float W = 0, H = 0;
	float Px = X(X0);
	if (bCenter) { GetTextSize(S, W, H, F, Sc); Px -= W * .5f; }
	DrawText(S, C, Px, Y(Y0), F, Sc);
}

void APPHUD::Rect(const FLinearColor& C, float X0, float Y0, float W, float H) { DrawRect(C, X(X0), Y(Y0), W * Ui, H * Ui); }
void APPHUD::Line2(float X1, float Y1, float X2, float Y2, const FLinearColor& C, float Thick) { DrawLine(X(X1), Y(Y1), X(X2), Y(Y2), C, Thick * Ui); }
void APPHUD::Panel(float X0, float Y0, float W, float H) { Rect(PanelC, X0, Y0, W, H); }

void APPHUD::Circle(float Cx, float Cy, float R, const FLinearColor& Col, float Thick, int32 Seg)
{
	for (int32 i = 0; i < Seg; i++)
	{
		const float A0 = 2.f * PI * i / Seg, A1 = 2.f * PI * (i + 1) / Seg;
		Line2(Cx + R * FMath::Cos(A0), Cy + R * FMath::Sin(A0), Cx + R * FMath::Cos(A1), Cy + R * FMath::Sin(A1), Col, Thick);
	}
}

void APPHUD::DrawMenu(APPGameMode* M)
{
	Rect(FLinearColor(.02f, .03f, .06f, .78f), -2000, 0, 6000, 1080);
	Text(TEXT("HOCKEY SOBRE PATINES"), 960, 70, Amber, 1.2f, true);
	Text(TEXT("PENAL"), 960, 120, Ink, 4.f, true);
	auto Opt = [&](const FString& Label, bool bOn, float Yy) { Text(FString::Printf(TEXT("%s %s"), bOn ? TEXT(">") : TEXT("  "), *Label), 960, Yy, bOn ? Amber : Muted, 1.f, true); };
	float Yy = 290.f;
	Text(TEXT("MODO"), 960, Yy, Muted, .8f, true); Yy += 38;
	Opt(TEXT("[1] Práctica - rebotes libres"), M->Format == EPPFormat::Practica, Yy); Yy += 36;
	Opt(TEXT("[2] Tanda contra un rival - 5 y 5, muerte súbita"), M->Format == EPPFormat::Tanda, Yy); Yy += 36;
	Opt(TEXT("[3] Atajá vos - el rival patea, vos sos el arquero"), M->Format == EPPFormat::Arquero, Yy); Yy += 36;
	Opt(TEXT("[4] Desafíos - 7 objetivos, ganá estrellas"), M->Format == EPPFormat::Desafio, Yy); Yy += 36;
	Opt(TEXT("[5] 2 jugadores - patean de a uno, alternados"), M->Format == EPPFormat::Duelo, Yy); Yy += 54;
	Text(FString::Printf(TEXT("[6] Tiro: %s"), M->ShotMode == EPPShotMode::Penal ? TEXT("Penal (5,40 m)") : TEXT("Libre directo (7,40 m)")), 960, Yy, Ink, 1.f, true); Yy += 40;
	static const TCHAR* Lv[3] = { TEXT("Infantiles"), TEXT("Primera"), TEXT("Mundial") };
	Text(FString::Printf(TEXT("[7] Nivel del arquero: %s"), Lv[int32(M->Difficulty)]), 960, Yy, Ink, 1.f, true); Yy += 40;
	static const TCHAR* St[4] = { TEXT("Variado"), TEXT("Atrevido"), TEXT("De línea"), TEXT("Se tira temprano") };
	Text(FString::Printf(TEXT("[8] Estilo del arquero: %s"), St[int32(M->GoalieStyle)]), 960, Yy, Ink, 1.f, true); Yy += 40;
	static const TCHAR* Ql[3] = { TEXT("Baja (más fluido)"), TEXT("Media"), TEXT("Alta (más lindo)") };
	Text(FString::Printf(TEXT("[9] Gráficos: %s"), Ql[int32(M->Quality)]), 960, Yy, M->Quality == EPPQuality::Alta ? Amber : Ink, 1.f, true); Yy += 70;
	Text(TEXT("ENTER para jugar"), 960, Yy, Green, 1.5f, true);
}

void APPHUD::DrawScore(APPGameMode* M)
{
	Panel(660, 14, 600, 92);
	switch (M->Format)
	{
	case EPPFormat::Tanda:
	{
		Text(FString::FromInt(M->GoalsOf(M->Results)), 700, 30, Amber, 2.2f);
		Text(FString::FromInt(M->GoalsOfRival()), 1190, 30, Amber, 2.2f);
		Text(FString::Printf(TEXT("%s %d"), M->ShotMode == EPPShotMode::Penal ? TEXT("PENAL") : TEXT("LIBRE"), M->Results.Num() + (M->State == EPPState::Aim || M->State == EPPState::Charge || M->State == EPPState::Swing ? 1 : 0)), 960, 24, Muted, .8f, true);
		const int32 N = FMath::Max(5, FMath::Max(M->Results.Num(), M->Rival.Num()));
		for (int32 i = 0; i < N; i++)
		{
			const float Xs = 960.f - (N * 26) * .5f + i * 26 + 13;
			FLinearColor C = Muted; if (M->Results.IsValidIndex(i)) C = M->Results[i] == EPPOutcome::Goal ? Green : Red;
			Rect(C, Xs - 9, 62, 18, 9);
			FLinearColor C2 = Muted; if (M->Rival.IsValidIndex(i)) C2 = M->Rival[i] ? Green : Red;
			Rect(C2, Xs - 9, 76, 18, 9);
		}
		break;
	}
	case EPPFormat::Duelo:
	{
		Text(TEXT("JUGADOR 1"), 690, 20, Muted, .6f); Text(FString::FromInt(M->GoalsOf(M->DuelA)), 700, 44, Amber, 2.f);
		Text(TEXT("JUGADOR 2"), 1090, 20, Muted, .6f); Text(FString::FromInt(M->GoalsOf(M->DuelB)), 1180, 44, Amber, 2.f);
		Text(FString::Printf(TEXT("PATEA %s"), M->bDuelA ? TEXT("J1") : TEXT("J2")), 960, 36, Muted, .8f, true);
		const int32 N = FMath::Max(5, FMath::Max(M->DuelA.Num(), M->DuelB.Num()));
		for (int32 i = 0; i < N; i++)
		{
			const float Xs = 960.f - (N * 26) * .5f + i * 26 + 13;
			FLinearColor C = Muted; if (M->DuelA.IsValidIndex(i)) C = M->DuelA[i] == EPPOutcome::Goal ? Green : Red;
			Rect(C, Xs - 9, 66, 18, 9);
			FLinearColor C2 = Muted; if (M->DuelB.IsValidIndex(i)) C2 = M->DuelB[i] == EPPOutcome::Goal ? Green : Red;
			Rect(C2, Xs - 9, 80, 18, 9);
		}
		break;
	}
	case EPPFormat::Desafio:
	{
		const int32 Ci = FMath::Min(M->ChI, 6);
		Text(FString::Printf(TEXT("DESAFÍO %d de 7"), Ci + 1), 960, 22, Muted, .8f, true);
		Text(APPGameMode::Challenges[Ci].Name, 960, 48, Amber, 1.4f, true);
		Text(FString::Printf(TEXT("Intentos: %d de %d"), M->ChTries, APPGameMode::Challenges[Ci].Tries), 960, 80, Ink, .7f, true);
		break;
	}
	case EPPFormat::Arquero:
	{
		int32 Sv = 0; for (EPPOutcome O : M->Results) if (O == EPPOutcome::Save) Sv++;
		Text(TEXT("GOLES RIVAL"), 690, 20, Muted, .6f); Text(FString::FromInt(M->GoalsOf(M->Results)), 700, 44, Red, 2.f);
		Text(TEXT("ATAJADAS"), 1130, 20, Muted, .6f); Text(FString::FromInt(Sv), 1150, 44, Green, 2.f);
		Text(FString::Printf(TEXT("TIRO %d de 5"), FMath::Min(M->Results.Num() + 1, 5)), 960, 40, Muted, .8f, true);
		break;
	}
	default:
	{
		int32 Sv = 0; for (EPPOutcome O : M->Results) if (O == EPPOutcome::Save) Sv++;
		Text(TEXT("GOLES"), 690, 20, Muted, .6f); Text(FString::FromInt(M->GoalsOf(M->Results)), 700, 44, Amber, 2.f);
		Text(TEXT("ATAJADAS"), 1130, 20, Muted, .6f); Text(FString::FromInt(Sv), 1150, 44, Amber, 2.f);
		Text(M->ShotMode == EPPShotMode::Penal ? TEXT("PRÁCTICA · PENAL") : TEXT("PRÁCTICA · LIBRE"), 960, 40, Muted, .8f, true);
		break;
	}
	}
}

void APPHUD::DrawHUD()
{
	Super::DrawHUD();
	APPGameMode* M = GetWorld() ? GetWorld()->GetAuthGameMode<APPGameMode>() : nullptr;
	if (!M || !Canvas) return;
	// pantalla virtual de 1920 × 1080 centrada
	Ui = Canvas->ClipY / 1080.f;
	Ox = (Canvas->ClipX - 1920.f * Ui) * .5f;

	if (M->State == EPPState::Menu) { DrawMenu(M); return; }
	if (M->State == EPPState::Replay)
	{
		Panel(40, 40, 300, 56);
		Text(TEXT("REPETICIÓN"), 60, 52, Red, 1.1f);
		Text(TEXT("clic o Espacio para saltear"), 60, 78, Muted, .55f);
		return;
	}
	DrawScore(M);

	APPShooter* S = M->Shooter;
	const bool bUserShoots = M->UserShoots();
	// ---- mira sobre el plano del arco ----
	if (S && bUserShoots && (M->State == EPPState::Aim || M->State == EPPState::Charge))
	{
		const FVector P = Project(PP::ToUE(FPPVec(S->Aim.X, S->Aim.Y, .03f)));
		const bool bInside = FMath::Abs(S->Aim.X) < PP::PostHalf && S->Aim.Y < PP::BarH;
		const FLinearColor C = bInside ? Amber : Red;
		const float R = (11.f + FMath::Sin(M->Time * 6.f) * 1.5f);
		const float Px = P.X, Py = P.Y;
		auto Cr = [&](float Cx, float Cy, float Rr, const FLinearColor& Col, float Th, int32 Sg)
		{
			for (int32 i = 0; i < Sg; i++)
			{
				const float A0 = 2.f * PI * i / Sg, A1 = 2.f * PI * (i + 1) / Sg;
				DrawLine(Cx + Rr * FMath::Cos(A0), Cy + Rr * FMath::Sin(A0), Cx + Rr * FMath::Cos(A1), Cy + Rr * FMath::Sin(A1), Col, Th);
			}
		};
		const float Sc = Ui;
		Cr(Px, Py, R * Sc, C, 2.f * Sc, 28);
		DrawLine(Px - (R + 6) * Sc, Py, Px - (R - 4) * Sc, Py, C, 2.f * Sc); DrawLine(Px + (R - 4) * Sc, Py, Px + (R + 6) * Sc, Py, C, 2.f * Sc);
		DrawLine(Px, Py - (R + 6) * Sc, Px, Py - (R - 4) * Sc, C, 2.f * Sc); DrawLine(Px, Py + (R - 4) * Sc, Px, Py + (R + 6) * Sc, C, 2.f * Sc);
		if (M->State == EPPState::Charge && S->Power > .04f)
		{
			// anillo de dispersión: hasta dónde se puede abrir el tiro con la fuerza actual
			const float Err = M->EstimatedError(S->Power) * (S->bLobHeld ? 2.6f : 2.f);
			const FVector Q = Project(PP::ToUE(FPPVec(S->Aim.X + Err, S->Aim.Y, .03f)));
			Cr(Px, Py, FMath::Max(8.f * Sc, FMath::Abs(Q.X - Px)), S->Power > .82f ? Red : S->Power > .55f ? Green : Ink, 1.5f * Sc, 36);
		}
	}
	// ---- manos del arquero (modos con arquero humano) ----
	if (M->Gk && M->Gk->bUser && M->State != EPPState::Menu && M->State != EPPState::End)
	{
		const FVector P = Project(PP::ToUE(FPPVec(M->Gk->In.X, M->Gk->In.Y, M->Gk->Pos.Y + .3f)));
		const float R = 14.f * Ui;
		const FLinearColor C = M->Gk->In.bDown ? Green : Ink;
		for (int32 i = 0; i < 24; i++)
		{
			const float A0 = 2.f * PI * i / 24, A1 = 2.f * PI * (i + 1) / 24;
			DrawLine(P.X + R * FMath::Cos(A0), P.Y + R * FMath::Sin(A0), P.X + R * FMath::Cos(A1), P.Y + R * FMath::Sin(A1), C, 2.f * Ui);
		}
	}

	// ---- medidor de fuerza ----
	if (S && bUserShoots && (M->State == EPPState::Aim || M->State == EPPState::Charge))
	{
		const float Bw = 560.f, X0 = 960.f - Bw * .5f, Y0 = 930.f;
		Panel(X0 - 14, Y0 - 14, Bw + 28, 118);
		const bool bCh = M->State == EPPState::Charge;
		Text(bCh ? TEXT("FUERZA") : TEXT("FUERZA · tirá el mouse hacia atrás"), X0, Y0 - 6, Muted, .7f);
		const float Pw = S->Power;
		Text(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Pw * 100)), X0 + Bw - 70, Y0 - 8, Amber, 1.1f);
		if (bCh) Text(FString::Printf(TEXT("~%d km/h"), FMath::RoundToInt((8.f + Pw * 18.f) * 3.6f)), X0 + Bw * .5f - 40, Y0 - 6, Ink, .8f);
		Rect(FLinearColor(1, 1, 1, .1f), X0, Y0 + 28, Bw, 22);
		Rect(FLinearColor(.24f, .86f, .52f, .3f), X0 + Bw * .55f, Y0 + 28, Bw * .27f, 22);   // zona ideal
		Rect(Pw > .82f ? Red : Amber, X0, Y0 + 28, Bw * Pw, 22);
		Text(FString::Printf(TEXT("Picada [Shift] %s    Efecto [Ctrl] %s"), S->bLobHeld ? TEXT("X") : TEXT("_"), S->bCurveHeld ? TEXT("X") : TEXT("_")), X0, Y0 + 60, Muted, .7f);
		Text(FString::Printf(TEXT("Precisión ±%.0f cm"), M->EstimatedError(FMath::Max(Pw, .3f)) * 100.f), X0 + Bw - 200, Y0 + 60, Pw > .82f ? Red : Muted, .7f);
	}

	// ---- cartel del resultado ----
	if (!M->FlashBig.IsEmpty())
	{
		const FLinearColor C = M->FlashClass == 1 ? Green : M->FlashClass == 2 ? Red : Ink;
		Text(M->FlashBig, 960, 220, C, 3.4f, true);
		Text(M->FlashSmall, 960, 305, Ink, 1.1f, true);
	}
	if (!M->Hint.IsEmpty() && M->State != EPPState::Charge) Text(M->Hint, 24, 1046, Muted, .7f);
	if (M->bPaused) { Rect(FLinearColor(0, 0, 0, .55f), -2000, 0, 6000, 1080); Text(TEXT("PAUSA — Esc para seguir"), 960, 480, Ink, 2.4f, true); }
}
