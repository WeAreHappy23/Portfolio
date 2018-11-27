// Fill out your copyright notice in the Description page of Project Settings.

#include "Lever.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"	
#include	"MyCharacter/MotionControllerCharacter.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "HandMotionController/RightHandMotionController.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

#include"Components/BoxComponent.h"
#include"Components/StaticMeshComponent.h"

// キャラクターの手をドアのTransform基準に変えてAtan2で角を探してRotationに適用します。
// 底辺と高さを知る時,ラジアン=atan(高さ/底辺)なので,角度を得ることができます。

ALever::ALever()
{

	PrimaryActorTick.bCanEverTick = true;

	Scene = CreateDefaultSubobject<USceneComponent>(TEXT("Scene"));
	Scene->SetupAttachment(RootComponent);

	LeverScene = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeverScene"));
	LeverScene->SetupAttachment(Scene);

	Lever = CreateDefaultSubobject<UBoxComponent>(TEXT("Lever"));
	Lever->SetupAttachment(LeverScene);

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetupAttachment(Lever);

	// スタティックメッシュを探します。
	static ConstructorHelpers::FObjectFinder<UStaticMesh>PotionShape(TEXT("StaticMesh'/Game/Assets/MapBuild/RoughMap/Bridge/mesh/bridge_door_bridge_door_01.bridge_door_bridge_door_01'"));
	if (PotionShape.Succeeded())
	{
		LeverScene->SetStaticMesh(PotionShape.Object);
	}

	// 各コンポーネントの大きさと位置を設定します。
	LeverScene->SetRelativeScale3D(FVector(4.0f, 4.0f, 4.0f));
	Lever->SetRelativeLocation(FVector(-62.0f, 0.0f, 40.0f));
	Lever->SetRelativeScale3D(FVector(0.09f, 0.09f, 1.0f));
	Collision->SetRelativeLocation(FVector(360.0f, 0.0f, -40.0f));
	Collision->SetRelativeScale3D(FVector(10.0f, 0.7f, 2.6f));

	// 自動的に開く位置です。
	AutoRot = FRotator(LeverScene->RelativeRotation.Pitch, LeverScene->RelativeRotation.Yaw+60.0f, LeverScene->RelativeRotation.Roll);

	Collision->bGenerateOverlapEvents = false;

	// 各コンポーネントのコリジョンの属性を設定します。
	LeverScene->SetCollisionProfileName("NoCollision");		// 重なる時,コリジョンなし
	Lever->SetCollisionProfileName("OverlapAll");			// 重なる時,オーバーラップイベント
	Collision->SetCollisionProfileName("BlockAll");			// 重なる時,ブロック

	Tags.Add(FName("Door"));
}

void ALever::BeginPlay()
{
	Super::BeginPlay();

	Lever->OnComponentBeginOverlap.AddDynamic(this, &ALever::OnLeverOverlap);
	Lever->OnComponentEndOverlap.AddDynamic(this, &ALever::OnLeverEndOverlap);
}

void ALever::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// タッチされたアクターがある時に開くことができます。
	if (TouchActor)
	{
		ARightHandMotionController* RightHand = Cast<ARightHandMotionController>(TouchActor);
		if (RightHand)
		{
			if (RightHand->bisRightGrab)
			{
				// 手をこのアクターのTransformに変換後,手の位置によって変わります。
				FVector Cal = UKismetMathLibrary::InverseTransformLocation
				(GetActorTransform(), RightHand->GetActorLocation());

				float degree = UKismetMathLibrary::RadiansToDegrees(UKismetMathLibrary::Atan2(-Cal.Y, -Cal.X));

				// 角度適用
				LeverScene->SetRelativeRotation(FRotator(0.0f, degree, 0.0f));
			}
		}
	}
	
	// 角度が10以上になると自動的に特定地点まで角度が変わります。
	if (LeverScene->RelativeRotation.Yaw > 10.0f)
	{
		LeverScene->SetRelativeRotation(FMath::Lerp(LeverScene->RelativeRotation, AutoRot, 0.05f));
	}

}

// オーバーラップされたアクターが右手の時,TouchActorに値が入ってきます。
void ALever::OnLeverOverlap(UPrimitiveComponent * OverlappedComp, AActor * OtherActor, UPrimitiveComponent * OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult)
{
	if (OtherActor->ActorHasTag("RightHand"))
	{
		AMotionControllerCharacter* Character = Cast<AMotionControllerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
		if (Character)
		{
			ARightHandMotionController* RightHand = Cast<ARightHandMotionController>(OtherActor);

			if (RightHand)
				TouchActor = Character->RightHand;
		}
	}
}

