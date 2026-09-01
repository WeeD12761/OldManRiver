// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "IglooBlueprintFunctionLibrary.generated.h"


/**
 * 
 */
UCLASS()
class IGLOO_API UIglooBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	
	UFUNCTION(BlueprintCallable, Category = "Canvas Render Target 2D", meta = (WorldContext = "WorldContextObject"))
	static UCanvasRenderTarget2D* CreateCanvasRenderTarget2DWithFormat(UObject* WorldContextObject, TSubclassOf<UCanvasRenderTarget2D> CanvasRenderTarget2DClass, int32 Width = 1024, int32 Height = 1024, ETextureRenderTargetFormat Format = RTF_RGBA16f, bool bNeedsTwoCopies = true);
	
	UFUNCTION(BlueprintCallable, Category = "Canvas Render Target 2D")
	static void RequestCanvasUpdate(UCanvasRenderTarget2D* CanvasRenderTarget2D);
};
