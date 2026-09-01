#pragma once
#include "SpoutBPFunctionLibrary.generated.h"

UENUM(BlueprintType)
enum class ESpoutType : uint8
{
    Sender
};

UENUM(BlueprintType)
enum class ESpoutState : uint8
{
    ER,     // Exists + Registered
    EnoR,   // Exists + not Registered (by us)
    noER,   // Not Exists + Registered (stale)
    noEnoR  // Not Exists + not Registered
};

UENUM(BlueprintType)
enum class ESpoutSendTextureFrom : uint8
{
    GameViewport,
    TextureRenderTarget2D
};

// Forward decls (only used in non-UHT members)
struct ID3D11Texture2D;
class  spoutFrameCount;
enum   DXGI_FORMAT : int; 

USTRUCT(BlueprintType)
struct FSenderStruct
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spout")
    FName sName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spout")
    bool bIsAlive = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spout")
    ESpoutType spoutType = ESpoutType::Sender;

    // Store OS handle as raw integer for UHT; cast to HANDLE in .cpp
    uint64 sHandle = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spout")
    int32 w = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spout")
    int32 h = 0;

    // --- Non-UHT fields (no UPROPERTY!) ---
    ID3D11Texture2D* activeTextures = nullptr; // shared DX11 texture we copy into
    spoutFrameCount* frame          = nullptr; // optional frame counter

    // convenience setters
    void SetName(FName In)      { sName = In; }
    void SetHandle(uint64 In)   { sHandle = In; }
    void SetW(int32 In)         { w = In; }
    void SetH(int32 In)         { h = In; }
};

UCLASS(ClassGroup=Spout, Blueprintable)
class SPOUTPLUGIN_API USpoutBPFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Create or update a sender registration (internal)
    static bool CreateRegisterSender(FName spoutName, unsigned int width, unsigned int height, DXGI_FORMAT format);
    static bool UpdateRegisteredSpout(FName spoutName, unsigned int width, unsigned int height, DXGI_FORMAT format);

    // Send
    UFUNCTION(BlueprintCallable, Category="Spout", meta=(AdvancedDisplay="2"))
    static bool SpoutSender(FName spoutName, ESpoutSendTextureFrom sendTextureFrom,
                            UTextureRenderTarget2D* textureRenderTarget2D,
                            float targetGamma = 2.2f, bool discardWarningMessage = false);

    // Close
    UFUNCTION(BlueprintCallable, Category="Spout")
    static void CloseSender(FName spoutName);

    // Info
    UFUNCTION(BlueprintCallable, Category="Spout")
    static bool SpoutInfo(TArray<FSenderStruct>& Senders);

    UFUNCTION(BlueprintCallable, Category="Spout")
    static bool SpoutInfoFrom(FName spoutName, FSenderStruct& SenderStruct);

    UFUNCTION(BlueprintCallable, Category="Spout")
    static int32 SetMaxSenders(int32 max);

    UFUNCTION(BlueprintCallable, Category="Spout")
    static void GetMaxSenders(int32& max);

    UFUNCTION(BlueprintCallable, Category="Spout")
    static UTextureRenderTarget2D* CreateTextureRenderTarget2D(
        int32 w=1024, int32 h=768, EPixelFormat pixelFormat=EPixelFormat::PF_B8G8R8A8, bool forceLinearGamma=true);
};
