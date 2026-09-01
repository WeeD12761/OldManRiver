#include "../Public/SpoutBPFunctionLibrary.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif
#include "SpoutDirectX.h"
#include "SpoutFrameCount.h"
#include <d3d11.h>
#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#include "SpoutPluginPrivatePCH.h"
#include "../Public/SpoutModule.h"
#include "SpoutSenderNames.h"

#include <string>
#include <sstream>

// Keep link for some distros that reference it
#pragma comment(lib, "opengl32.lib")

// -------------------------
// Globals / singletons
// -------------------------
static ID3D11Device* g_D3D11Device = nullptr;
static ID3D11DeviceContext* g_pImmediateContext = nullptr; // not used in RT code paths, but nulled on shutdown

static spoutSenderNames* sender = nullptr;
static spoutDirectX*     sdx    = nullptr;

#include "D3D11On12Interop.h"
static FD3D11On12Interop dx12Interop;

static TArray<FSenderStruct> FSenders;

// (Kept only because some projects load this material; not used by sender codepath)
static UMaterialInterface* BaseMaterial = nullptr;
static FName TextureParameterName = "SpoutTexture";

// -------------------------
// Module shutdown
// -------------------------
void FSpoutModule::ShutdownModule()
{
	// Close on GT (will enqueue RT cleanups)
	for (int32 i = FSenders.Num() - 1; i >= 0; --i)
	{
		USpoutBPFunctionLibrary::CloseSender(FSenders[i].sName);
	}
	FSenders.Empty();

	// Drain all RT work before we tear down interop/context
	FlushRenderingCommands();

	if (sender) { delete sender; sender = nullptr; }
	if (sdx)    { delete sdx;    sdx    = nullptr; }

	ENQUEUE_RENDER_COMMAND(ReleaseInterop)(
		[](FRHICommandListImmediate& RHICmdList)
		{
			dx12Interop.releaseInterop();
			g_D3D11Device = nullptr;
			g_pImmediateContext = nullptr;
		});
	FlushRenderingCommands();

	UE_LOG(SpoutUELog, Warning, TEXT("Spout Module Shutdown"));
}

// -------------------------
// Helpers (some retained for API compatibility; sender path doesn’t use the UI material)
// -------------------------
static void DestroyTexture(UTexture2D*& Texture)
{
	if (Texture)
	{
		Texture->RemoveFromRoot();
		if (Texture->GetResource())
		{
			BeginReleaseResource(Texture->GetResource());
			FlushRenderingCommands();
		}
		Texture->MarkAsGarbage();
		Texture = nullptr;
	}
}

static void ResetMatInstance(UTexture2D*& Texture, UMaterialInstanceDynamic*& MaterialInstance)
{
	if (!Texture || !BaseMaterial || TextureParameterName.IsNone()) return;

	if (!MaterialInstance)
	{
		MaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, nullptr);
		if (!MaterialInstance) return;
	}

	UTexture* Tex = nullptr;
	if (!MaterialInstance->GetTextureParameterValue(TextureParameterName, Tex)) return;
	MaterialInstance->SetTextureParameterValue(TextureParameterName, Texture);
}

static void ResetTexture(UTexture2D*& Texture, UMaterialInstanceDynamic*& MaterialInstance, FSenderStruct*& SenderStruct)
{
	DestroyTexture(Texture);
	Texture = UTexture2D::CreateTransient(SenderStruct->w, SenderStruct->h, PF_B8G8R8A8);
	Texture->AddToRoot();
	Texture->UpdateResource();
	//SenderStruct->Texture2DResource = Texture->GetResource();
	ResetMatInstance(Texture, MaterialInstance);
}

// -------------------------
// Init / device
// -------------------------
static void initSpout()
{
	if (!sender) sender = new spoutSenderNames;
	if (!sdx)    sdx    = new spoutDirectX;
}

static void GetDevice()
{
	ENQUEUE_RENDER_COMMAND(InitSpoutInterop)(
		[](FRHICommandListImmediate& RHICmdList)
		{
			if (!dx12Interop.createInterop(GDynamicRHI))
			{
				UE_LOG(SpoutUELog, Error, TEXT("Spout interop init failed"));
				g_D3D11Device = nullptr;
				g_pImmediateContext = nullptr;
				return;
			}
			g_D3D11Device       = dx12Interop.getDevice11();
			g_pImmediateContext = dx12Interop.getDeviceContext11();
		});
	FlushRenderingCommands();
}

// -------------------------
// Public API
// -------------------------
UTextureRenderTarget2D* USpoutBPFunctionLibrary::CreateTextureRenderTarget2D(
	int32 w, int32 h, EPixelFormat pixelFormat, bool forceLinearGamma)
{
	UTextureRenderTarget2D* textureTarget = NewObject<UTextureRenderTarget2D>();
	textureTarget->bNeedsTwoCopies = true;
	textureTarget->InitCustomFormat(w, h, pixelFormat, forceLinearGamma);
	textureTarget->AddressX = TextureAddress::TA_Wrap;
	textureTarget->AddressY = TextureAddress::TA_Wrap;
#if WITH_EDITORONLY_DATA
	textureTarget->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
#endif
	textureTarget->AddToRoot();
	textureTarget->UpdateResource();
	return textureTarget;
}

int32 USpoutBPFunctionLibrary::SetMaxSenders(int32 max)
{
	if (!sender) initSpout();
	sender->SetMaxSenders(max);
	return max;
}

void USpoutBPFunctionLibrary::GetMaxSenders(int32& max)
{
	if (!sender) initSpout();
	max = sender->GetMaxSenders();
}

bool USpoutBPFunctionLibrary::SpoutInfo(TArray<FSenderStruct>& Senders)
{
	Senders = FSenders;
	return true;
}

bool USpoutBPFunctionLibrary::SpoutInfoFrom(FName spoutName, FSenderStruct& Out)
{
	auto Pred = [&](const FSenderStruct& InItem) { return InItem.sName == spoutName; };
	if (const FSenderStruct* Found = FSenders.FindByPredicate(Pred))
	{
		Out = *Found;
		return true;
	}
	UE_LOG(SpoutUELog, Warning, TEXT("No Sender was found with the name : %s"), *spoutName.GetPlainNameString());
	return false;
}

// -------------------------
// Sender registry helpers
// -------------------------
static ESpoutState CheckSenderState(FName spoutName)
{
	auto Pred = [&](const FSenderStruct& InItem) { return InItem.sName == spoutName; };
	const bool bInList = FSenders.ContainsByPredicate(Pred);

	ESpoutState state = ESpoutState::noEnoR;

	if (sender->FindSenderName(TCHAR_TO_ANSI(*spoutName.ToString())))
	{
		state = bInList ? ESpoutState::ER : ESpoutState::EnoR;
	}
	else
	{
		state = bInList ? ESpoutState::noER : ESpoutState::noEnoR;
	}
	return state;
}

// Create shared DX11 texture on RT, register as BGRA8, then add to FSenders on GT
bool USpoutBPFunctionLibrary::CreateRegisterSender(FName spoutName, unsigned int width, unsigned int height, DXGI_FORMAT /*format*/)
{
	if (!sender || !sdx) initSpout();
	if (!g_D3D11Device)  GetDevice();

	// result container
	struct FCreateResult { bool bOK=false; uint64 Handle=0; ID3D11Texture2D* Tex=nullptr; };
	FCreateResult Result;

	ENQUEUE_RENDER_COMMAND(CreateSharedTex)(
		[width, height, &Result](FRHICommandListImmediate& RHICmdList)
		{
			ID3D11Device* Dev = g_D3D11Device;
			if (!Dev) return;

			const DXGI_FORMAT TexFmt = DXGI_FORMAT_B8G8R8A8_UNORM;
			HANDLE SharedHandle = NULL;                           // real OS handle here
			ID3D11Texture2D* SendingTexture = nullptr;

			const bool ok = sdx->CreateSharedDX11Texture(Dev, width, height, TexFmt, &SendingTexture, SharedHandle);
			if (ok)
			{
				Result.bOK    = true;
				Result.Handle = static_cast<uint64>(reinterpret_cast<uintptr_t>(SharedHandle)); // -> uint64
				Result.Tex    = SendingTexture;
			}
		});
	FlushRenderingCommands();

	if (!Result.bOK) return false;

	// Register with Spout (cast back to HANDLE)
	sender->CreateSender(
		TCHAR_TO_ANSI(*spoutName.ToString()), width, height,
		reinterpret_cast<HANDLE>(static_cast<uintptr_t>(Result.Handle)),
		DXGI_FORMAT_B8G8R8A8_UNORM);

	// Fill our registry entry
	FSenderStruct NewEntry;
	NewEntry.SetW(width);
	NewEntry.SetH(height);
	NewEntry.SetName(spoutName);
	NewEntry.bIsAlive   = true;
	NewEntry.spoutType  = ESpoutType::Sender;
	NewEntry.SetHandle(Result.Handle);        // stays as uint64
	NewEntry.activeTextures = Result.Tex;
	NewEntry.frame = new spoutFrameCount();
	if (!NewEntry.frame->CreateAccessMutex(TCHAR_TO_ANSI(*spoutName.ToString())))
		UE_LOG(SpoutUELog, Warning, TEXT("CreateAccessMutex failure: %s"), *spoutName.ToString());
	NewEntry.frame->EnableFrameCount(TCHAR_TO_ANSI(*spoutName.ToString()));

	FSenders.Add(MoveTemp(NewEntry));
	return true;
}

// Recreate shared texture on RT, then update Spout & registry on GT
bool USpoutBPFunctionLibrary::UpdateRegisteredSpout(FName spoutName, unsigned int width, unsigned int height, DXGI_FORMAT /*format*/)
{
	if (!g_D3D11Device) GetDevice();

	struct FUpd { bool OK=false; uint64 Handle=0; ID3D11Texture2D* Tex=nullptr; };
	FUpd R;

	ENQUEUE_RENDER_COMMAND(SpoutUpdateSharedTex)(
		[width, height, &R](FRHICommandListImmediate& RHICmdList)
		{
			ID3D11Device* Dev = g_D3D11Device;
			if (!Dev) return;

			const DXGI_FORMAT TexFmt = DXGI_FORMAT_B8G8R8A8_UNORM;
			HANDLE SharedHandle = NULL;                        // real handle
			ID3D11Texture2D* SendingTexture = nullptr;

			const bool ok = sdx->CreateSharedDX11Texture(Dev, width, height, TexFmt, &SendingTexture, SharedHandle);
			if (ok) { R.OK = true; R.Handle = static_cast<uint64>(reinterpret_cast<uintptr_t>(SharedHandle)); R.Tex = SendingTexture; }
		});
	FlushRenderingCommands();

	if (!R.OK) return false;

	// Update our GT registry
	for (FSenderStruct& S : FSenders)
	{
		if (S.sName == spoutName)
		{
			S.SetW(width);
			S.SetH(height);
			S.SetHandle(R.Handle);         // stays as uint64
			S.activeTextures = R.Tex;
			S.spoutType = ESpoutType::Sender;
			break;
		}
	}

	// Spout CPU-side update (cast back to HANDLE)
	sender->UpdateSender(
		TCHAR_TO_ANSI(*spoutName.ToString()), width, height,
		reinterpret_cast<HANDLE>(static_cast<uintptr_t>(R.Handle)),
		DXGI_FORMAT_B8G8R8A8_UNORM);

	return true;
}

bool USpoutBPFunctionLibrary::SpoutSender(FName spoutName, ESpoutSendTextureFrom sendTextureFrom, UTextureRenderTarget2D* textureRenderTarget2D, float targetGamma, bool discardWarningMessage)
{
	if (!sender || !sdx) initSpout();
	if (!g_D3D11Device)  GetDevice();

	FTextureRHIRef sentTexture;
	switch (sendTextureFrom)
	{
	case ESpoutSendTextureFrom::GameViewport:
		sentTexture = (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
			? GEngine->GameViewport->Viewport->GetRenderTargetTexture().GetReference()
			: nullptr;
		break;
	case ESpoutSendTextureFrom::TextureRenderTarget2D:
		if (!textureRenderTarget2D)
		{
			if (!discardWarningMessage)
				UE_LOG(SpoutUELog, Warning, TEXT("No TextureRenderTarget2D Selected!!"));
			return false;
		}
		textureRenderTarget2D->TargetGamma = targetGamma;
		sentTexture = (textureRenderTarget2D->GetResource() && textureRenderTarget2D->GetResource()->TextureRHI.IsValid())
			? textureRenderTarget2D->GetResource()->TextureRHI->GetTexture2D()
			: nullptr;
		break;
	default:
		break;
	}

	if (!sentTexture.IsValid())
	{
		if (!discardWarningMessage)
			UE_LOG(SpoutUELog, Warning, TEXT("Texture is not yet ready to be sent"));
		return false;
	}

	// Get current source dimensions (may be HDR etc.; we normalize to BGRA8 on the destination/shared)
	unsigned int width = 0, height = 0;
	dx12Interop.getTextureFormat(sentTexture, width, height);

	// Ensure registration
	ESpoutState state = CheckSenderState(spoutName);
	if (state == ESpoutState::noEnoR || state == ESpoutState::noER)
	{
		UE_LOG(SpoutUELog, Display, TEXT("Creating and registering new Sender..."));
		CreateRegisterSender(spoutName, width, height, DXGI_FORMAT_B8G8R8A8_UNORM);
		return false; // first frame registers; copy on next call
	}

	if (state == ESpoutState::EnoR)
	{
		UE_LOG(SpoutUELog, Warning, TEXT("A Sender with the name %s already exists (external)."), *spoutName.GetPlainNameString());
		return false;
	}

	// state == ER
	FSenderStruct* SenderStruct = nullptr;
	{
		auto Pred = [&](const FSenderStruct& InItem) { return InItem.sName == spoutName; };
		SenderStruct = FSenders.FindByPredicate(Pred);
	}
	if (!SenderStruct || !SenderStruct->activeTextures)
	{
		UE_LOG(SpoutUELog, Warning, TEXT("SenderStruct or activeTextures missing for %s"), *spoutName.ToString());
		return false;
	}

	// Resize if needed
	if ((int32)width != SenderStruct->w || (int32)height != SenderStruct->h)
	{
		UE_LOG(SpoutUELog, Display, TEXT("Texture size changed (%dx%d -> %dx%d). Updating sender."),
			SenderStruct->w, SenderStruct->h, width, height);
		UpdateRegisteredSpout(spoutName, width, height, DXGI_FORMAT_B8G8R8A8_UNORM);
		return false;
	}

	// --- Prepare POD/COM captures for RT/RHI-thread work (no struct deref inside the lambda)
	ID3D11Texture2D* TargetTex   = SenderStruct->activeTextures;
	spoutFrameCount*  FramePtr   = SenderStruct->frame;
	uint64            SharedH    = SenderStruct->sHandle;
	const FString     NameCopy   = spoutName.ToString();

	ENQUEUE_RENDER_COMMAND(SpoutCopyToDX11)(
		[sentTexture, TargetTex, FramePtr, SharedH, NameCopy](FRHICommandListImmediate& RHICmdList)
		{
			// Hop to the RHI thread for all D3D11 immediate-context work
			RHICmdList.EnqueueLambda([sentTexture, TargetTex, FramePtr, SharedH, NameCopy](FRHICommandListImmediate& InRHICmdList)
			{
				if (!TargetTex) return;

				unsigned int w = 0, h = 0;
				dx12Interop.getTextureFormat(sentTexture, w, h);

				// Copy via interop (internally uses 11 or 11-on-12 context)
				dx12Interop.copyToDx11Texture(sentTexture, TargetTex);

				// Spout CPU-side update (dimensions only)
				sender->UpdateSender(
							TCHAR_TO_ANSI(*NameCopy), w, h,
							reinterpret_cast<HANDLE>(static_cast<uintptr_t>(SharedH)),
							DXGI_FORMAT_B8G8R8A8_UNORM);

				if (FramePtr)
				{
					FramePtr->AllowTextureAccess(TargetTex);
					FramePtr->SetNewFrame();
				}
			});
		});

	return true;
}

void USpoutBPFunctionLibrary::CloseSender(FName spoutName)
{
	if (!sender) initSpout();
	if (!g_D3D11Device) GetDevice();

	ESpoutState state = CheckSenderState(spoutName);

	if (state == ESpoutState::noEnoR)
	{
		UE_LOG(SpoutUELog, Warning, TEXT("%s is already closed; nothing to close."), *spoutName.GetPlainNameString());
		return;
	}
	if (state == ESpoutState::EnoR)
	{
		UE_LOG(SpoutUELog, Warning, TEXT("A Sender with the name %s exists but isn't ours; cannot close."), *spoutName.GetPlainNameString());
		return;
	}

	// Find our entry (if any)
	FSenderStruct* Entry = nullptr;
	int32 Index = INDEX_NONE;
	for (int32 i = 0; i < FSenders.Num(); ++i)
	{
		if (FSenders[i].sName == spoutName)
		{
			Entry = &FSenders[i];
			Index = i;
			break;
		}
	}

	if (Entry)
	{
		// Release Spout name
		sender->ReleaseSenderName(TCHAR_TO_ANSI(*spoutName.ToString()));

		// Frame mutex cleanup
		if (Entry->frame)
		{
			delete Entry->frame;
			Entry->frame = nullptr;
		}

		// Release our shared texture on RT
		ID3D11Texture2D* TexToRelease = Entry->activeTextures;
		ENQUEUE_RENDER_COMMAND(SpoutReleaseSenderTex)(
			[TexToRelease](FRHICommandListImmediate& RHICmdList)
			{
				if (TexToRelease) TexToRelease->Release();
			});
		Entry->activeTextures = nullptr;

		// Remove from registry
		FSenders.RemoveAt(Index);
	}
	UE_LOG(SpoutUELog, Display, TEXT("There are now %i senders remaining"), FSenders.Num());
}
