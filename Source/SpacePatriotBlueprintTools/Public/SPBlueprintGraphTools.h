#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SPBlueprintGraphTools.generated.h"

/** Editor-only, idempotent graph build and structural audit for the playable Blueprint handoff. */
UCLASS()
class SPACEPATRIOTBLUEPRINTTOOLS_API USPBlueprintGraphTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Editor") static bool BuildShipGraph(FString& Report);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Editor") static bool VerifyShipGraph(FString& Report);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Editor") static bool BuildWorldGraph(FString& Report);
    UFUNCTION(BlueprintCallable, Category="Space Patriot|Editor") static bool VerifyWorldGraph(FString& Report);
};
