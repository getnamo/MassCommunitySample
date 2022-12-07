﻿// Fill out your copyright notice in the Description page of Project Settings.

#include "MSHashGridProcessor.h"

#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "Common/Fragments/MSHashGridFragments.h"


UMSHashGridProcessor::UMSHashGridProcessor()
{
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Movement);
	// In theory it should be thread save but I'm seeing wackiness so I'm slapping on singlethreaded for now
	bRequiresGameThreadExecution = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
}

void UMSHashGridProcessor::Initialize(UObject& Owner)
{
	MassSampleSystem = GetWorld()->GetSubsystem<UMSSubsystem>();
}

void UMSHashGridProcessor::ConfigureQueries()
{
	UpdateHashGridQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	UpdateHashGridQuery.AddRequirement<FMSGridCellStartingLocationFragment>(EMassFragmentAccess::ReadWrite);
	UpdateHashGridQuery.AddTagRequirement<FMSInHashGridTag>(EMassFragmentPresence::All);
	UpdateHashGridQuery.RegisterWithProcessor(*this);

}

void UMSHashGridProcessor::Execute(FMassEntityManager& EntitySubsystem, FMassExecutionContext& Context)
{
	UpdateHashGridQuery.ForEachEntityChunk(EntitySubsystem, Context, [this](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();

		const auto LocationList = Context.GetFragmentView<FTransformFragment>();
		const auto NavigationObstacleCellLocationList = Context.GetMutableFragmentView<FMSGridCellStartingLocationFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			const FVector& Location = LocationList[i].GetTransform().GetLocation();
			MassSampleSystem->HashGrid.UpdatePoint(Context.GetEntity(i),NavigationObstacleCellLocationList[i].Location,Location);

			NavigationObstacleCellLocationList[i].Location = Location;
		}
	});
}

UMSHashGridMemberInitializationProcessor::UMSHashGridMemberInitializationProcessor()
{
	ObservedType = FMSGridCellStartingLocationFragment::StaticStruct();
	bRequiresGameThreadExecution = true;
	Operation = EMassObservedOperation::Add;
}

void UMSHashGridMemberInitializationProcessor::Initialize(UObject& Owner)
{
	MassSampleSystem = GetWorld()->GetSubsystem<UMSSubsystem>();
}

void UMSHashGridMemberInitializationProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMSGridCellStartingLocationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.RegisterWithProcessor(*this);
	
}

void UMSHashGridMemberInitializationProcessor::Execute(FMassEntityManager& EntitySubsystem, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(EntitySubsystem, Context, [&](FMassExecutionContext& Context)
	{
		const auto LocationList = Context.GetFragmentView<FTransformFragment>();
		const auto NavigationObstacleCellLocationList = Context.GetMutableFragmentView<FMSGridCellStartingLocationFragment>();

		const int32 NumEntities = Context.GetNumEntities();
		
		for (int32 i = 0; i < NumEntities; ++i)
		{
			const FVector& Location = LocationList[i].GetTransform().GetLocation();

			MassSampleSystem->HashGrid.InsertPoint(Context.GetEntity(i),Location);

			NavigationObstacleCellLocationList[i].Location = Location;
			
			Context.Defer().AddTag<FMSInHashGridTag>(Context.GetEntity(i));
		}
	});
}
