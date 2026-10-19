#pragma once

#include "CoreMinimal.h"
#include "Subsystem/ModSubsystem.h"
#include "FGSaveInterface.h"
#include "TimingUpSubsystem.generated.h"

/** Current state of the challenge, used by the HUD */
UENUM()
enum class ETimingUpState : uint8
{
	/** Countdown to the next upgrade is running */
	Waiting,
	/** An upgrade is available and waiting to be spent; the countdown is paused */
	UpgradeAvailable,
	/** All upgrades are used and the countdown to the theoretical upgrade Max+1 is running */
	FinalCountdown,
	/** The theoretical time of upgrade 31 was reached: the challenge is over */
	Finished
};

/**
 * TimingUp! challenge logic.
 *
 * Rules:
 *  - An "upgrade" is: a HUB milestone (incl. tier 0), a MAM research, a space elevator phase.
 *  - At most N upgrades (setting, default 40).
 *  - Only game time counts (actors don't tick while the game is paused or loading).
 *  - The first upgrade becomes available after the first wait time (setting, default 5 min).
 *    Every following wait is "wait factor" (setting, default 1.12) times longer;
 *    the factor can shrink per upgrade and each wait can be capped (settings).
 *  - The countdown to the next upgrade only starts once the currently available upgrade was spent.
 *  - The challenge ends when the countdown of the theoretical upgrade Max+1 runs out.
 *  - Alternate recipes are forbidden (hard drive research and alternate schematics are blocked).
 */
UCLASS()
class TIMINGUP_API ATimingUpSubsystem : public AModSubsystem, public IFGSaveInterface
{
	GENERATED_BODY()
public:
	ATimingUpSubsystem();

	/** Speed-up when the game is started with the launch option -TimingUpTest */
	static constexpr float TestModeTimeScale = 60.0f;

	/** 1.0 normally; faster with -TimingUpTest or the test-mode setting */
	float GetTimeScale() const;

	/** Max upgrades from the mod settings (default 40) */
	int32 GetMaxUpgrades() const;

	/** Returns the subsystem for the given world, or nullptr */
	static ATimingUpSubsystem* Get(const UObject* WorldContext);

	// AActor
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IFGSaveInterface
	virtual bool ShouldSave_Implementation() const override { return true; }

	/** True if an upgrade may be spent right now */
	bool CanSpendUpgrade() const { return State == ETimingUpState::UpgradeAvailable; }

	/** Called when an upgrade was actually granted (milestone/MAM unlocked, elevator phase completed) */
	void OnUpgradeSpent(const FString& What);

	/** Text for the on-screen overlay */
	FText GetOverlayText() const;
	FSlateColor GetOverlayColor() const;

	/** Wait time (seconds) before upgrade number N (1-based), from the mod settings */
	float GetWaitForUpgrade(int32 UpgradeNumber) const;

	/** Number of upgrades already spent */
	UPROPERTY(SaveGame, Replicated)
	int32 UpgradesUsed = 0;

	/** Upgrade number the current countdown belongs to (1..31) */
	UPROPERTY(SaveGame, Replicated)
	int32 CountdownFor = 1;

	/** Remaining game time of the current countdown in seconds */
	UPROPERTY(SaveGame, Replicated)
	float RemainingSeconds = 60.0f;

	/** Total game time since the challenge started */
	UPROPERTY(SaveGame, Replicated)
	float ElapsedSeconds = 0.0f;

	UPROPERTY(SaveGame, Replicated)
	ETimingUpState State = ETimingUpState::Waiting;

	/** Upgrades the game granted while none was available (should not happen; spent as soon as one becomes free) */
	UPROPERTY(SaveGame)
	int32 Debt = 0;

private:
	void ShowMessage(const FString& Message, float Seconds = 6.0f);
	void AddOverlay();
	void RemoveOverlay();

	TSharedPtr<class SWidget> OverlayWidget;
	FString FlashMessage;
	float FlashRemaining = 0.0f;
};
