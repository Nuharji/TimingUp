#include "TimingUpSubsystem.h"

#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Subsystem/SubsystemActorManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "TimingUpConfig.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "TimingUp"

ATimingUpSubsystem::ATimingUpSubsystem()
{
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer_Replicate;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	// Paused game = no tick = timer stops. That is exactly "only game time counts".
	PrimaryActorTick.bTickEvenWhenPaused = false;
}

ATimingUpSubsystem* ATimingUpSubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;
	UWorld* World = WorldContext->GetWorld();
	if (!World) return nullptr;
	USubsystemActorManager* Manager = World->GetSubsystem<USubsystemActorManager>();
	return Manager ? Manager->GetSubsystemActor<ATimingUpSubsystem>() : nullptr;
}

float ATimingUpSubsystem::GetTimeScale() const
{
	// Launch option -TimingUpTest or the setting "Testmodus: Zeit-Beschleunigung"
	static const bool bTestMode = FParse::Param(FCommandLine::Get(), TEXT("TimingUpTest"));
	return FMath::Max(bTestMode ? TestModeTimeScale : 1.0f, FTimingUpConfig::TimeScale(this));
}

int32 ATimingUpSubsystem::GetMaxUpgrades() const
{
	return FTimingUpConfig::MaxUpgrades(this);
}

float ATimingUpSubsystem::GetWaitForUpgrade(int32 UpgradeNumber) const
{
	return FTimingUpConfig::FirstWaitSeconds(this) * FMath::Pow(FTimingUpConfig::WaitFactor(this), FMath::Max(0, UpgradeNumber - 1));
}

void ATimingUpSubsystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATimingUpSubsystem, UpgradesUsed);
	DOREPLIFETIME(ATimingUpSubsystem, CountdownFor);
	DOREPLIFETIME(ATimingUpSubsystem, RemainingSeconds);
	DOREPLIFETIME(ATimingUpSubsystem, ElapsedSeconds);
	DOREPLIFETIME(ATimingUpSubsystem, State);
}

void ATimingUpSubsystem::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && UpgradesUsed == 0 && CountdownFor == 1 && ElapsedSeconds == 0.0f)
	{
		// Fresh challenge: first countdown uses the configured wait time
		RemainingSeconds = GetWaitForUpgrade(1);
	}
	AddOverlay();
}

void ATimingUpSubsystem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveOverlay();
	Super::EndPlay(EndPlayReason);
}

void ATimingUpSubsystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (FlashRemaining > 0.0f)
	{
		FlashRemaining -= DeltaSeconds;
	}

	if (!HasAuthority()) return;

	// Test mode: start the game with launch option -TimingUpTest -> timer runs 60x faster
	DeltaSeconds *= GetTimeScale();

	if (State == ETimingUpState::Finished) return;
	ElapsedSeconds += DeltaSeconds;

	// The countdown is paused while an upgrade is waiting to be spent.
	if (State == ETimingUpState::UpgradeAvailable) return;

	RemainingSeconds -= DeltaSeconds;
	if (RemainingSeconds > 0.0f) return;
	RemainingSeconds = 0.0f;

	if (State == ETimingUpState::FinalCountdown)
	{
		State = ETimingUpState::Finished;
		ShowMessage(FString::Printf(TEXT("CHALLENGE VORBEI! Die Zeit von Upgrade %d ist erreicht."), GetMaxUpgrades() + 1), 30.0f);
		return;
	}

	State = ETimingUpState::UpgradeAvailable;
	ShowMessage(FString::Printf(TEXT("UPGRADE %d/%d FREI!"), CountdownFor, GetMaxUpgrades()));

	// Pay back an upgrade the game granted while none was available
	if (Debt > 0)
	{
		Debt--;
		OnUpgradeSpent(TEXT("Nachzahlung"));
	}
}

void ATimingUpSubsystem::OnUpgradeSpent(const FString& What)
{
	if (!HasAuthority()) return;

	if (State != ETimingUpState::UpgradeAvailable)
	{
		// Should be blocked by the hooks; remember it so the next upgrade pays for it
		Debt++;
		UE_LOG(LogTemp, Warning, TEXT("[TimingUp] Upgrade '%s' ohne freies Upgrade erhalten (Schuld: %d)"), *What, Debt);
		return;
	}

	UpgradesUsed++;
	CountdownFor = UpgradesUsed + 1;
	RemainingSeconds = GetWaitForUpgrade(CountdownFor);
	State = (UpgradesUsed >= GetMaxUpgrades()) ? ETimingUpState::FinalCountdown : ETimingUpState::Waiting;

	UE_LOG(LogTemp, Log, TEXT("[TimingUp] Upgrade %d/%d benutzt: %s"), UpgradesUsed, GetMaxUpgrades(), *What);
	ShowMessage(FString::Printf(TEXT("Upgrade %d/%d benutzt: %s"), UpgradesUsed, GetMaxUpgrades(), *What), 4.0f);
}

static FString FormatTime(float Seconds)
{
	const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
	return FString::Printf(TEXT("%d:%02d:%02d"), Total / 3600, (Total / 60) % 60, Total % 60);
}

FText ATimingUpSubsystem::GetOverlayText() const
{
	FString Line1 = FString::Printf(TEXT("TIMINGUP!   Upgrades %d/%d   Spielzeit %s"), UpgradesUsed, GetMaxUpgrades(), *FormatTime(ElapsedSeconds));
	if (GetTimeScale() != 1.0f)
	{
		Line1 += FString::Printf(TEXT("   [TESTMODUS x%.0f]"), GetTimeScale());
	}
	FString Line2;
	switch (State)
	{
	case ETimingUpState::Waiting:
		Line2 = FString::Printf(TEXT("Nächstes Upgrade (%d) in %s"), CountdownFor, *FormatTime(RemainingSeconds));
		break;
	case ETimingUpState::UpgradeAvailable:
		Line2 = FString::Printf(TEXT(">> UPGRADE %d FREI – Timer pausiert <<"), CountdownFor);
		break;
	case ETimingUpState::FinalCountdown:
		Line2 = FString::Printf(TEXT("Keine Upgrades mehr – Ende in %s"), *FormatTime(RemainingSeconds));
		break;
	case ETimingUpState::Finished:
		Line2 = TEXT("CHALLENGE VORBEI!");
		break;
	}
	if (FlashRemaining > 0.0f && !FlashMessage.IsEmpty())
	{
		Line2 += TEXT("\n") + FlashMessage;
	}
	return FText::FromString(Line1 + TEXT("\n") + Line2);
}

FSlateColor ATimingUpSubsystem::GetOverlayColor() const
{
	switch (State)
	{
	case ETimingUpState::UpgradeAvailable: return FSlateColor(FLinearColor(0.25f, 1.0f, 0.35f));
	case ETimingUpState::Finished:         return FSlateColor(FLinearColor(1.0f, 0.25f, 0.25f));
	default:                               return FSlateColor(FLinearColor(1.0f, 0.59f, 0.22f)); // Novy-Orange
	}
}

void ATimingUpSubsystem::ShowMessage(const FString& Message, float Seconds)
{
	FlashMessage = Message;
	FlashRemaining = Seconds;
	UE_LOG(LogTemp, Log, TEXT("[TimingUp] %s"), *Message);
}

void ATimingUpSubsystem::AddOverlay()
{
	if (OverlayWidget.IsValid() || !GEngine || !GEngine->GameViewport) return;
	if (GetNetMode() == NM_DedicatedServer) return;

	TWeakObjectPtr<ATimingUpSubsystem> WeakThis(this);

	// Position (0..1 of the screen) and font size come from the mod settings and update live
	auto Pos = [WeakThis]() -> FVector2D
	{
		return WeakThis.IsValid()
			? FVector2D(FTimingUpConfig::OverlayX(WeakThis.Get()), FTimingUpConfig::OverlayY(WeakThis.Get()))
			: FVector2D(0.5f, 0.02f);
	};

	OverlayWidget =
		SNew(SConstraintCanvas)
		.Visibility_Lambda([WeakThis]()
		{
			return (WeakThis.IsValid() && FTimingUpConfig::ShowOverlay(WeakThis.Get())) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		+ SConstraintCanvas::Slot()
		.Anchors_Lambda([Pos]() { const FVector2D P = Pos(); return FAnchors(P.X, P.Y); })
		.Alignment_Lambda([Pos]() { return Pos(); })
		.AutoSize(true)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.10f, 0.02f, 0.04f, 0.75f))
			.Padding(FMargin(14.f, 8.f))
			[
				SNew(STextBlock)
				.Font_Lambda([WeakThis]()
				{
					const int32 Size = WeakThis.IsValid() ? FTimingUpConfig::OverlayFontSize(WeakThis.Get()) : 18;
					return FCoreStyle::GetDefaultFontStyle("Bold", Size);
				})
				.Justification(ETextJustify::Center)
				.ShadowOffset(FVector2D(2.f, 2.f))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f))
				.Text_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->GetOverlayText() : FText::GetEmpty(); })
				.ColorAndOpacity_Lambda([WeakThis]() { return WeakThis.IsValid() ? WeakThis->GetOverlayColor() : FSlateColor(FLinearColor::White); })
			]
		];

	GEngine->GameViewport->AddViewportWidgetContent(OverlayWidget.ToSharedRef(), 50);
}

void ATimingUpSubsystem::RemoveOverlay()
{
	if (OverlayWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(OverlayWidget.ToSharedRef());
	}
	OverlayWidget.Reset();
}

#undef LOCTEXT_NAMESPACE
