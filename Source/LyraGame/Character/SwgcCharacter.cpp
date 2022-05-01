#include "SwgcCharacter.h"

#include "AlsCameraComponent.h"
#include "AlsCharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "LyraLogChannels.h"
#include "EnhancedInputSubsystems.h"
#include "LyraGameplayTags.h"
#include "LyraHealthComponent.h"
#include "LyraPawnExtensionComponent.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Player/LyraPlayerController.h"
#include "Player/LyraPlayerState.h"
#include "System/LyraSignificanceManager.h"

static FName NAME_LyraCharacterCollisionProfile_Capsule(TEXT("LyraPawnCapsule"));
static FName NAME_LyraCharacterCollisionProfile_Mesh(TEXT("LyraPawnMesh"));

ASwgcCharacter::ASwgcCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UAlsCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	NetCullDistanceSquared = 900000000.0f;

	// Setup Capsule
	UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
	check(CapsuleComp);
	CapsuleComp->InitCapsuleSize(40.0f, 90.0f);
	CapsuleComp->SetCollisionProfileName(NAME_LyraCharacterCollisionProfile_Capsule);

	// Setup Mesh
	USkeletalMeshComponent* MeshComp = GetMesh();
	check(MeshComp);
	MeshComp->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));  // Rotate mesh to be X forward since it is exported as Y forward.
	MeshComp->SetCollisionProfileName(NAME_LyraCharacterCollisionProfile_Mesh);

	PawnExtComponent = CreateDefaultSubobject<ULyraPawnExtensionComponent>(TEXT("PawnExtensionComponent"));
	PawnExtComponent->OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemInitialized));
	PawnExtComponent->OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemUninitialized));
	
	AlsCamera = CreateDefaultSubobject<UAlsCameraComponent>(TEXT("AlsCamera"));
	AlsCamera->SetupAttachment(GetMesh());
	AlsCamera->SetRelativeRotation_Direct({0.0f, 90.0f, 0.0f});
}

void ASwgcCharacter::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

void ASwgcCharacter::BeginPlay()
{
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);

	Super::BeginPlay();

	UWorld* World = GetWorld();

	const bool bRegisterWithSignificanceManager = !IsNetMode(NM_DedicatedServer);
	if (bRegisterWithSignificanceManager)
	{
		if (ULyraSignificanceManager* SignificanceManager = USignificanceManager::Get<ULyraSignificanceManager>(World))
		{
			//@TODO: SignificanceManager->RegisterObject(this, (EFortSignificanceType)SignificanceType);
		}
	}
}

void ASwgcCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);

	Super::EndPlay(EndPlayReason);
	
	UWorld* World = GetWorld();

	const bool bRegisterWithSignificanceManager = !IsNetMode(NM_DedicatedServer);
	if (bRegisterWithSignificanceManager)
	{
		if (ULyraSignificanceManager* SignificanceManager = USignificanceManager::Get<ULyraSignificanceManager>(World))
		{
			SignificanceManager->UnregisterObject(this);
		}
	}
}

void ASwgcCharacter::Reset()
{
	DisableMovementAndCollision();

	K2_OnReset();

	UninitAndDestroy();
}


void ASwgcCharacter::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, MyTeamID);
}


void ASwgcCharacter::NotifyControllerChanged()
{
	const FGenericTeamId OldTeamId = GetGenericTeamId();

	Super::NotifyControllerChanged();

	// Update our team ID based on the controller
	if (HasAuthority() && (Controller != nullptr))
	{
		if (ILyraTeamAgentInterface* ControllerWithTeam = Cast<ILyraTeamAgentInterface>(Controller))
		{
			MyTeamID = ControllerWithTeam->GetGenericTeamId();
			ConditionalBroadcastTeamChanged(this, OldTeamId, MyTeamID);
		}
	}
}

ALyraPlayerController* ASwgcCharacter::GetLyraPlayerController() const
{
	return CastChecked<ALyraPlayerController>(Controller, ECastCheckedType::NullAllowed);
}

ALyraPlayerState* ASwgcCharacter::GetLyraPlayerState() const
{
	return CastChecked<ALyraPlayerState>(GetPlayerState(), ECastCheckedType::NullAllowed);
}

ULyraAbilitySystemComponent* ASwgcCharacter::GetLyraAbilitySystemComponent() const
{
	return Cast<ULyraAbilitySystemComponent>(GetAbilitySystemComponent());
}

UAbilitySystemComponent* ASwgcCharacter::GetAbilitySystemComponent() const
{
	return PawnExtComponent->GetLyraAbilitySystemComponent();
}

void ASwgcCharacter::OnAbilitySystemInitialized()
{
	ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent();
	check(LyraASC);

	HealthComponent->InitializeWithAbilitySystem(LyraASC);

	InitializeGameplayTags();
}

void ASwgcCharacter::OnAbilitySystemUninitialized()
{
	HealthComponent->UninitializeFromAbilitySystem();
}


void ASwgcCharacter::PossessedBy(AController* NewController)
{
	const FGenericTeamId OldTeamID = MyTeamID;

	Super::PossessedBy(NewController);

	PawnExtComponent->HandleControllerChanged();

	// Grab the current team ID and listen for future changes
	if (ILyraTeamAgentInterface* ControllerAsTeamProvider = Cast<ILyraTeamAgentInterface>(NewController))
	{
		MyTeamID = ControllerAsTeamProvider->GetGenericTeamId();
		ControllerAsTeamProvider->GetTeamChangedDelegateChecked().AddDynamic(this, &ThisClass::OnControllerChangedTeam);
	}
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}

void ASwgcCharacter::UnPossessed()
{
	AController* const OldController = Controller;

	// Stop listening for changes from the old controller
	const FGenericTeamId OldTeamID = MyTeamID;
	if (ILyraTeamAgentInterface* ControllerAsTeamProvider = Cast<ILyraTeamAgentInterface>(OldController))
	{
		ControllerAsTeamProvider->GetTeamChangedDelegateChecked().RemoveAll(this);
	}

	Super::UnPossessed();

	PawnExtComponent->HandleControllerChanged();

	// Determine what the new team ID should be afterwards
	MyTeamID = DetermineNewTeamAfterPossessionEnds(OldTeamID);
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}

void ASwgcCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	PawnExtComponent->HandleControllerChanged();
}

void ASwgcCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	PawnExtComponent->HandlePlayerStateReplicated();
}

void ASwgcCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PawnExtComponent->SetupPlayerInputComponent();
}


void ASwgcCharacter::InitializeGameplayTags()
{
	// Clear tags that may be lingering on the ability system from the previous pawn.
	if (ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		const FLyraGameplayTags& GameplayTags = FLyraGameplayTags::Get();

		for (const TPair<uint8, FGameplayTag>& TagMapping : GameplayTags.MovementModeTagMap)
		{
			if (TagMapping.Value.IsValid())
			{
				LyraASC->SetLooseGameplayTagCount(TagMapping.Value, 0);
			}
		}

		for (const TPair<uint8, FGameplayTag>& TagMapping : GameplayTags.CustomMovementModeTagMap)
		{
			if (TagMapping.Value.IsValid())
			{
				LyraASC->SetLooseGameplayTagCount(TagMapping.Value, 0);
			}
		}

		UAlsCharacterMovementComponent* AlsMoveComp = CastChecked<UAlsCharacterMovementComponent>(GetCharacterMovement());
		SetMovementModeTag(AlsMoveComp->MovementMode, AlsMoveComp->CustomMovementMode, true);
	}
}

void ASwgcCharacter::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (const ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		LyraASC->GetOwnedGameplayTags(TagContainer);
	}
}

bool ASwgcCharacter::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	if (const ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		return LyraASC->HasMatchingGameplayTag(TagToCheck);
	}

	return false;
}

bool ASwgcCharacter::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		return LyraASC->HasAllMatchingGameplayTags(TagContainer);
	}

	return false;
}

bool ASwgcCharacter::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		return LyraASC->HasAnyMatchingGameplayTags(TagContainer);
	}

	return false;
}

void ASwgcCharacter::FellOutOfWorld(const class UDamageType& dmgType)
{
	HealthComponent->DamageSelfDestruct(/*bFellOutOfWorld=*/ true);
}

void ASwgcCharacter::OnDeathStarted(AActor*)
{
	DisableMovementAndCollision();
}

void ASwgcCharacter::OnDeathFinished(AActor*)
{
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::DestroyDueToDeath);
}


void ASwgcCharacter::DisableMovementAndCollision()
{
	if (Controller)
	{
		Controller->SetIgnoreMoveInput(true);
	}

	UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
	check(CapsuleComp);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapsuleComp->SetCollisionResponseToAllChannels(ECR_Ignore);

	UAlsCharacterMovementComponent* LyraMoveComp = CastChecked<UAlsCharacterMovementComponent>(GetCharacterMovement());
	LyraMoveComp->StopMovementImmediately();
	LyraMoveComp->DisableMovement();
}

void ASwgcCharacter::DestroyDueToDeath()
{
	K2_OnDeathFinished();

	UninitAndDestroy();
}


void ASwgcCharacter::UninitAndDestroy()
{
	if (GetLocalRole() == ROLE_Authority)
	{
		DetachFromControllerPendingDestroy();
		SetLifeSpan(0.1f);
	}

	// Uninitialize the ASC if we're still the avatar actor (otherwise another pawn already did it when they became the avatar actor)
	if (ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		if (LyraASC->GetAvatarActor() == this)
		{
			PawnExtComponent->UninitializeAbilitySystem();
		}
	}

	SetActorHiddenInGame(true);
}

void ASwgcCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	const UAlsCharacterMovementComponent* AlsMoveComp = CastChecked<UAlsCharacterMovementComponent>(GetCharacterMovement());

	SetMovementModeTag(PrevMovementMode, PreviousCustomMode, false);
	SetMovementModeTag(AlsMoveComp->MovementMode, AlsMoveComp->CustomMovementMode, true);
}

void ASwgcCharacter::SetMovementModeTag(EMovementMode MovementMode, uint8 CustomMovementMode, bool bTagEnabled)
{
	if (ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		const FLyraGameplayTags& GameplayTags = FLyraGameplayTags::Get();
		const FGameplayTag* MovementModeTag = nullptr;

		if (MovementMode == MOVE_Custom)
		{
			MovementModeTag = GameplayTags.CustomMovementModeTagMap.Find(CustomMovementMode);
		}
		else
		{
			MovementModeTag = GameplayTags.MovementModeTagMap.Find(MovementMode);
		}

		if (MovementModeTag && MovementModeTag->IsValid())
		{
			LyraASC->SetLooseGameplayTagCount(*MovementModeTag, (bTagEnabled ? 1 : 0));
		}
	}
}

void ASwgcCharacter::ToggleCrouch()
{
	const UAlsCharacterMovementComponent* AlsMoveComp = CastChecked<UAlsCharacterMovementComponent>(GetCharacterMovement());

	if (bIsCrouched || AlsMoveComp->bWantsToCrouch)
	{
		UnCrouch();
	}
	else if (AlsMoveComp->IsMovingOnGround())
	{
		Crouch();
	}
}

void ASwgcCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	if (ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		LyraASC->SetLooseGameplayTagCount(FLyraGameplayTags::Get().Status_Crouching, 1);
	}


	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

void ASwgcCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	if (ULyraAbilitySystemComponent* LyraASC = GetLyraAbilitySystemComponent())
	{
		LyraASC->SetLooseGameplayTagCount(FLyraGameplayTags::Get().Status_Crouching, 0);
	}

	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

bool ASwgcCharacter::CanJumpInternal_Implementation() const
{
	// same as ACharacter's implementation but without the crouch check
	return JumpIsAllowedInternal();
}

void ASwgcCharacter::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	if (GetController() == nullptr)
	{
		if (HasAuthority())
		{
			const FGenericTeamId OldTeamID = MyTeamID;
			MyTeamID = NewTeamID;
			ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
		}
		else
		{
			UE_LOG(LogLyraTeams, Error, TEXT("You can't set the team ID on a character (%s) except on the authority"), *GetPathNameSafe(this));
		}
	}
	else
	{
		UE_LOG(LogLyraTeams, Error, TEXT("You can't set the team ID on a possessed character (%s); it's driven by the associated controller"), *GetPathNameSafe(this));
	}
}

FGenericTeamId ASwgcCharacter::GetGenericTeamId() const
{
	return MyTeamID;
}

FOnLyraTeamIndexChangedDelegate* ASwgcCharacter::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
}

void ASwgcCharacter::OnControllerChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam)
{
	const FGenericTeamId MyOldTeamID = MyTeamID;
	MyTeamID = IntegerToGenericTeamId(NewTeam);
	ConditionalBroadcastTeamChanged(this, MyOldTeamID, MyTeamID);
}

void ASwgcCharacter::OnRep_MyTeamID(FGenericTeamId OldTeamID)
{
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}

void ASwgcCharacter::CalcCamera(const float DeltaTime, FMinimalViewInfo& ViewInfo)
{
	if (AlsCamera->IsActive())
	{
		AlsCamera->GetViewInfo(ViewInfo);
		return;
	}

	Super::CalcCamera(DeltaTime, ViewInfo);
}

void ASwgcCharacter::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& Unused, float& VerticalLocation)
{
	if (AlsCamera->IsActive())
	{
		AlsCamera->DisplayDebug(Canvas, DebugDisplay, VerticalLocation);
	}

	Super::DisplayDebug(Canvas, DebugDisplay, Unused, VerticalLocation);
}






