﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "MSNiagaraRepresentationProcessors.h"
#include "MassCommonFragments.h"
#include "MassSignalSubsystem.h"
#include "Common/Fragments/MSFragments.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "Representation//MSNiagaraActor.h"
#include "Representation/Fragments/MSRepresentationFragments.h"

UMSNiagaraRepresentationProcessors::UMSNiagaraRepresentationProcessors()
{
	//We don't care about rendering on the dedicated server!
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	//join the other representation processors in their existing group
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Representation;
	// but EndPhysics as that seems more reasonable to deal with real rendering state?
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
}

void UMSNiagaraRepresentationProcessors::ConfigureQueries()
{
	PositionToNiagaraFragmentQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PositionToNiagaraFragmentQuery.AddSharedRequirement<FSharedNiagaraSystemFragment>(EMassFragmentAccess::ReadWrite);
	PositionToNiagaraFragmentQuery.RegisterWithProcessor(*this);

}

// todo-performance separate setup for rarely moving pieces?
void UMSNiagaraRepresentationProcessors::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
		// Query mass for transform data
		PositionToNiagaraFragmentQuery.ForEachEntityChunk(EntityManager,Context,
			[&,this](FMassExecutionContext& Context)
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_MASS_PositionToNiagara);
				const int32 QueryLength = Context.GetNumEntities();
				
				const auto& Transforms = Context.GetFragmentView<FTransformFragment>().GetData();
				auto& SharedNiagaraFragment = Context.GetMutableSharedFragment<FSharedNiagaraSystemFragment>();
				
				int32 ArrayResizeAmount = SharedNiagaraFragment.IteratorOffset + QueryLength;
				
				SharedNiagaraFragment.IteratorOffset += QueryLength;
				SharedNiagaraFragment.ParticlePositions.SetNumUninitialized(ArrayResizeAmount);
				SharedNiagaraFragment.ParticleOrientations.SetNumUninitialized(ArrayResizeAmount);

				FVector* PositionsDataArray = SharedNiagaraFragment.ParticlePositions.GetData();
				FQuat4f* DirectionsDataArray = SharedNiagaraFragment.ParticleOrientations.GetData();

				
				for (int32 i = 0; i < QueryLength; ++i)
				{
					 // this is needed because there are multiple chunks for each shared niagara system 
					 const int32 ArrayPosition = i + SharedNiagaraFragment.IteratorOffset - QueryLength;
					auto& Transform = Transforms[i].GetTransform(); 
					
					
					 PositionsDataArray[ArrayPosition] = Transform.GetTranslation();
					 DirectionsDataArray[ArrayPosition] = (FQuat4f)Transform.GetRotation();
					 // temp log to double check iteration order etc
					 // UE_LOG( LogTemp, Error, TEXT("projectile manager niagara system %s iterated on! with chunk length %i "),*NiagaraActor->GetName(),QueryLength);
				}
			});

	// with our nice new data, we push to the actual niagara components in the world!
	EntityManager.ForEachSharedFragment<FSharedNiagaraSystemFragment>([](FSharedNiagaraSystemFragment& SharedNiagaraFragment)
	{
		const AMSNiagaraActor* NiagaraActor =  SharedNiagaraFragment.NiagaraManagerActor.Get();

		// UE_LOG( LogTemp, Error, TEXT("Niagara array length for %s is %i"),*NiagaraActor->GetName(),SharedNiagaraFragment.NiagaraManagerActor->ParticlePositions.Num());
		if(UNiagaraComponent* NiagaraComponent = NiagaraActor->GetNiagaraComponent())
		{
			// If the offset is 0, we didn't iterate anything and should reset our arrays (could do an evil setnumunsafe?)
			if(SharedNiagaraFragment.IteratorOffset == 0)
			{
				SharedNiagaraFragment.ParticlePositions.Reset();
				SharedNiagaraFragment.ParticleOrientations.Reset();
			}
			// congratulations to me (karl) for making SetNiagaraArrayVector public in an engine PR (he's so cool) (wow)
			UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(NiagaraComponent,SharedNiagaraFragment.ParticlePositionsName,SharedNiagaraFragment.ParticlePositions);
			UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayQuat(NiagaraComponent,SharedNiagaraFragment.ParticleOrientationsParameterName,SharedNiagaraFragment.ParticleOrientations);
		}
		else
		{
			UE_LOG( LogTemp, Error, TEXT("projectile manager %s was invalid during array push!"),*NiagaraActor->GetName());
		}

		// Let's prepare the shared fragments to accept new data next frame!
		SharedNiagaraFragment.IteratorOffset = 0;
	});

	
}


UMSNiagaraRepresentationSpawnProcs::UMSNiagaraRepresentationSpawnProcs()
{
	//We don't care about rendering on the dedicated server!
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	//join the other representation processors in their existing group
	//ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Representation;
	ExecutionOrder.ExecuteAfter.Add(TEXT("MSNiagaraRepresentationProcessors"));
}

void UMSNiagaraRepresentationSpawnProcs::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSharedRequirement<FSharedNiagaraSystemSpawnFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.RegisterWithProcessor(*this);

}

void UMSNiagaraRepresentationSpawnProcs::SignalEntities(FMassEntityManager& EntityManager,
	FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	//query mass for transform data
	EntityQuery.ForEachEntityChunk(EntityManager,Context,
		[&,this](FMassExecutionContext& Context)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_MASS_SpawnPositionToNiagara);
			
			const auto& Transforms = Context.GetFragmentView<FTransformFragment>().GetData();
			auto& SharedNiagaraFragment = Context.GetSharedFragment<FSharedNiagaraSystemSpawnFragment>();
			
			check(SharedNiagaraFragment.NiagaraManagerActor.Get()->GetNiagaraComponent())


			UNiagaraComponent* NiagaraComponent = SharedNiagaraFragment.NiagaraManagerActor.Get()->GetNiagaraComponent();

			// There is a world where this signal execution is done multiple times 
			TArray<FVector> LocationsArray;
			TArray<FVector> NormalsArray;
			for (int32 i = 0; i < Context.GetNumEntities(); ++i)
			{
				LocationsArray.Add(Transforms[i].GetTransform().GetLocation());
				NormalsArray.Add(-Transforms[i].GetTransform().GetRotation().GetForwardVector());

				 UE_LOG( LogTemp, Error, TEXT("projectile manager niagara system %s iterated on! with chunk length "),*NiagaraComponent->GetName());
			}

			UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(NiagaraComponent,"HitPositions",LocationsArray);
			UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(NiagaraComponent,"HitNormals",NormalsArray);
			//NiagaraComponent->SetNiagaraVariableInt("System.NumToSpawnThisFrame",Context.GetNumEntities());
			

		});
}

void UMSNiagaraRepresentationSpawnProcs::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);
	UMassSignalSubsystem* SignalSubsystem = GetWorld()->GetSubsystem<UMassSignalSubsystem>();

	SubscribeToSignal(*SignalSubsystem, MassSample::Signals::OnProjectileHitSomething);
}