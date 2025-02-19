// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Character.h"
#include "Guard.generated.h"

UCLASS()
class FPSHORROR_API AGuard : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AGuard();

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick( float DeltaSeconds ) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* InputComponent) override;

	UPROPERTY(EditAnywhere)
		float Health;

	UPROPERTY(EditAnywhere) //line trace range for the player's attack
		float range = 250;

	UPROPERTY(EditAnywhere)
		float Damage = 10.0f;

	

public:

	UFUNCTION(BlueprintCallable, Category = "Guard")
		virtual void OnAttack();

	UFUNCTION()
		void OnHit(class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
};
