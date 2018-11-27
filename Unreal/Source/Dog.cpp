﻿// Fill out your copyright notice in the Description page of Project Settings.

#include "Dog.h"

#include "UObject/ConstructorHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/PawnSensingComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"

#include "Kismet/GameplayStatics.h"
#include "MyCharacter/MotionControllerCharacter.h"

#include "Monster/Dog/DogAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimBlueprint.h"

#include "Camera/CameraComponent.h"

#include "Kismet/KismetMathLibrary.h"

#include "HandMotionController/RightHandMotionController.h"
#include "HandMotionController/LeftHandMotionController.h"
#include "Components/SphereComponent.h"

#include "Equipment/PlayerSword.h"

// プレーヤーを感知すれば追ってきて腕を刺すモンスターです。

ADog::ADog()
{
	PrimaryActorTick.bCanEverTick = true;

	// メッシュを探して適用します。
	static ConstructorHelpers::FObjectFinder<USkeletalMesh>MonsterMesh(TEXT("SkeletalMesh'/Game/Assets/CharacterEquipment/Monster/Dog/Mesh2/MON_DOG_MESH.MON_DOG_MESH'"));
	if (MonsterMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MonsterMesh.Object);
	}

	// ビヘイビアツリーを探して適用します。
	static ConstructorHelpers::FObjectFinder<UBehaviorTree>Monster_BehaviorTree(TEXT("BehaviorTree'/Game/Blueprints/Monster/Dog/AI/RagdollDogBT_2.RagdollDogBT_2'"));
	if (Monster_BehaviorTree.Succeeded())
	{
		BehaviorTree = Monster_BehaviorTree.Object;
	}

	// アニメを探して適用します。
	static ConstructorHelpers::FObjectFinder<UClass>Monster_AnimBlueprint(TEXT("AnimBlueprint'/Game/Blueprints/Monster/Dog/Blueprints2/ABP_Dog_3.ABP_Dog_3_C'"));
	if (Monster_AnimBlueprint.Succeeded())
	{
		UClass* DogAnimBlueprint = Monster_AnimBlueprint.Object;

		if (DogAnimBlueprint)
		{
			GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			GetMesh()->SetAnimInstanceClass(DogAnimBlueprint);
		}
	}

	// マテリアルを探して適用します。
	static ConstructorHelpers::FObjectFinder<UMaterialInterface>Monster_Material(TEXT("Material'/Game/Assets/CharacterEquipment/Monster/Dog/Materials/M_Dog.M_Dog'"));
	if (Monster_Material.Succeeded())
	{
		GetMesh()->SetMaterial(0, Monster_Material.Object);
	}

	// 犬の頭コリジョンのを生成します。
	DogAttackCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("DogAttack"));
	DogAttackCollision->SetupAttachment(GetMesh(), TEXT("HeadSocket"));

	// 感覚コンポーネントを適用します。
	PawnSensing = CreateDefaultSubobject<UPawnSensingComponent>(TEXT("PawnSensing"));

	// AIコントローラを適用します。
	AIControllerClass = ADogAIController::StaticClass();

	// カプセルの大きさとメッシュの大きさと方向を設定します。(Pawnだけを無視。)
	GetCapsuleComponent()->SetCapsuleHalfHeight(55.0f);
	GetCapsuleComponent()->SetCapsuleRadius(30.0f);
	GetCapsuleComponent()->SetCollisionProfileName("IgnoreOnlyPawn");
	GetMesh()->SetRelativeLocation(FVector(-20.0f, 0.0f, -55.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->SetCollisionProfileName("Ragdoll");

	// 犬の頭のコリジョンの大きさと位置を設定します。 (Overlap)
	DogAttackCollision->SetCollisionProfileName(TEXT("OverlapAll"));
	DogAttackCollision->SetRelativeLocation(FVector(10.0f, -10.0f, 0.0f));
	DogAttackCollision->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.6f));
	DogAttackCollision->SetActive(false);
	DogAttackCollision->ComponentTags.Add("DogAttackCollision");
	DogAttackCollision->bGenerateOverlapEvents = false;

	// 感覚の要素を設定します。
	PawnSensing->bHearNoises = false;
	PawnSensing->bSeePawns = true;
	PawnSensing->SetPeripheralVisionAngle(40.0f);
	PawnSensing->SightRadius = 1200.0f;
	PawnSensing->SensingInterval = 0.1f;

	bOnLand = true;
	Landing = false;

	MaxHP = 1.0f;
	CurrentHP = MaxHP;
	bIsDeath = false;
	bIsDetach = false;

	AttackWaite = false;

	Tags.Add("Monster");
	Tags.Add("Dog");
	Tags.Add(FName(TEXT("DisregardForLeftHand")));
	Tags.Add(FName(TEXT("DisregardForRightHand")));
}

void ADog::BeginPlay()
{
	Super::BeginPlay();

	// 状態初期化
	CurrentDogState = EDogState::Idle;
	CurrentDogAnimState = EDogAnimState::Idle;
	CurrentDogBattleState = EDogBattleState::Nothing;
	CurrentDogAirState = EDogAirState::Nothing;
	CurrentDogJumpState = EDogJumpState::Nothing;
	CurrentDogCircleState = EDogCircleState::Nothing;

	// 犬の頭とオーバーラップの時実行する関数を登録します。
	DogAttackCollision->OnComponentBeginOverlap.AddDynamic(this, &ADog::OnAttackCollisionOverlap);

	if (PawnSensing)
	{
		PawnSensing->OnSeePawn.AddDynamic(this, &ADog::OnSeePlayer);
	}
}

void ADog::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AI = Cast<ADogAIController>(GetController());
	FFindFloorResult FloorDistance;
	GetCharacterMovement()->ComputeFloorDist(GetCapsuleComponent()->GetComponentLocation(), 10000.0f, 10000.0f, FloorDistance, 34.0f);

	// ブラックボードに情報を伝達します。
	if (AI)
	{
		AI->BBComponent->SetValueAsEnum("CurrentDogState", (uint8)CurrentDogState);
		AI->BBComponent->SetValueAsEnum("CurrentDogAnimState", (uint8)CurrentDogAnimState);
		AI->BBComponent->SetValueAsEnum("CurrentDogJumpState", (uint8)CurrentDogJumpState);
		AI->BBComponent->SetValueAsEnum("CurrentDogCircleState", (uint8)CurrentDogCircleState);
		AI->BBComponent->SetValueAsEnum("CurrentDogBattleState", (uint8)CurrentDogBattleState);
		AI->BBComponent->SetValueAsEnum("CurrentDogAirState", (uint8)CurrentDogAirState);
		AI->BBComponent->SetValueAsObject("AttachActor", AttachActor);
		AI->BBComponent->SetValueAsBool("bOnLand", bOnLand);
		AI->BBComponent->SetValueAsBool("DeathFlag", bIsDeath);
		AI->BBComponent->SetValueAsFloat("HP", CurrentHP);
		height = FloorDistance.FloorDist;
		CurrentFalling = GetCharacterMovement()->IsFalling();
	}

}

void ADog::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

// プレーヤー感知
void ADog::OnSeePlayer(APawn * Pawn)
{
	FFindFloorResult FloorDistance;;
	// 土地との距離をチェックします。
	GetCharacterMovement()->ComputeFloorDist(GetCapsuleComponent()->GetComponentLocation(), 10000.0f, 10000.0f, FloorDistance, 34, 0);

	// 感知したアクターのタグが'Character'の時,そのアクターは犬のターゲットになります。
	if (Pawn->ActorHasTag("Character") && FloorDistance.FloorDist < 3.0f)
	{
		ADogAIController* AI = Cast<ADogAIController>(GetController());

		if (AI && !AI->BBComponent->GetValueAsObject("Player"))
		{
			// 状態の設定
			Target = Pawn;
			CurrentDogState = EDogState::Chase;
			CurrentDogAnimState = EDogAnimState::Run;
			CurrentDogJumpState = EDogJumpState::Nothing;
			CurrentDogCircleState = EDogCircleState::Nothing;
			AI->BBComponent->SetValueAsObject("Player", Pawn);
			GetCharacterMovement()->MaxWalkSpeed = 550.0f;
		}
	}
}

// 犬とプレーヤーの頭とオーバーラップされれば,腕を刺すことになります。
void ADog::OnAttackCollisionOverlap(UPrimitiveComponent * OverlappedComp, AActor * OtherActor, UPrimitiveComponent * OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult)
{
	if (OtherComp->ComponentHasTag("Head"))
	{
		UGameplayStatics::ApplyDamage(OtherActor, 20, UGameplayStatics::GetPlayerController(GetWorld(), 0), this, nullptr);		// オーバーラップされたアクターにダメージを与えます。

		AMotionControllerCharacter* Character = Cast<AMotionControllerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

		if (Character)
		{
			ARightHandMotionController* RightController = Cast<ARightHandMotionController>(Character->RightHand);

			// 腕に犬がなければ,貼ります。
			if (!RightController->AttachDog)
			{
				RightController->AttachDog = this;

				DogAttackCollision->bGenerateOverlapEvents = false;			// 頭のコリジョン非活性化

				// 貼ります。
				FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, true);
				AttachToComponent(RightController->AttachDogPosition, AttachRules);

				// 犬をレグドルさせます。
				GetMesh()->SetAllBodiesBelowSimulatePhysics("Bip002-Spine1", true, true);
				GetCapsuleComponent()->SetCapsuleRadius(10.0f);
				GetCapsuleComponent()->SetCapsuleHalfHeight(10.0f);

				SetActorRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
				SetActorRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));

				AttachActor = RightController;
				AI->BBComponent->SetValueAsBool("bIsBiting", true);
			}
		}
	}
}

// ダメージを受けます。
float ADog::TakeDamage(float Damage, FDamageEvent const & DamageEvent, AController * EventInstigator, AActor * DamageCauser)
{
	APlayerSword* Sword = Cast<APlayerSword>(DamageCauser);
	if (CurrentDogState == EDogState::Bite && Sword)
	{
		return 0;
	}

	CurrentHP -= Damage;

	// 腕を刺したら離れてでなければ死にます。
	if (CurrentHP < 0.0f)
	{
		DogAttackCollision->bGenerateOverlapEvents = false;

		if (CurrentDogState == EDogState::Bite)
		{
			CurrentDogAnimState = EDogAnimState::FallingDeath;
			bIsDeath = true;
			bpunchDetach = true;
		}
		else if (CurrentDogState == EDogState::Battle)
		{
			CurrentDogState = EDogState::Death;
		}
	}

	return Damage;
}

