// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "AISpawner.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Interop/SpatialWorkerFlags.h"

// Sets default values
AAISpawner::AAISpawner()
{
	// This actor only ticks in the authoritative server
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// An entity is needed so that the correct server gains authority
	bReplicates = true;

	bSpawningEnabled = false;

	NumSpawned = 0;
	SecondsSinceLastSpawn = 0.f;
	SecondsSinceLastUpdateParameters = 0.f;
}

// Called when the game starts or when spawned
void AAISpawner::BeginPlay()
{
	Super::BeginPlay();

	NumSpawned = 0;
	SecondsSinceLastSpawn = 0.f;
	SecondsSinceLastUpdateParameters = 0.f;
}

void AAISpawner::OnAuthorityGained()
{
	SpawnInitial();
	SetActorTickEnabled(true);

	// force parameters update so that spawning starts with correct data
	if (UpdateParametersIntervalSeconds > 0)
	{
		UpdateParameters();
	}

	Super::OnAuthorityGained();
}

void AAISpawner::SpawnInitial()
{
	for (TActorIterator<APlayerStart> Itr(GetWorld()); Itr; ++Itr)
	{
		SpawnPoints.Add(Itr->GetActorTransform());
	}

	bCanSpawn = SpawnPoints.Num() > 0;
	bSpawningEnabled = true;
}

void AAISpawner::UpdateParameters()
{
	SecondsSinceLastUpdateParameters = 0;

	FString FlagValue;

	USpatialWorkerFlags::GetWorkerFlag("ai_spawning_enabled", FlagValue);
	bSpawningEnabled = FlagValue.ToBool();

	USpatialWorkerFlags::GetWorkerFlag("ai_min_seconds_between_spawns", FlagValue);
	MinSecondsBetweenSpawns = FCString::Atof(*FlagValue);

	USpatialWorkerFlags::GetWorkerFlag("ai_num_to_spawn", FlagValue);
	NumAIToSpawn = FCString::Atoi(*FlagValue);
}

void AAISpawner::SpawnActor()
{
	SecondsSinceLastSpawn = 0;

	FActorSpawnParameters SpawnParams;
	int SpawnPointIndex = FMath::RandRange(0, SpawnPoints.Num() - 1);

	ACharacter* SpawnedActor = GetWorld()->SpawnActor<ACharacter>(AICharacterTemplate, SpawnPoints[SpawnPointIndex], SpawnParams);
	AICharacterHandles.Push(SpawnedActor);

	NumSpawned++;
}

void AAISpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	SecondsSinceLastSpawn += DeltaTime;
	SecondsSinceLastUpdateParameters += DeltaTime;

	if (UpdateParametersIntervalSeconds > 0 && SecondsSinceLastUpdateParameters > UpdateParametersIntervalSeconds)
	{
		UpdateParameters();
	}

	if (NumSpawned == NumAIToSpawn)
	{
		return;
	}

	// batch clean up
	while (NumSpawned > NumAIToSpawn && AICharacterHandles.Num() > 0)
	{
		if (ACharacter* ActorToRemove = AICharacterHandles.Pop())
		{
			GetWorld()->DestroyActor(ActorToRemove);
			NumSpawned--;
		}
	}
	
	// interval based spawning
	if (bCanSpawn && bSpawningEnabled && NumSpawned < NumAIToSpawn && SecondsSinceLastSpawn >= MinSecondsBetweenSpawns)
	{
		SpawnActor();
	}
}

