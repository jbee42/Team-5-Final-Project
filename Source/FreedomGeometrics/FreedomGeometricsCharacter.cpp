// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FreedomGeometrics.h"
#include "Kismet/HeadMountedDisplayFunctionLibrary.h"
#include "FreedomGeometricsCharacter.h"

//////////////////////////////////////////////////////////////////////////

AFreedomGeometricsCharacter::AFreedomGeometricsCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
    
    // Get PlayerMesh & PlayerLight
    HealthMesh = CreateDefaultSubobject<UStaticMeshComponent>(FName("PlayerMesh"));
    PlayerLight = CreateDefaultSubobject<UPointLightComponent>(FName("PointLight"));

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = false; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 0.f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;


	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

												// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)

	CurrentLocation = GetActorLocation();
}

//////////////////////////////////////////////////////////////////////////
// Input

void AFreedomGeometricsCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AFreedomGeometricsCharacter::BeginFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &AFreedomGeometricsCharacter::EndFire);

	PlayerInputComponent->BindAxis("MoveForward", this, &AFreedomGeometricsCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AFreedomGeometricsCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AFreedomGeometricsCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AFreedomGeometricsCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AFreedomGeometricsCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AFreedomGeometricsCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AFreedomGeometricsCharacter::OnResetVR);
}


void AFreedomGeometricsCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AFreedomGeometricsCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	Jump();
}

void AFreedomGeometricsCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
	StopJumping();
}

void AFreedomGeometricsCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AFreedomGeometricsCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AFreedomGeometricsCharacter::MoveForward(float Value)
{
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AFreedomGeometricsCharacter::MoveRight(float Value)
{
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

void AFreedomGeometricsCharacter::BeginFire()
{
	IsFiring = true;
}

void AFreedomGeometricsCharacter::EndFire()
{
	IsFiring = false;
}

void AFreedomGeometricsCharacter::Fire()
{
	TimeSinceLastShotFired = 0;

	ReadyToSpawnProjectile = true;
}

FVector AFreedomGeometricsCharacter::GetAimDirection()
{
	AimDirection = FollowCamera->GetForwardVector();
	AimDirection.Z = 0;
	AimDirection.Normalize();

	return AimDirection;
}

bool AFreedomGeometricsCharacter::IsProjectileReady()
{
	return ReadyToSpawnProjectile;
}

void AFreedomGeometricsCharacter::SetReadyToSpawnProjectile(bool b)
{
	ReadyToSpawnProjectile = b;
}

void AFreedomGeometricsCharacter::UpdateSpinSpeed()
{	
	if (IsFiring) CurrentSpinSpeed += SpinAcceleration;
	else CurrentSpinSpeed -= SpinAcceleration;

	if (CurrentSpinSpeed > MaxSpinSpeed) CurrentSpinSpeed = MaxSpinSpeed;
	if (CurrentSpinSpeed < MinSpinSpeed) CurrentSpinSpeed = MinSpinSpeed;
}

void AFreedomGeometricsCharacter::UpdateDeltaLocation()
{
	PreviousLocation = CurrentLocation;
	CurrentLocation = GetActorLocation();
	//
}

float AFreedomGeometricsCharacter::GetCurrentSpinSpeed()
{
	return CurrentSpinSpeed;
}

FVector AFreedomGeometricsCharacter::GetDeltaLocation()
{
	//
	FVector DeltaLoc = CurrentLocation - PreviousLocation;
	return DeltaLoc;
}

void AFreedomGeometricsCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeSinceLastShotFired += DeltaTime;

	if (IsFiring && TimeSinceLastShotFired >= FireDelaySeconds)
	{
		Fire();
	}

	UpdateSpinSpeed();
	UpdateDeltaLocation();
}

/** Health */
float AFreedomGeometricsCharacter::getHealth()
{
    return Health;
}

float AFreedomGeometricsCharacter::getMaxHealth()
{
	return MaxHealth;
}

void AFreedomGeometricsCharacter::addHealth(float val)
{
	Health += val;

	if (Health > MaxHealth) Health = MaxHealth;
	if (Health < 0) Health = 0;

    UMaterialInstanceDynamic* HealthMaterial = HealthMesh->CreateAndSetMaterialInstanceDynamic(0);
    
    float healthRatio = Health / MaxHealth;
    
    if (healthRatio > 1) healthRatio = 1;
    else if (healthRatio < 0) healthRatio = 0;
    
    // Make health color
    FLinearColor HealthColor;
    HealthColor.R = (1 - healthRatio);
    HealthColor.G = healthRatio;
    HealthColor.B = 0.0f;
    HealthColor.A = 1;
    
    if (HealthMaterial) {
        HealthMaterial->SetVectorParameterValue(FName("ColorParam"), HealthColor);
        HealthMaterial->SetScalarParameterValue(FName("Metallic"), 0.5f);
        HealthMaterial->SetScalarParameterValue(FName("Specular"), 0);
        HealthMaterial->SetScalarParameterValue(FName("Roughness"), 0);
    }

    if (PlayerLight) {
        PlayerLight->SetLightColor(HealthColor, 0);
    }
}

void AFreedomGeometricsCharacter::isHit(float damageValue)
{
	addHealth(-damageValue);
}
