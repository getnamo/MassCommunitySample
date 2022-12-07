﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "MassProcessor.h"
#include "MSSubsystem.h"
#include "MSHashGridProcessor.generated.h"

/**
 * We reimplement a hashgrid because the one built in is too attached to crowd avoidance.
 * If we used the built in one, avoiding enabled crowdmembers avoid everything on the hashgrid!
 * Feels dirty to have two but oh well. Perhaps it should be 3d?
 * 
 */
UCLASS()
class MASSCOMMUNITYSAMPLE_API UMSHashGridProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	
	UMSHashGridProcessor();

protected:
	
	FMassEntityQuery AddToHashGridQuery;
	FMassEntityQuery UpdateHashGridQuery;
	FMassEntityQuery RemoveFromGridEntityQuery;
	
	UPROPERTY()
	UMSSubsystem* MassSampleSystem;

	virtual void Initialize(UObject& Owner) override;
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntitySubsystem, FMassExecutionContext& Context) override;
};

UCLASS()
class MASSCOMMUNITYSAMPLE_API UMSHashGridMemberInitializationProcessor : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	
	UMSHashGridMemberInitializationProcessor();

protected:

	FMassEntityQuery EntityQuery;
	UMSSubsystem* MassSampleSystem;
	
	virtual void Initialize(UObject& Owner) override;
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntitySubsystem, FMassExecutionContext& Context) override;

};
