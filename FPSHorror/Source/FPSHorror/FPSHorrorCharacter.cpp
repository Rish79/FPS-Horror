// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "FPSHorror.h"
#include "Engine.h"
#include "FPSHorrorCharacter.h"
#include "FPSHorrorProjectile.h"
#include "Animation/AnimInstance.h"
#include "DrawDebugHelpers.h"
#include "Guard.h"
#include "GameFramework/InputSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// AFPSHorrorCharacter

AFPSHorrorCharacter::AFPSHorrorCharacter()
{
	//test
	Health = MaxHealth;
	DecayingRate = 1.5f;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->AttachParent = GetCapsuleComponent();
	FirstPersonCameraComponent->RelativeLocation = FVector(0, 0, 64.f); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;


	// Scene Color Settings
	FirstPersonCameraComponent->PostProcessSettings.bOverride_SceneColorTint = true;
	FirstPersonCameraComponent->PostProcessSettings.bOverride_VignetteIntensity = true;
	FirstPersonCameraComponent->PostProcessSettings.bOverride_GrainIntensity = true;
	FirstPersonCameraComponent->PostProcessSettings.bOverride_ColorContrast = true;
	FirstPersonCameraComponent->PostProcessSettings.bOverride_ColorGamma = true;
	FirstPersonCameraComponent->PostProcessSettings.bOverride_ColorGain = true;

	FirstPersonCameraComponent->PostProcessSettings.SceneColorTint = FLinearColor(.5f, .5f, .5f, 1.0f);
	FirstPersonCameraComponent->PostProcessSettings.VignetteIntensity = 0.0f;
	FirstPersonCameraComponent->PostProcessSettings.GrainIntensity = .15f;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->AttachParent = FirstPersonCameraComponent;
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;

	// Create a Sowrd mesh component
	FP_Sword = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_Sword"));
	FP_Sword->SetOnlyOwnerSee(true);			// only the owning player will see this mesh
	FP_Sword->bCastDynamicShadow = false;
	FP_Sword->CastShadow = false;
	FP_Sword->AttachTo(Mesh1P, TEXT("Sword"), EAttachLocation::SnapToTargetIncludingScale, true);

	// Create a Hammer mesh component
	FP_Hammer = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_Hammer"));
	FP_Hammer->SetOnlyOwnerSee(true);			// only the owning player will see this mesh
	FP_Hammer->bCastDynamicShadow = false;
	FP_Hammer->CastShadow = false;
	FP_Hammer->AttachTo(Mesh1P, TEXT("Hammer"), EAttachLocation::SnapToTargetIncludingScale, true);

	// Default offset from the character location for projectiles to spawn
	GunOffset = FVector(100.0f, 30.0f, 10.0f);

	// Note: The ProjectileClass and the skeletal mesh/anim blueprints for Mesh1P are set in the
	// derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}

//////////////////////////////////////////////////////////////////////////
// Input

void AFPSHorrorCharacter::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
	// set up gameplay key bindings
	check(InputComponent);

	InputComponent->BindAction("BloodBall", IE_Pressed, this, &AFPSHorrorCharacter::ShootBloodBall);
	InputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	InputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	//InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AFPSHorrorCharacter::TouchStarted);
	if (EnableTouchscreenMovement(InputComponent) == false)
	{
		//InputComponent->BindAction("Fire", IE_Pressed, this, &AFPSHorrorCharacter::OnFire);
	}

	InputComponent->BindAxis("MoveForward", this, &AFPSHorrorCharacter::MoveForward);
	InputComponent->BindAxis("MoveRight", this, &AFPSHorrorCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	InputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	InputComponent->BindAxis("TurnRate", this, &AFPSHorrorCharacter::TurnAtRate);
	InputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	InputComponent->BindAxis("LookUpRate", this, &AFPSHorrorCharacter::LookUpAtRate);
}

#pragma optimize("",off)
void AFPSHorrorCharacter::OnFire()
{

	// LINE TRACE STUFF

	FCollisionQueryParams Traceparam;
	FCollisionObjectQueryParams CollisionObjectParams;

	FVector Start = FirstPersonCameraComponent->GetComponentLocation();
	//FVector Start = FirstPersonCameraComponent

	FVector End = Start + FirstPersonCameraComponent->GetForwardVector() * range;
	FHitResult HitData(ForceInit);

	//ignore collision with player
	AFPSHorrorCharacter* myCharacter = Cast<AFPSHorrorCharacter>(this);
	Traceparam.AddIgnoredActor(myCharacter);

	GetWorld()->LineTraceSingle(HitData, Start, End, Traceparam, CollisionObjectParams);


	//Check the target's hit by the line trace
	if (HitData.GetActor() != NULL)
	{
		AGuard* newGuard = Cast<AGuard>(HitData.GetActor());
		if (newGuard)
		{
			newGuard->Health -= Damage;
			if (newGuard->Health <= 0)//increase the power meter if the guard is dead
			{
				CurrentPower += 10;
				IncreaseCurrentHealth();
				CheckPower();
			}
		}
	}
}
#pragma optimize("",on)
void AFPSHorrorCharacter::BeginTouch(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (TouchItem.bIsPressed == true)
	{
		return;
	}
	TouchItem.bIsPressed = true;
	TouchItem.FingerIndex = FingerIndex;
	TouchItem.Location = Location;
	TouchItem.bMoved = false;
}

void AFPSHorrorCharacter::EndTouch(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	if (TouchItem.bIsPressed == false)
	{
		return;
	}
	if ((FingerIndex == TouchItem.FingerIndex) && (TouchItem.bMoved == false))
	{
		OnFire();
	}
	TouchItem.bIsPressed = false;
}

void AFPSHorrorCharacter::TouchUpdate(const ETouchIndex::Type FingerIndex, const FVector Location)
{

	if ((TouchItem.bIsPressed == true) && (TouchItem.FingerIndex == FingerIndex))
	{
		if (TouchItem.bIsPressed)
		{
			if (GetWorld() != nullptr)
			{
				UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport();
				if (ViewportClient != nullptr)
				{
					FVector MoveDelta = Location - TouchItem.Location;
					FVector2D ScreenSize;
					ViewportClient->GetViewportSize(ScreenSize);
					FVector2D ScaledDelta = FVector2D(MoveDelta.X, MoveDelta.Y) / ScreenSize;
					if (ScaledDelta.X != 0.0f)
					{
						TouchItem.bMoved = true;
						float Value = ScaledDelta.X * BaseTurnRate;
						AddControllerYawInput(Value);
					}
					if (ScaledDelta.Y != 0.0f)
					{
						TouchItem.bMoved = true;
						float Value = ScaledDelta.Y* BaseTurnRate;
						AddControllerPitchInput(Value);
					}
					TouchItem.Location = Location;
				}
				TouchItem.Location = Location;
			}
		}
	}
}

void AFPSHorrorCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AFPSHorrorCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void AFPSHorrorCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AFPSHorrorCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

bool AFPSHorrorCharacter::EnableTouchscreenMovement(class UInputComponent* InputComponent)
{
	bool bResult = false;
	if (FPlatformMisc::GetUseVirtualJoysticks() || GetDefault<UInputSettings>()->bUseMouseForTouch)
	{
		bResult = true;
		InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AFPSHorrorCharacter::BeginTouch);
		InputComponent->BindTouch(EInputEvent::IE_Released, this, &AFPSHorrorCharacter::EndTouch);
		InputComponent->BindTouch(EInputEvent::IE_Repeat, this, &AFPSHorrorCharacter::TouchUpdate);
	}
	return bResult;
}


void AFPSHorrorCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);


	HealthDecay(DeltaTime);
	GEngine->AddOnScreenDebugMessage(1, 2, FColor::Red, FString::FromInt(Health));

	// Screen getting bloodier as blood level gets smaller

	FirstPersonCameraComponent->PostProcessSettings.ColorContrast.Set(1.0f + (1.0f * ((MaxHealth - Health) / 100.0f)), 1.0f + (1.0f * ((MaxHealth - Health) / 100.0f)), 1.0f + (1.0f * ((MaxHealth - Health) / 100.0f)));
	FirstPersonCameraComponent->PostProcessSettings.SceneColorTint = FLinearColor(.5f + (.5f * ((MaxHealth - Health) / 100.0f)), .5f - (.3f * ((MaxHealth - Health) / 100.0f)), .5f - (.3f * ((MaxHealth - Health) / 100.0f)));
	FirstPersonCameraComponent->PostProcessSettings.ColorGamma.Set(1.0f, 1.0f, 1.0f - (.2f * ((MaxHealth - Health) / 100.0f)));
	FirstPersonCameraComponent->PostProcessSettings.ColorGain.Set(1.0f - (.25f * ((MaxHealth - Health) / 100.0f)), 1.0f - (.25f * ((MaxHealth - Health) / 100.0f)), 1.0f - (.25f * ((MaxHealth - Health) / 100.0f)));

	if (Health <= 20)
	{
		FirstPersonCameraComponent->PostProcessSettings.VignetteIntensity = (20.0f - Health) / 10.0f;
	}
}

void AFPSHorrorCharacter::HealthDecay(float DeltaTime)
{
	if (Health <= 0)
	{
		const FString Message = "You Died";
		GEngine->AddOnScreenDebugMessage(1, 2, FColor::Red, Message);
		LoadMainMenu();
		return;
	}

	Health -= DeltaTime * DecayingRate;
}

int8 AFPSHorrorCharacter::GetPeerRatio()
{
	return CurrentPeers / MaxPeers;
}

void AFPSHorrorCharacter::LoadMainMenu()
{
	UGameplayStatics::OpenLevel(this, FName(TEXT("MainMenu")));
}

void AFPSHorrorCharacter::CheckPower()
{
	const int8 upgradeDenom = UpgradeDivision + UpgradeAddition;
	CharacterMovement->MaxWalkSpeed = (CharacterMovement->MaxWalkSpeed + (CharacterMovement->MaxWalkSpeed / upgradeDenom));
	Damage +=  (Damage * 1.5 / upgradeDenom);

	GEngine->AddOnScreenDebugMessage(2, 2, FColor::Blue, FString::FromInt(CurrentPower));
	GEngine->AddOnScreenDebugMessage(3, 2, FColor::Green, FString::FromInt(CharacterMovement->MaxWalkSpeed));
	GEngine->AddOnScreenDebugMessage(4, 2, FColor::Yellow, FString::FromInt(Damage));

}

void AFPSHorrorCharacter::IncreaseCurrentHealth()
{
	if (Health <= 100)
	{
		Health += 15;
		if (Health > 100)
		{
			Health = 100;
		}
	}
}

void AFPSHorrorCharacter::ShootBloodBall()
{
	Health -= 10;
	if (Health <= 0)
	{
		LoadMainMenu();
	}
	if (ProjectileClass != NULL)
	{
		const FRotator SpawnRotation = GetControlRotation();
		// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
		//const FVector SpawnLocation = GetActorLocation() + SpawnRotation.RotateVector(GunOffset);

		const FVector SpawnLocation = GetActorLocation();

		UWorld* const World = GetWorld();
		if (World != NULL)
		{
			// spawn the projectile at the muzzle
			World->SpawnActor<AFPSHorrorProjectile>(ProjectileClass, SpawnLocation, SpawnRotation);
		}
	}
}