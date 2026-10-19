#include "TimingUpModule.h"

#include "TimingUpSubsystem.h"
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
	static bool IsUpgradeSchematic(TSubclassOf<UFGSchematic> Schematic)
	{
		if (!Schematic) return false;
		const ESchematicType Type = UFGSchematic::GetType(Schematic);
		const bool bUpgradeType = Type == ESchematicType::EST_Milestone
			|| Type == ESchematicType::EST_Tutorial
			|| Type == ESchematicType::EST_MAM;
		return bUpgradeType && UFGSchematic::GetCost(Schematic).Num() > 0;
	}

	static bool IsHubSchematic(TSubclassOf<UFGSchematic> Schematic)
	{
		if (!Schematic) return false;
		const ESchematicType Type = UFGSchematic::GetType(Schematic);
		return Type == ESchematicType::EST_Milestone || Type == ESchematicType::EST_Tutorial;
	}

	/** Alternate recipes are forbidden in the challenge */
	static bool IsForbiddenSchematic(TSubclassOf<UFGSchematic> Schematic)
	{
		if (!Schematic) return false;
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
			if ((IsHubSchematic(Schematic) && !MaySpend(Self)) || IsForbiddenSchematic(Schematic))
			{
				Scope.Override(false);
			}
		});

	// --- HUB: no paying into a milestone while no upgrade is available
	SUBSCRIBE_METHOD(AFGSchematicManager::PayOffOnSchematic,
		[](TCallScope<bool(*)(AFGSchematicManager*, TSubclassOf<UFGSchematic>, TArray<FItemAmount>&)>& Scope, AFGSchematicManager* Self, TSubclassOf<UFGSchematic> Schematic, TArray<FItemAmount>& Amount)
		{
			if (IsHubSchematic(Schematic) && !MaySpend(Self))
			{
				Scope.Override(false);
			}
		});

	// --- Unlocking: count upgrades, block alternate recipes
	SUBSCRIBE_METHOD(AFGSchematicManager::GiveAccessToSchematic,
		[](TCallScope<void(*)(AFGSchematicManager*, TSubclassOf<UFGSchematic>, AFGCharacterPlayer*, ESchematicUnlockFlags)>& Scope, AFGSchematicManager* Self, TSubclassOf<UFGSchematic> Schematic, AFGCharacterPlayer* Instigator, ESchematicUnlockFlags Flags)
		{
			if (IsForbiddenSchematic(Schematic))
			{
				Scope.Cancel();
				return;
			}
			if (IsUpgradeSchematic(Schematic) && !Self->IsSchematicPurchased(Schematic))
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
			if (IsForbiddenSchematic(Schematic) || !MaySpend(Self))
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
