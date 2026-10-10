#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PPHUD.generated.h"

class APPGameMode;

/**
 * Marcador, mira, medidor de fuerza, menú y pantalla final dibujados con el Canvas (sin UMG, para no depender de assets).
 * Todo se diseña en una pantalla virtual de 1920 × 1080 y se escala según la resolución real.
 */
UCLASS()
class PENALPATINES_API APPHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;

private:
	float Ui = 1.f, Ox = 0.f;
	float X(float V) const { return Ox + V * Ui; }
	float Y(float V) const { return V * Ui; }
	void Text(const FString& S, float X0, float Y0, const FLinearColor& C, float Scale = 1.f, bool bCenter = false);
	void Rect(const FLinearColor& C, float X0, float Y0, float W, float H);
	void Line2(float X1, float Y1, float X2, float Y2, const FLinearColor& C, float Thick = 2.f);
	void Circle(float Cx, float Cy, float R, const FLinearColor& Col, float Thick = 2.f, int32 Seg = 28);
	void Panel(float X0, float Y0, float W, float H);
	void DrawMenu(APPGameMode* M);
	void DrawScore(APPGameMode* M);
};
