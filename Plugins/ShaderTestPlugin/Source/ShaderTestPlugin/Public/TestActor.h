#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TestActor.generated.h"

class UTextureRenderTarget2D;
class FRHICommandListImmediate;

UCLASS()
class SHADERTESTPLUGIN_API ATestActor : public AActor
{
	GENERATED_BODY()
public:	
	ATestActor();
protected:
	virtual void BeginPlay() override;
public:	
	virtual void Tick(float DeltaTime) override;
	void ExecuteTestShader_RenderThread(FRHICommandListImmediate& RHICmdList);
private:
	UPROPERTY(VisibleAnywhere)
	UTextureRenderTarget2D* RenderTarget2D;
};
