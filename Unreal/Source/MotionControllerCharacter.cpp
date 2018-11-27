// Fill out your copyright notice in the Description page of Project Settings.

#include "MotionControllerCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"

#include "GameFramework/CharacterMovementComponent.h"

#include "HandMotionController/LeftHandMotionController.h"
#include "HandMotionController/RightHandMotionController.h"

#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"

#include "Public/TimerManager.h" 
#include "Equipment/PlayerShield.h"
#include "Components/WidgetComponent.h"
#include "HandMotionController/Widget/LeftHandWidget.h"

#include "Components/StereoLayerComponent.h"
#include "Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"

#include "MyCharacter/Widget/HitBloodyWidget.h"

#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"

#include "Equipment/PlayerSword.h"

#include "Monster/Dog/Dog.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "MyCharacter/Widget/HPStaminaBar.h"

#include "MyCharacter/Widget/Menu.h"
#include "Components/WidgetInteractionComponent.h"	
#include "TimerManager.h"
#include "Components/PawnNoiseEmitterComponent.h"

// キャラクターです。主席部分は他のプログラマーが作成しました。

AMotionControllerCharacter::AMotionControllerCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationYaw = true;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bInheritPitch = true;
	SpringArm->bInheritYaw = true;
	SpringArm->bInheritRoll = false;
	SpringArm->TargetArmLength = 1.0f;

	GetCapsuleComponent()->bHiddenInGame = false;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);	
	
	HeadBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HeadBox"));
	HeadBox->SetupAttachment(Camera);

	// カメラの前にStereoLayerComponentをつけて,ウィジェットを示すようにしました。
	Stereo = CreateDefaultSubobject<UStereoLayerComponent>(TEXT("StereoB"));
	Stereo->SetupAttachment(Camera);

	Widget = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetB"));
	Widget->SetupAttachment(Camera);

	Stereo->SetRelativeLocation(FVector(100.0f, 0.0f, 0.0f));
	Stereo->bLiveTexture = true;
	Stereo->SetQuadSize(FVector2D(250.0f, 250.0f));

	// 襲撃時に発生するウィジェットを探して適用します。
	static ConstructorHelpers::FClassFinder<UUserWidget> HitUI(TEXT("WidgetBlueprint'/Game/Blueprints/UI/BloodEffectHUD.BloodEffectHUD_C'"));
	if (HitUI.Succeeded())
	{
		Widget->SetWidgetClass(HitUI.Class);
	}

	Widget->SetWidgetSpace(EWidgetSpace::World);
	Widget->SetDrawSize(FVector2D(1000.0f, 1000.0f));
	Widget->bVisible = true;

	// 頭コリジョンの大きさを設定してコリジョン作用条件を設定します。
	HeadBox->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	HeadBox->SetCollisionProfileName(TEXT("OverlapAll"));
	HeadBox->bGenerateOverlapEvents = true;
	
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	// 残りの変数を初期化します。
	MaxHp = 100.0f;
	CurrentHp = MaxHp;
	MaxStamina = 100.0f;
	CurrentStamina = MaxStamina;

	AttackPoint = 5.0f;
	DefencePoint = 10.0f;
	DashPoint = 30.0f;
	RecoveryPoint = 1.0f;
	bIsUseStamina = false;

	InvincibleTimeOn = false;
	CurrentState = EPlayerState::Idle;
	bAllowBreathe = true;
	DashPower = 800.0f;
	GrabState = E_HandState::Open;

	HeadBox->ComponentTags.Add(FName(TEXT("DisregardForLeftHand")));
	HeadBox->ComponentTags.Add(FName(TEXT("DisregardForRightHand")));
	HeadBox->ComponentTags.Add(FName("Head"));
	Tags.Add(FName("Character"));
	Tags.Add(FName(TEXT("DisregardForLeftHand")));
	Tags.Add(FName(TEXT("DisregardForRightHand")));
}

void AMotionControllerCharacter::BeginPlay()
{
	Super::BeginPlay();

	//FName DeviceName = UHeadMountedDisplayFunctionLibrary::GetHMDDeviceName();

	//if (DeviceName == "SteamVR" || DeviceName == "OculusHMD")
	//{
	//	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Eye);
	//}

	//FActorSpawnParameters SpawnActorOption;
	//SpawnActorOption.Owner = this;
	//SpawnActorOption.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	//FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);

	//LeftHand = GetWorld()->SpawnActor<ALeftHandMotionController>(LeftHand->StaticClass(), GetMesh()->GetComponentLocation(), GetMesh()->GetComponentRotation(), SpawnActorOption);
	//if (LeftHand)
	//	LeftHand->AttachToComponent(GetMesh(), AttachRules);

	//RightHand = GetWorld()->SpawnActor<ARightHandMotionController>(RightHand->StaticClass(), GetMesh()->GetComponentLocation(), GetMesh()->GetComponentRotation(), SpawnActorOption);
	//if (RightHand)
	//	RightHand->AttachToComponent(GetMesh(), AttachRules);

	// 頭のオーバーラップイベント設定
	if (HeadBox)
		HeadBox->OnComponentBeginOverlap.AddDynamic(this, &AMotionControllerCharacter::OnHeadOverlap);

	// 自動的にスタミナを満たしてくれるようにします。
	GetWorld()->GetTimerManager().SetTimer(AutoTimerHandle, this, &AMotionControllerCharacter::AutoStamina, 0.05f, false);
}

void AMotionControllerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentHp > 100.0f)
	{
		CurrentHp = 100.0f;
	}
	if (CurrentStamina > 100.0f)
	{
		CurrentStamina = 100.0f;
	}

	// 犬が自分を発見した場合,配列にちょうど保存する大きさだけ割り当てします。
	if (DogArray.Num() > 0)
	{
		DogArray.Shrink();
	}

	// Sin 関数を通じて呼吸するようにしてくれます。
	if (SpringArm)
	{
		if (bAllowBreathe)
		{
			FVector newLocation = SpringArm->RelativeLocation;
			float loca = (FMath::Sin(RunningTime + DeltaTime) - FMath::Sin(RunningTime));
			newLocation.Z += loca * 10.f;
			RunningTime += (DeltaTime * 5);

			SpringArm->SetRelativeLocation(newLocation);
		}
	}

	// ウィジェットが描く情報をステレオのテクスチャーにセットします。
	if (IsValid(Widget->GetRenderTarget()))
	{
		UTexture* texture;
		texture = Cast<UTextureRenderTarget2D>(Widget->GetRenderTarget());
		Stereo->SetTexture(Widget->GetRenderTarget());
	}

	// 移動の時,息を停止します。
	if (CurrentState != EPlayerState::Idle && this->GetVelocity().Size() == 0)
	{
		CurrentState = EPlayerState::Idle;
		GetWorld()->GetTimerManager().SetTimer(SetIdleTimerHandle, this, &AMotionControllerCharacter::SetAllowBreathe, 0.01f, false, 1.0f);
	}
	else if (this->GetVelocity().Size() > 0)
	{
		bAllowBreathe = false;
	}		
}

// Called to bind functionality to input
void AMotionControllerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction(TEXT("GrabLeft"), IE_Pressed, this, &AMotionControllerCharacter::GrabLeftOn);
	PlayerInputComponent->BindAction(TEXT("GrabLeft"), IE_Released, this, &AMotionControllerCharacter::GrabLeftOff);

	PlayerInputComponent->BindAction(TEXT("GrabRight"), IE_Pressed, this, &AMotionControllerCharacter::GrabRightOn);
	PlayerInputComponent->BindAction(TEXT("GrabRight"), IE_Released, this, &AMotionControllerCharacter::GrabRightOff);

	PlayerInputComponent->BindAction(TEXT("Dash"), IE_Pressed, this, &AMotionControllerCharacter::DashOn);
	PlayerInputComponent->BindAction(TEXT("Dash"), IE_Released, this, &AMotionControllerCharacter::DashOff);

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AMotionControllerCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AMotionControllerCharacter::MoveRight);

	PlayerInputComponent->BindAction(TEXT("Run"), IE_Pressed, this, &AMotionControllerCharacter::RunOn);
	PlayerInputComponent->BindAction(TEXT("Run"), IE_Released, this, &AMotionControllerCharacter::RunOff);
	//  Test
	PlayerInputComponent->BindAction(TEXT("Menu"), IE_Released, this, &AMotionControllerCharacter::GameMenu);
}

// 左手のグラブ
void AMotionControllerCharacter::GrabLeftOn()
{
	LeftHand->interaction->PressPointerKey(EKeys::LeftMouseButton);
	GrabState = E_HandState::Grab;
	LeftHand->GrabActor();
	LeftHand->Shield->ConvertOfOpacity(0.14f);		// 방패의 투명도를 낮춥니다
}

// 左手のグラブ解除
void AMotionControllerCharacter::GrabLeftOff()
{
	LeftHand->interaction->ReleasePointerKey(EKeys::LeftMouseButton);
	GrabState = E_HandState::Open;
	LeftHand->ReleaseActor();
	LeftHand->Shield->ConvertOfOpacity(0.8f);		// 방패의 투명도를 높입니다
}

// 右手のグラブ
void AMotionControllerCharacter::GrabRightOn()
{
	RightHand->interaction->PressPointerKey(EKeys::LeftMouseButton);
	GrabState = E_HandState::Grab;
	RightHand->GrabActor();
	RightHand->Sword->ConvertOfOpacity(1);			// 검의 투명도를 낮춥니다
}

// 右手のグラブ解除
void AMotionControllerCharacter::GrabRightOff()
{
	RightHand->interaction->ReleasePointerKey(EKeys::LeftMouseButton);
	GrabState = E_HandState::Open;
	RightHand->ReleaseActor();
	RightHand->Sword->ConvertOfOpacity(0.5f);		// 검의 투명도를 높입니다
}

// プレーヤーの状態によって (歩き/ラン)
void AMotionControllerCharacter::MoveForward(float Value)
{
	if (Value != 0)
	{
		if(CurrentState == EPlayerState::Run)
			GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
		else
		{
			CurrentState = EPlayerState::Walk;
			GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		}
		AddMovementInput(Camera->GetForwardVector(), Value);
	}
}

// プレーヤーの状態によって (Walk/Run)
void AMotionControllerCharacter::MoveRight(float Value)
{
	if (Value != 0)
	{
		if (CurrentState == EPlayerState::Run)
			GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
		else
		{
			CurrentState = EPlayerState::Walk;
			GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		}
		AddMovementInput(Camera->GetRightVector(), Value);
	}
}

// トラックパッドを押すとRun状態 / トラックパッドにはAxis値がなくて押した時の状態を変更しました。
void AMotionControllerCharacter::RunOn()
{
	CurrentState = EPlayerState::Run;
}

// Runの解除
void AMotionControllerCharacter::RunOff()
{
	CurrentState = EPlayerState::Idle;
}

// ダッシュ / 特定の速度以上の時だけ実行します。
void AMotionControllerCharacter::DashOn()
{
	if (CurrentStamina > DashPoint)
	{
		if (GetVelocity().Size() >= 20.0f)
		{
			UseStamina(DashPoint);
			FVector DashVector = FVector::ZeroVector;
			GetCharacterMovement()->GroundFriction = 0;					// 土地との摩擦を無視します。
			DashVector = GetVelocity().GetSafeNormal() * 3000.0f;
			DashVector.Z = 0;
			LaunchCharacter(DashVector, false, false);
		}
	}
}

// ダッシュ解除
void AMotionControllerCharacter::DashOff()
{
	GetCharacterMovement()->GroundFriction = 8.0f;
}

// コントローラーのメニューを押すとキャラクターの正面にメニューウィンドウが生成されます。
void AMotionControllerCharacter::GameMenu()
{
	// メニュー生成
	if (!Menu)
	{
		FActorSpawnParameters SpawnActorOption;
		SpawnActorOption.Owner = this;
		SpawnActorOption.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// メニューをプレイヤーが見る位置と角度に生成します。
		FVector location = FVector(Camera->GetComponentLocation().X, Camera->GetComponentLocation().Y, GetActorLocation().Z);
		FRotator rotator = FRotator(Camera->GetComponentRotation().Pitch + 20.0f, Camera->GetComponentRotation().Yaw + 180.0f, 0.0f);
		FVector CameraForwardVectorzeroHeight = FVector(Camera->GetForwardVector().X, Camera->GetForwardVector().Y, 0.0f);
		
		Menu = GetWorld()->SpawnActor<AMenu>(Menu->StaticClass(), location + CameraForwardVectorzeroHeight.GetSafeNormal() * 70.0f,
			rotator, SpawnActorOption);
	}
	//	再び押すと,消えます。
	else
	{
		Menu->Destroy();
		Menu = nullptr;
	}
}

// タイマーに動いた後,少し後にまた呼吸することができるようにします。
void AMotionControllerCharacter::SetAllowBreathe()
{
	bAllowBreathe = true;			
}

// ダメージを受けます。
float AMotionControllerCharacter::TakeDamage(float Damage, FDamageEvent const & DamageEvent, AController * EventInstigator, AActor * DamageCauser)
{
	if (!InvincibleTimeOn)
	{
		bisHit = true;
		Widget->bVisible = true;

		if (Widget->bVisible)
		{
			UHitBloodyWidget* bloodyWidget = Cast<UHitBloodyWidget>(Widget->GetUserWidgetObject());
			if (bloodyWidget)
			{
				bloodyWidget->PlayAnimationByName("Bloody", 0.0, 1, EUMGSequencePlayMode::Forward, 1.0f);		// 赤い色で点滅するアニメを実行します。
			}
		}

		// 体力減少
		if (CurrentHp > 0.0f)
		{
			CurrentHp -= Damage;
			if (CurrentHp < 0.0f)
				CurrentHp = 0.0f;
		}

		LeftHand->Shield->StateBar->GetDamage(Damage);
		GLog->Log(FString::Printf(TEXT("데미지 받음")));
		InvincibleTimeOn = true;		// 持続的にダメージを受けないように無敵時間をくれます。
		GetWorld()->GetTimerManager().SetTimer(DamageTimerHandle, this, &AMotionControllerCharacter::DamageTimer, 0.01f, false, 1.5f);		// タイマーで無敵時間を非活性化します。
	}

	return Damage;
}

// 無敵時間非活性化
void AMotionControllerCharacter::DamageTimer()
{
	InvincibleTimeOn = false;			
}

// ウィジェットを見えないようにします。
void AMotionControllerCharacter::DisableBloody()
{
	if (Widget->bVisible)
		Widget->bVisible = false;		
}

void AMotionControllerCharacter::OnHeadOverlap(UPrimitiveComponent * OverlappedComp, AActor * OtherActor, UPrimitiveComponent * OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult)
{
	if (OtherActor->ActorHasTag("Potion") && GrabState == E_HandState::Grab)
	{
		CurrentHp += 30;
	}
}

// スタミナを使用します。
void AMotionControllerCharacter::UseStamina(float _stamina)
{
	CurrentStamina -= _stamina;
	bIsUseStamina = true;

	GetWorld()->GetTimerManager().ClearTimer(AutoTimerHandle);			// しばらくスタミナ回復を止まります。
	GetWorld()->GetTimerManager().SetTimer(AutoTimerHandle, this, &AMotionControllerCharacter::AutoStamina, 3.0f, false);		// 一定時間後スタミナがまた回復されます。
}

// スタミナを回復します。
void AMotionControllerCharacter::AutoStamina()
{
	if (CurrentStamina < MaxStamina)
	{
		CurrentStamina += RecoveryPoint;
		
		if (CurrentStamina > MaxStamina)
		{
			CurrentStamina = MaxStamina;
		}
		GetWorld()->GetTimerManager().SetTimer(AutoTimerHandle, this, &AMotionControllerCharacter::AutoStamina, 0.01f, false);
	}
}
