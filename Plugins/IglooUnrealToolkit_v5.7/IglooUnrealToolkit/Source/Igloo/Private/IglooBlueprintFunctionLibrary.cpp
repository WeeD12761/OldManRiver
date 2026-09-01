// Fill out your copyright notice in the Description page of Project Settings.

#include "IglooBlueprintFunctionLibrary.h"
#include "Engine/Engine.h"




UCanvasRenderTarget2D* UIglooBlueprintFunctionLibrary::CreateCanvasRenderTarget2DWithFormat(UObject* WorldContextObject, TSubclassOf<UCanvasRenderTarget2D> CanvasRenderTarget2DClass, int32 Width, int32 Height, ETextureRenderTargetFormat Format, bool bNeedsTwoCopies)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	if (Width > 0 && Height > 0 && World && (CanvasRenderTarget2DClass != NULL) && FApp::CanEverRender())
	{
		UCanvasRenderTarget2D* NewCanvasRenderTarget = NewObject<UCanvasRenderTarget2D>(WorldContextObject, CanvasRenderTarget2DClass);
	//	UCanvasRenderTarget2D* NewCanvasRenderTarget = NewObject<UCanvasRenderTarget2D>(GetTransientPackage(), CanvasRenderTarget2DClass);
		check(NewCanvasRenderTarget);
		//	NewCanvasRenderTarget->World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		NewCanvasRenderTarget->RenderTargetFormat = Format;
		NewCanvasRenderTarget->TargetGamma = 2.2;
		NewCanvasRenderTarget->bNeedsTwoCopies = bNeedsTwoCopies;
		NewCanvasRenderTarget->InitAutoFormat(Width, Height);

#if WITH_EDITORONLY_DATA
		NewCanvasRenderTarget->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
#endif

	//	NewCanvasRenderTarget->AddToRoot();
		//textureTarget->UpdateResourceW();
		NewCanvasRenderTarget->UpdateResource();


		return NewCanvasRenderTarget;
	}

	return nullptr;
}

void UIglooBlueprintFunctionLibrary::RequestCanvasUpdate(UCanvasRenderTarget2D* CanvasRenderTarget2D)
{
	CanvasRenderTarget2D->FastUpdateResource();
}

