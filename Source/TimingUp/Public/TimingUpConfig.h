#pragma once

#include "CoreMinimal.h"
#include "Configuration/ModConfiguration.h"
#include "TimingUpConfig.generated.h"

/**
 * In-game settings for TimingUp (Hauptmenü -> Mods -> TimingUp -> Konfiguration).
 * The properties are created at runtime (see FTimingUpConfig::Register) so the mod
 * needs no Blueprint assets. Values are saved by SML in Configs/TimingUp.cfg.
 */
UCLASS()
class TIMINGUP_API UTimingUpModConfiguration : public UModConfiguration
{
	GENERATED_BODY()
public:
	UTimingUpModConfiguration();
};

/** Typed access to the TimingUp settings */
struct TIMINGUP_API FTimingUpConfig
{
	// Keys
	static const FString KeyOverlayShow;
	static const FString KeyOverlayX;
	static const FString KeyOverlayY;
	static const FString KeyOverlayFontSize;
	static const FString KeyMaxUpgrades;
	static const FString KeyFirstWaitMinutes;
	static const FString KeyWaitFactor;
	static const FString KeySkipTier0;
	static const FString KeyBlockAlternates;
	static const FString KeyTimeScale;

	/** Registers the configuration with SML's config manager (safe to call multiple times) */
	static void Register(UGameInstance* GameInstance);

	static bool GetBool(const UObject* WorldContext, const FString& Key, bool Default);
	static float GetFloat(const UObject* WorldContext, const FString& Key, float Default);
	static int32 GetInt(const UObject* WorldContext, const FString& Key, int32 Default);

	// Convenience getters with defaults
	static bool  ShowOverlay(const UObject* C)      { return GetBool(C, KeyOverlayShow, true); }
	static float OverlayX(const UObject* C)         { return FMath::Clamp(GetFloat(C, KeyOverlayX, 50.f), 0.f, 100.f) / 100.f; }
	static float OverlayY(const UObject* C)         { return FMath::Clamp(GetFloat(C, KeyOverlayY, 2.f), 0.f, 100.f) / 100.f; }
	static int32 OverlayFontSize(const UObject* C)  { return FMath::Clamp(GetInt(C, KeyOverlayFontSize, 18), 8, 72); }
	static int32 MaxUpgrades(const UObject* C)      { return FMath::Max(1, GetInt(C, KeyMaxUpgrades, 30)); }
	static float FirstWaitSeconds(const UObject* C) { return FMath::Max(1.f, GetFloat(C, KeyFirstWaitMinutes, 1.f) * 60.f); }
	static float WaitFactor(const UObject* C)       { return FMath::Max(1.f, GetFloat(C, KeyWaitFactor, 1.2f)); }
	static bool  SkipTier0(const UObject* C)        { return GetBool(C, KeySkipTier0, true); }
	static bool  BlockAlternates(const UObject* C)  { return GetBool(C, KeyBlockAlternates, true); }
	static float TimeScale(const UObject* C)        { return FMath::Clamp(GetFloat(C, KeyTimeScale, 1.f), 1.f, 1000.f); }
};
