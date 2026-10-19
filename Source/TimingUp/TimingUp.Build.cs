using UnrealBuildTool;

public class TimingUp : ModuleRules
{
	public TimingUp(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bLegacyPublicIncludePaths = false;

		PublicDependencyModuleNames.AddRange(new[] {
			"Core", "CoreUObject", "Engine", "InputCore",
			"Slate", "SlateCore", "UMG", "NetCore",
			"FactoryGame", "SML"
		});
	}
}
