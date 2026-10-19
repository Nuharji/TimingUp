#include "TimingUpModule.h"

#include "TimingUpSubsystem.h"
#include "TimingUpConfig.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Patching/NativeHookManager.h"
#include "Subsystem/SubsystemActorManager.h"

#include "FGSchematic.h"
#include "FGSchematicManager.h"
#include "FGResearchManager.h"
#include "FGGamePhaseManager.h"

namespace TimingUp
{
	/**
	 * HUB milestones (incl. tier 0) and MAM research count as upgrades.
	 * Only schematics that actually cost something count: free "background unlock"
	 * schematics that the game grants automatically alongside a milestone are ignored.
	 */
	/** Tier 0 (HUB upgrades) is ignored by the challenge when "Tier 0 nicht zählen" is on */
	static bool IsIgnoredTier0(const UObject* Ctx, TSubclassOf<UFGSchematic> Schematic)
	{
		if (!Schematic || !FTimingUpConfig::SkipTier0(Ctx)) return false;
		const ESchematicType Type = UFGSchematic::GetType(Schematic);
		// Only HUB schematics: MAM research also reports tech tier 0 and must still count
		return Type == ESchematicType::EST_Tutorial
			|| (Type == ESchematicType::EST_Milestone && UFGSchematic::GetTechTier(Schematic) == 0);
	}

	static bool IsUpgradeSchematic(const UObject* Ctx, TSubclassOf<UFGSchematic> Schematic)
	{
		if (!Schematic || IsIgnoredTier0(Ctx, Schematic)) return false;
		const ESchematicType Type = UFGSchematic::GetType(Schematic);
		const bool bUpgradeType = Type == ESchematicType::EST_Milestone
			|| Type == ESchematicType::EST_Tutorial
			|| Type == ESchematicType::EST_MAM;
		return bUpgradeType && UFGSchematic::GetCost(Schematic).Num() > 0;
	}

	/** HUB milestones that are subject to the timer */
	static bool IsHubSchematic(const UObject* Ctx, TSubclassOf<UFGSchematic> Schematic)
	{
		if (!Schematic || IsIgnoredTier0(Ctx, Schematic)) return false;
		const ESchematicType Type = UFGSchematic::GetType(Schematic);
		return Type == ESchematicType::EST_Milestone || Type == ESchematicType::EST_Tutorial;
	}

	/** Alternate recipes are forbidden in the challenge (setting) */
	static bool IsForbiddenSchematic(const UObject* Ctx, TSubclassOf<UFGSchematic> Schematic)
	{
		if (!Schematic || !FTimingUpConfig::BlockAlternates(Ctx)) return false;
		const ESchematicType Type = UFGSchematic::GetType(Schematic);
		return Type == ESchematicType::EST_Alternate || Type == ESchematicType::EST_HardDrive;
	}

	/** True when no TimingUp subsystem exists (e.g. main menu) or an upgrade may be spent */
	static bool MaySpend(const UObject* WorldContext)
	{
		ATimingUpSubsystem* Sub = ATimingUpSubsystem::Get(WorldContext);
		return !Sub || Sub->CanSpendUpgrade();
	}
}

void FTimingUpModule::StartupModule()
{
	// Spawn the TimingUp subsystem in every game world (no Blueprint asset needed)
	WorldInitHandle = FWorldDelegates::OnWorldInitializedActors.AddLambda([](const UWorld::FActorsInitializedParams& Params)
	{
		UWorld* World = Params.World;
		if (!World || !World->IsGameWorld()) return;

		// Settings are registered as early as possible (main menu), so they show up under Mods
		FTimingUpConfig::Register(World->GetGameInstance());

		if (World->GetMapName().Contains(TEXT("MenuScene"))) return;

		if (USubsystemActorManager* Manager = World->GetSubsystem<USubsystemActorManager>())
		{
			Manager->RegisterSubsystemActor(ATimingUpSubsystem::StaticClass());
		}
	});

	RegisterHooks();
}

void FTimingUpModule::ShutdownModule()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
}

void FTimingUpModule::RegisterHooks()
{
#if !WITH_EDITOR
	using namespace TimingUp;

	// --- HUB: milestone can only be selected while an upgrade is available
	SUBSCRIBE_METHOD(AFGSchematicManager::CanSetAsActiveSchematic,
		[](TCallScope<bool(*)(AFGSchematicManager*, TSubclassOf<UFGSchematic>)>& Scope, AFGSchematicManager* Self, TSubclassOf<UFGSchematic> Schematic)
		{
			if ((IsHubSchematic(Self, Schematic) && !MaySpend(Self)) || IsForbiddenSchematic(Self, Schematic))
			{
				Scope.Override(false);
			}
		});

	// --- HUB: no paying into a milestone while no upgrade is available
	SUBSCRIBE_METHOD(AFGSchematicManager::PayOffOnSchematic,
		[](TCallScope<bool(*)(AFGSchematicManager*, TSubclassOf<UFGSchematic>, TArray<FItemAmount>&)>& Scope, AFGSchematicManager* Self, TSubclassOf<UFGSchematic> Schematic, TArray<FItemAmount>& Amount)
		{
			if (IsHubSchematic(Self, Schematic) && !MaySpend(Self))
			{
				Scope.Override(false);
			}
		});

	// --- Unlocking: count upgrades, block alternate recipes
	SUBSCRIBE_METHOD(AFGSchematicManager::GiveAccessToSchematic,
		[](TCallScope<void(*)(AFGSchematicManager*, TSubclassOf<UFGSchematic>, AFGCharacterPlayer*, ESchematicUnlockFlags)>& Scope, AFGSchematicManager* Self, TSubclassOf<UFGSchematic> Schematic, AFGCharacterPlayer* Instigator, ESchematicUnlockFlags Flags)
		{
			if (IsForbiddenSchematic(Self, Schematic))
			{
				Scope.Cancel();
				return;
			}
			if (IsUpgradeSchematic(Self, Schematic) && !Self->IsSchematicPurchased(Schematic))
			{
				if (ATimingUpSubsystem* Sub = ATimingUpSubsystem::Get(Self))
				{
					Sub->OnUpgradeSpent(UFGSchematic::GetSchematicDisplayName(Schematic).ToString());
				}
			}
		});

	// --- MAM: research can only be started while an upgrade is available; hard drives are forbidden
	SUBSCRIBE_METHOD(AFGResearchManager::CanResearchBeInitiated,
		[](TCallScope<bool(*)(const AFGResearchManager*, TSubclassOf<UFGSchematic>)>& Scope, const AFGResearchManager* Self, TSubclassOf<UFGSchematic> Schematic)
		{
			if (IsForbiddenSchematic(Self, Schematic) || !MaySpend(Self))
			{
				Scope.Override(false);
			}
		});

	// --- Space elevator: no paying into a phase while no upgrade is available
	SUBSCRIBE_METHOD(AFGGamePhaseManager::PayOffOnTargetGamePhase,
		[](TCallScope<int32(*)(AFGGamePhaseManager*, FItemAmount)>& Scope, AFGGamePhaseManager* Self, FItemAmount PayOff)
		{
			if (!MaySpend(Self))
			{
				Scope.Override(0);
			}
		});

	// --- Space elevator: completing a phase spends an upgrade
	SUBSCRIBE_METHOD(AFGGamePhaseManager::GoToNextGamePhase,
		[](TCallScope<void(*)(AFGGamePhaseManager*)>& Scope, AFGGamePhaseManager* Self)
		{
			if (ATimingUpSubsystem* Sub = ATimingUpSubsystem::Get(Self))
			{
				Sub->OnUpgradeSpent(TEXT("Weltraumaufzug-Phase"));
			}
		});
#endif
}

IMPLEMENT_GAME_MODULE(FTimingUpModule, TimingUp);
