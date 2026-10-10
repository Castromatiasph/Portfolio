#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PPHUD.generated.h"

/** Marcador, mira, medidor de fuerza, menú y pantalla final dibujados con el Canvas (sin UMG, para no depender de assets). */
UCLASS()
class PENALPATINES_API APPHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;

private:
	void Text(const FString& S, float X, float Y, const FLinearColor& C, float Scale = 1.f, bool bCenter = false);
	void Circle(const FVector2D& C, float R, const FLinearColor& Col, float Thick = 2.f, int32 Seg = 28);
	void Panel(float X, float Y, float W, float H);
	void DrawMenu(class APPGameMode* M);
};
