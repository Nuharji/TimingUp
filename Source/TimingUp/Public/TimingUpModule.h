#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FTimingUpModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterHooks();
	FDelegateHandle WorldInitHandle;
};
