#include "TimingUpConfig.h"

#include "Configuration/ConfigManager.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyFloat.h"
#include "Configuration/Properties/ConfigPropertyInteger.h"
#include "Configuration/Properties/WidgetExtension/CP_Float.h"
#include "Configuration/Properties/WidgetExtension/CP_Integer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

const FString FTimingUpConfig::KeyOverlayShow      = TEXT("OverlayShow");
const FString FTimingUpConfig::KeyOverlayX         = TEXT("OverlayX");
const FString FTimingUpConfig::KeyOverlayY         = TEXT("OverlayY");
const FString FTimingUpConfig::KeyOverlayFontSize  = TEXT("OverlayFontSize");
const FString FTimingUpConfig::KeyMaxUpgrades      = TEXT("MaxUpgrades");
const FString FTimingUpConfig::KeyFirstWaitMinutes = TEXT("FirstWaitMinutes");
const FString FTimingUpConfig::KeyWaitFactor       = TEXT("WaitFactor");
const FString FTimingUpConfig::KeySkipTier0        = TEXT("SkipTier0");
const FString FTimingUpConfig::KeyBlockAlternates  = TEXT("BlockAlternates");
const FString FTimingUpConfig::KeyTimeScale        = TEXT("TimeScale");

static FConfigId GetTimingUpConfigId()
{
	FConfigId Id;
	Id.ModReference = TEXT("TimingUp");
	return Id;
}

UTimingUpModConfiguration::UTimingUpModConfiguration()
{
	ConfigId = GetTimingUpConfigId();
	DisplayName = FText::FromString(TEXT("TimingUp!"));
	Description = FText::FromString(TEXT("Einstellungen für die TimingUp!-Challenge (Novy Labs)"));
}

namespace
{
	// SML's Blueprint property classes provide the in-game widgets. If they can't be loaded,
	// the plain C++ classes are used (setting still works via the config file, just without a widget).
	UClass* LoadPropertyClass(const TCHAR* BlueprintName, UClass* Fallback)
	{
		const FString Path = FString::Printf(TEXT("/SML/Interface/UI/Menu/Mods/ConfigProperties/%s.%s_C"), BlueprintName, BlueprintName);
		UClass* Loaded = LoadClass<UConfigProperty>(nullptr, *Path);
		return Loaded ? Loaded : Fallback;
	}

	template<typename T>
	T* AddProperty(UConfigPropertySection* Section, const FString& Key, const TCHAR* Blueprint, const FString& Name, const FString& Tooltip)
	{
		UClass* Class = LoadPropertyClass(Blueprint, T::StaticClass());
		if (!Class->IsChildOf(T::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract))
		{
			Class = T::StaticClass();
		}
		T* Property = NewObject<T>(Section, Class, *Key);
		Property->DisplayName = FText::FromString(Name);
		Property->Tooltip = FText::FromString(Tooltip);
		Section->SectionProperties.Add(Key, Property);
		return Property;
	}

	void AddBool(UConfigPropertySection* S, const FString& Key, bool Default, const FString& Name, const FString& Tooltip)
	{
		UConfigPropertyBool* P = AddProperty<UConfigPropertyBool>(S, Key, TEXT("BP_ConfigPropertyBool"), Name, Tooltip);
		P->DefaultValue = Default;
		P->Value = Default;
	}

	void AddFloat(UConfigPropertySection* S, const FString& Key, float Default, float Min, float Max, const FString& Name, const FString& Tooltip)
	{
		UConfigPropertyFloat* P = AddProperty<UConfigPropertyFloat>(S, Key, TEXT("BP_ConfigPropertyFloat"), Name, Tooltip);
		P->DefaultValue = Default;
		P->Value = Default;
		if (UCP_Float* Ext = Cast<UCP_Float>(P))
		{
			Ext->WidgetType = ECP_FloatWidgetType::CPF_Spinbox;
			Ext->MinValue = Min;
			Ext->MaxValue = Max;
		}
	}

	void AddInt(UConfigPropertySection* S, const FString& Key, int32 Default, int32 Min, int32 Max, const FString& Name, const FString& Tooltip)
	{
		UConfigPropertyInteger* P = AddProperty<UConfigPropertyInteger>(S, Key, TEXT("BP_ConfigPropertyInteger"), Name, Tooltip);
		P->DefaultValue = Default;
		P->Value = Default;
		if (UCP_Integer* Ext = Cast<UCP_Integer>(P))
		{
			Ext->MinValue = Min;
			Ext->MaxValue = Max;
		}
	}
}

void FTimingUpConfig::Register(UGameInstance* GameInstance)
{
	if (!GameInstance) return;
	UConfigManager* Manager = GameInstance->GetSubsystem<UConfigManager>();
	if (!Manager || Manager->GetConfigurationById(GetTimingUpConfigId())) return;

	UTimingUpModConfiguration* CDO = GetMutableDefault<UTimingUpModConfiguration>();
	UClass* SectionClass = LoadPropertyClass(TEXT("BP_ConfigPropertySection"), UConfigPropertySection::StaticClass());
	if (!SectionClass->IsChildOf(UConfigPropertySection::StaticClass()) || SectionClass->HasAnyClassFlags(CLASS_Abstract))
	{
		SectionClass = UConfigPropertySection::StaticClass();
	}
	UConfigPropertySection* Root = NewObject<UConfigPropertySection>(CDO, SectionClass, TEXT("Root"));
	Root->DisplayName = FText::FromString(TEXT("TimingUp!"));

	// --- Anzeige
	AddBool (Root, KeyOverlayShow, true, TEXT("Anzeige einblenden"), TEXT("Zeigt Upgrades, Spielzeit und Countdown im Spiel an."));
	AddFloat(Root, KeyOverlayX, 50.f, 0.f, 100.f, TEXT("Anzeige: Position links/rechts (%)"), TEXT("0 = ganz links, 50 = Mitte, 100 = ganz rechts."));
	AddFloat(Root, KeyOverlayY, 2.f, 0.f, 100.f, TEXT("Anzeige: Position oben/unten (%)"), TEXT("0 = ganz oben, 100 = ganz unten."));
	AddInt  (Root, KeyOverlayFontSize, 18, 8, 72, TEXT("Anzeige: Schriftgröße"), TEXT("Schriftgröße der Anzeige."));

	// --- Challenge
	AddInt  (Root, KeyMaxUpgrades, 30, 1, 200, TEXT("Maximale Upgrades"), TEXT("Wie viele Upgrades es insgesamt gibt. Die Challenge endet, wenn der Countdown für das Upgrade danach abläuft."));
	AddFloat(Root, KeyFirstWaitMinutes, 1.f, 0.1f, 600.f, TEXT("Erste Wartezeit (Minuten)"), TEXT("Wartezeit bis zum ersten Upgrade."));
	AddFloat(Root, KeyWaitFactor, 1.2f, 1.f, 5.f, TEXT("Wartezeit-Faktor"), TEXT("Jede Wartezeit ist so viel Mal länger als die vorherige (1,2 = +20 %)."));
	AddBool (Root, KeySkipTier0, true, TEXT("Tier 0 nicht zählen"), TEXT("HUB-Upgrades aus Tier 0 sind frei und zählen nicht als Upgrade. Tipp: Beim Erstellen des Spielstands \"Onboarding überspringen\" wählen."));
	AddBool (Root, KeyBlockAlternates, true, TEXT("Alternativrezepte sperren"), TEXT("Festplatten-Forschung und Alternativrezepte sind gesperrt."));

	// --- Test
	AddFloat(Root, KeyTimeScale, 1.f, 1.f, 1000.f, TEXT("Testmodus: Zeit-Beschleunigung"), TEXT("1 = normal. Zum Testen z. B. 60. FÜR DIE AUFNAHME AUF 1 LASSEN!"));

	CDO->RootSection = Root;
	Manager->RegisterModConfiguration(UTimingUpModConfiguration::StaticClass());
}

static UConfigProperty* FindProperty(const UObject* WorldContext, const FString& Key)
{
	if (!WorldContext) return nullptr;
	const UWorld* World = WorldContext->GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UConfigManager* Manager = GameInstance ? GameInstance->GetSubsystem<UConfigManager>() : nullptr;
	UConfigPropertySection* Root = Manager ? Manager->GetConfigurationRootSection(GetTimingUpConfigId()) : nullptr;
	return Root ? Root->SectionProperties.FindRef(Key) : nullptr;
}

bool FTimingUpConfig::GetBool(const UObject* WorldContext, const FString& Key, bool Default)
{
	const UConfigPropertyBool* P = Cast<UConfigPropertyBool>(FindProperty(WorldContext, Key));
	return P ? P->Value : Default;
}

float FTimingUpConfig::GetFloat(const UObject* WorldContext, const FString& Key, float Default)
{
	const UConfigPropertyFloat* P = Cast<UConfigPropertyFloat>(FindProperty(WorldContext, Key));
	return P ? P->Value : Default;
}

int32 FTimingUpConfig::GetInt(const UObject* WorldContext, const FString& Key, int32 Default)
{
	const UConfigPropertyInteger* P = Cast<UConfigPropertyInteger>(FindProperty(WorldContext, Key));
	return P ? P->Value : Default;
}
