// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MatineeToLevelSequencePCH.h"
#include "ModuleInterface.h"

#define LOCTEXT_NAMESPACE "MatineeToLevelSequence"

DEFINE_LOG_CATEGORY(LogMatineeToLevelSequence);

/**
 * Implements the MatineeToLevelSequence module.
 */
class FMatineeToLevelSequenceModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		RegisterMenuExtensions();
	}
	
	virtual void ShutdownModule() override
	{
		UnregisterMenuExtensions();
	}

protected:

	/** Register menu extensions for the level editor toolbar. */
	void RegisterMenuExtensions()
	{
		// Register level editor menu extender
		LevelEditorMenuExtenderDelegate = FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateStatic(&FMatineeToLevelSequenceModule::ExtendLevelViewportContextMenu);
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		auto& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();
		MenuExtenders.Add(LevelEditorMenuExtenderDelegate);
		LevelEditorExtenderDelegateHandle = MenuExtenders.Last().GetHandle();
	}

protected:

	/** Unregisters menu extensions for the level editor toolbar. */
	void UnregisterMenuExtensions()
	{
		// Unregister level editor menu extender
		if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
			LevelEditorModule.GetAllLevelViewportContextMenuExtenders().RemoveAll([&](const FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors& Delegate) {
				return Delegate.GetHandle() == LevelEditorExtenderDelegateHandle;
			});
		}
	}

	static TSharedRef<FExtender> ExtendLevelViewportContextMenu(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> SelectedActors)
	{
		TSharedRef<FExtender> Extender(new FExtender());

		TArray<TWeakObjectPtr<AActor> > ActorsToConvert;
		for (AActor* Actor : SelectedActors)
		{
			if (Actor->IsA(AMatineeActor::StaticClass()))
			{
				ActorsToConvert.Add(Actor);
			}
		}

		if (ActorsToConvert.Num())
		{
			// Add the convert to level sequence asset sub-menu extender
			Extender->AddMenuExtension(
				"ActorSelectVisibilityLevels",
				EExtensionHook::After,
				nullptr,
				FMenuExtensionDelegate::CreateStatic(&FMatineeToLevelSequenceModule::CreateLevelViewportContextMenuEntries, ActorsToConvert));
		}

		return Extender;
	}

	static void CreateLevelViewportContextMenuEntries(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<AActor> > ActorsToConvert)
	{
		MenuBuilder.BeginSection("LevelSequence", LOCTEXT("LevelSequenceLevelEditorHeading", "Level Sequence"));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("MenuExtensionConvertMatineeToLevelSequence", "Convert to Level Sequence"),
			LOCTEXT("MenuExtensionConvertMatineeToLevelSequence_Tooltip", "Convert to Level Sequence"),
			FSlateIcon(),
			FExecuteAction::CreateStatic(&FMatineeToLevelSequenceModule::OnConvertMatineeToLevelSequence, ActorsToConvert),
			NAME_None,
			EUserInterfaceActionType::Button);

		MenuBuilder.EndSection();
	}

	/** Callback for converting a matinee to a level sequence asset. */
	static void OnConvertMatineeToLevelSequence(TArray<TWeakObjectPtr<AActor> > ActorsToConvert)
	{
		TArray<TWeakObjectPtr<ALevelSequenceActor> > NewActors;

		for (TWeakObjectPtr<AActor> Actor : ActorsToConvert)
		{
			TWeakObjectPtr<ALevelSequenceActor> NewActor = ConvertSingleMatineeToLevelSequence(Actor);
			if (NewActor.IsValid())
			{
				NewActors.Add(NewActor);
			}
		}

		// Select the newly created level sequence actors
		const bool bNotifySelectionChanged = true;
		const bool bDeselectBSP = true;
		const bool bWarnAboutTooManyActors = false;
		const bool bSelectEvenIfHidden = false;

		GEditor->GetSelectedActors()->Modify();
		GEditor->GetSelectedActors()->BeginBatchSelectOperation();

		GEditor->SelectNone( bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors );

		for (TWeakObjectPtr<AActor> NewActor : NewActors )
		{
			GEditor->SelectActor(NewActor.Get(), true, bNotifySelectionChanged, bSelectEvenIfHidden );
		}

		GEditor->GetSelectedActors()->EndBatchSelectOperation();
		GEditor->NoteSelectionChange();

		// Edit the first asset
		if (NewActors.Num())
		{
			UObject* NewAsset = NewActors[0]->LevelSequence.TryLoad();
			if (NewAsset)
			{
				FAssetEditorManager::Get().OpenEditorForAsset(NewAsset);
			}
		}
	}

	/** Find or add a folder for the given actor **/
	static void FindOrAddFolder(UMovieScene* MovieScene, TWeakObjectPtr<AActor> Actor, FGuid Guid)
	{
		FName FolderName(NAME_None);
		if(Actor.Get()->IsA<ACharacter>() || Actor.Get()->IsA<ASkeletalMeshActor>())
		{
			FolderName = TEXT("Characters");
		}
		else if(Actor.Get()->GetClass()->IsChildOf(ACameraActor::StaticClass()))
		{
			FolderName = TEXT("Cameras");
		}
		else if(Actor.Get()->GetClass()->IsChildOf(ALight::StaticClass()))
		{
			FolderName = TEXT("Lights");
		}
		else if (Actor.Get()->GetComponentsByClass(UParticleSystemComponent::StaticClass()).Num())
		{
			FolderName = TEXT("Particles");
		}
		else
		{
			FolderName = TEXT("Misc");
		}

		// look for a folder to put us in
		UMovieSceneFolder* FolderToUse = nullptr;
		for (UMovieSceneFolder* Folder : MovieScene->GetRootFolders())
		{
			if (Folder->GetFolderName() == FolderName)
			{
				FolderToUse = Folder;
				break;
			}
		}

		if (FolderToUse == nullptr)
		{
			FolderToUse = NewObject<UMovieSceneFolder>(MovieScene, NAME_None, RF_Transactional);
			FolderToUse->SetFolderName(FolderName);
			MovieScene->GetRootFolders().Add(FolderToUse);
		}

		FolderToUse->AddChildObjectBinding(Guid);
	}

	/** Add master track to a folder **/
	static void AddMasterTrackToFolder(UMovieScene* MovieScene, UMovieSceneTrack* MovieSceneTrack, FName FolderName)
	{
		UMovieSceneFolder* FolderToUse = nullptr;
		for (UMovieSceneFolder* Folder : MovieScene->GetRootFolders())
		{
			if (Folder->GetFolderName() == FolderName)
			{
				FolderToUse = Folder;
				break;
			}
		}

		if (FolderToUse == nullptr)
		{
			FolderToUse = NewObject<UMovieSceneFolder>(MovieScene, NAME_None, RF_Transactional);
			FolderToUse->SetFolderName(FolderName);
			MovieScene->GetRootFolders().Add(FolderToUse);
		}

		FolderToUse->AddChildMasterTrack(MovieSceneTrack);
	}

	/** Add property to possessable node **/
	template <typename T>
	static T* AddPropertyTrack(FName InPropertyName, AActor* InActor, const FGuid& PossessableGuid, UMovieSceneSequence* NewSequence, UMovieScene* NewMovieScene)
	{
		T* PropertyTrack = nullptr;

		// Find the property that matinee uses
		void* PropContainer = NULL;
		UProperty* Property = NULL;
		UObject* PropObject = FMatineeUtils::FindObjectAndPropOffset(PropContainer, Property, InActor, InPropertyName );

		FGuid ObjectGuid = PossessableGuid;
		if (PropObject && Property)
		{
			// If the property object that owns this property isn't already bound, add a binding to the property object
			ObjectGuid = NewSequence->FindPossessableObjectId(*PropObject);
			if (!ObjectGuid.IsValid())
			{
				ObjectGuid = NewMovieScene->AddPossessable(PropObject->GetName(), PropObject->GetClass());
				NewSequence->BindPossessableObject(ObjectGuid, *PropObject, GWorld);
			}

			FString PropertyName = Property->GetFName().ToString();
			FString PropertyPath = Property->GetPathName();

			// Special case for Light components which have some deprecated names
			if (PropObject->GetClass()->IsChildOf(ULightComponentBase::StaticClass()))
			{
				TMap<FName, FName> PropertyNameRemappings;
				PropertyNameRemappings.Add(TEXT("Brightness"), TEXT("Intensity"));
				PropertyNameRemappings.Add(TEXT("Radius"), TEXT("AttenuationRadius"));

				FName* RemappedName = PropertyNameRemappings.Find(*PropertyName);
				if (RemappedName != nullptr)
				{
					PropertyName = RemappedName->ToString();
					int32 LastDot = INDEX_NONE;
					PropertyPath.FindLastChar(TCHAR(':'), LastDot);
					if (LastDot != INDEX_NONE)
					{
						PropertyPath = PropertyPath.Left(LastDot) + TEXT(":") + PropertyName;
					}
				}
			}

			PropertyTrack = NewMovieScene->AddTrack<T>(ObjectGuid);	
			PropertyTrack->SetPropertyNameAndPath(*PropertyName, PropertyPath);
		}
		else
		{
			UE_LOG(LogMatineeToLevelSequence, Log, TEXT("Can't find property '%s' for '%s'."), *InPropertyName.ToString(), *InActor->GetActorLabel());
		}

		return PropertyTrack;
	}

	/** Convert an interp group */
	static void ConvertInterpGroup(UInterpGroup* Group, AActor* GroupActor, UMovieScene* NewMovieScene, UMovieSceneSequence* NewSequence)
	{
		FGuid PossessableGuid;

		// Bind the group actor as a possessable						
		if (GroupActor)
		{
			PossessableGuid = NewMovieScene->AddPossessable(GroupActor->GetActorLabel(), GroupActor->GetClass());
			NewSequence->BindPossessableObject(PossessableGuid, *GroupActor, GWorld);
	
			FindOrAddFolder(NewMovieScene, GroupActor, PossessableGuid);
		}

		for (int32 j=0; j<Group->InterpTracks.Num(); ++j)
		{
			UInterpTrack* Track = Group->InterpTracks[j];
			if (Track->IsDisabled())
			{
				continue;
			}

			// Handle each track class
			if (Track->IsA(UInterpTrackMove::StaticClass()))					
			{
				UInterpTrackMove* MatineeMoveTrack = StaticCast<UInterpTrackMove*>(Track);
				if (MatineeMoveTrack->GetNumKeyframes() != 0 && PossessableGuid.IsValid())
				{
					UMovieScene3DTransformTrack* TransformTrack = NewMovieScene->AddTrack<UMovieScene3DTransformTrack>(PossessableGuid);								
					FMatineeImportTools::CopyInterpMoveTrack(MatineeMoveTrack, TransformTrack);
				}
			}
			else if (Track->IsA(UInterpTrackAnimControl::StaticClass()))
			{
				UInterpTrackAnimControl* MatineeAnimControlTrack = StaticCast<UInterpTrackAnimControl*>(Track);
				if (MatineeAnimControlTrack->GetNumKeyframes() != 0 && PossessableGuid.IsValid())
				{
					UMovieSceneSkeletalAnimationTrack* SkeletalAnimationTrack = NewMovieScene->AddTrack<UMovieSceneSkeletalAnimationTrack>(PossessableGuid);	
					float EndPlaybackRange = NewMovieScene->GetPlaybackRange().GetUpperBoundValue();
					FMatineeImportTools::CopyInterpAnimControlTrack(MatineeAnimControlTrack, SkeletalAnimationTrack, EndPlaybackRange);
				}
			}
			else if (Track->IsA(UInterpTrackToggle::StaticClass()))
			{
				UInterpTrackToggle* MatineeParticleTrack = StaticCast<UInterpTrackToggle*>(Track);
				if (MatineeParticleTrack->GetNumKeyframes() != 0 && PossessableGuid.IsValid())
				{
					UMovieSceneParticleTrack* ParticleTrack = NewMovieScene->AddTrack<UMovieSceneParticleTrack>(PossessableGuid);	
					FMatineeImportTools::CopyInterpParticleTrack(MatineeParticleTrack, ParticleTrack);
				}
			}
			else if (Track->IsA(UInterpTrackEvent::StaticClass()))
			{
				UInterpTrackEvent* MatineeEventTrack = StaticCast<UInterpTrackEvent*>(Track);
				if (MatineeEventTrack->GetNumKeyframes() != 0)
				{
					UMovieSceneEventTrack* EventTrack = Cast<UMovieSceneEventTrack>(NewMovieScene->AddMasterTrack<UMovieSceneEventTrack>());
					FString EventTrackName = Group->GroupName.ToString() + TEXT("Events");
					EventTrack->SetDisplayName(FText::FromString(EventTrackName));
					FMatineeImportTools::CopyInterpEventTrack(MatineeEventTrack, EventTrack);

					static FName EventsFolder("Events");
					AddMasterTrackToFolder(NewMovieScene, EventTrack, EventsFolder);
				}
			}
			else if (Track->IsA(UInterpTrackSound::StaticClass()))
			{
				UInterpTrackSound* MatineeSoundTrack = StaticCast<UInterpTrackSound*>(Track);
				if (MatineeSoundTrack->GetNumKeyframes() != 0)
				{
					UMovieSceneAudioTrack* AudioTrack = Cast<UMovieSceneAudioTrack>(NewMovieScene->AddMasterTrack<UMovieSceneAudioTrack>());
					FString AudioTrackName = Group->GroupName.ToString() + TEXT("Audio");
					AudioTrack->SetDisplayName(FText::FromString(AudioTrackName));					
					FMatineeImportTools::CopyInterpSoundTrack(MatineeSoundTrack, AudioTrack);

					static FName AudioFolder("Audio");
					AddMasterTrackToFolder(NewMovieScene, AudioTrack, AudioFolder);
				}
			}
			else if (Track->IsA(UInterpTrackBoolProp::StaticClass()))
			{
				UInterpTrackBoolProp* MatineeBoolTrack = StaticCast<UInterpTrackBoolProp*>(Track);
				if (MatineeBoolTrack->GetNumKeyframes() != 0 && GroupActor && PossessableGuid.IsValid())
				{
					UMovieSceneBoolTrack* BoolTrack = AddPropertyTrack<UMovieSceneBoolTrack>(MatineeBoolTrack->PropertyName, GroupActor, PossessableGuid, NewSequence, NewMovieScene);
					if (BoolTrack)
					{
						FMatineeImportTools::CopyInterpBoolTrack(MatineeBoolTrack, BoolTrack);
					}
				}
			}
			else if (Track->IsA(UInterpTrackFloatProp::StaticClass()))
			{
				UInterpTrackFloatProp* MatineeFloatTrack = StaticCast<UInterpTrackFloatProp*>(Track);
				if (MatineeFloatTrack->GetNumKeyframes() != 0 && GroupActor && PossessableGuid.IsValid())
				{
					UMovieSceneFloatTrack* FloatTrack = AddPropertyTrack<UMovieSceneFloatTrack>(MatineeFloatTrack->PropertyName, GroupActor, PossessableGuid, NewSequence, NewMovieScene);
					if (FloatTrack)
					{
						FMatineeImportTools::CopyInterpFloatTrack(MatineeFloatTrack, FloatTrack);
					}
				}
			}
			else if (Track->IsA(UInterpTrackColorProp::StaticClass()))
			{
				UInterpTrackColorProp* MatineeColorTrack = StaticCast<UInterpTrackColorProp*>(Track);
				if (MatineeColorTrack->GetNumKeyframes() != 0 && GroupActor && PossessableGuid.IsValid())
				{
					UMovieSceneColorTrack* ColorTrack = AddPropertyTrack<UMovieSceneColorTrack>(MatineeColorTrack->PropertyName, GroupActor, PossessableGuid, NewSequence, NewMovieScene);
					if (ColorTrack)
					{
						FMatineeImportTools::CopyInterpColorTrack(MatineeColorTrack, ColorTrack);
					}
				}
			}
			else if (Track->IsA(UInterpTrackVisibility::StaticClass()))
			{
				UInterpTrackVisibility* MatineeVisibilityTrack = StaticCast<UInterpTrackVisibility*>(Track);
				if (MatineeVisibilityTrack->GetNumKeyframes() != 0 && GroupActor && PossessableGuid.IsValid())
				{
					UMovieSceneVisibilityTrack* VisibilityTrack = NewMovieScene->AddTrack<UMovieSceneVisibilityTrack>(PossessableGuid);	
					if (VisibilityTrack)
					{
						VisibilityTrack->SetPropertyNameAndPath(TEXT("bHidden"), GroupActor->GetPathName() + TEXT(".bHidden"));

						FMatineeImportTools::CopyInterpVisibilityTrack(MatineeVisibilityTrack, VisibilityTrack);
					}
				}
			}
			else if (Track->IsA(UInterpTrackDirector::StaticClass()))
			{
				// Intentionally left blank - The director track is converted in a separate pass below.
			}
			else
			{
				if (GroupActor)
				{
					UE_LOG(LogMatineeToLevelSequence, Log, TEXT("Unsupported track '%s' for '%s'."), *Track->TrackTitle, *GroupActor->GetActorLabel());
				}
				else
				{
					UE_LOG(LogMatineeToLevelSequence, Log, TEXT("Unsupported track '%s'."), *Track->TrackTitle);
				}
			}
		}
	}

	/** Convert a single matinee to a level sequence asset */
	static TWeakObjectPtr<ALevelSequenceActor> ConvertSingleMatineeToLevelSequence(TWeakObjectPtr<AActor> ActorToConvert)
	{
		UObject* AssetOuter = ActorToConvert->GetOuter();
		UPackage* AssetPackage = AssetOuter->GetOutermost();

		FString NewLevelSequenceAssetName = ActorToConvert->GetActorLabel() + FString("LevelSequence");
		FString NewLevelSequenceAssetPath = AssetPackage->GetName();
		int LastSlashPos = NewLevelSequenceAssetPath.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		NewLevelSequenceAssetPath = NewLevelSequenceAssetPath.Left(LastSlashPos);

		// Create a new level sequence asset with the appropriate name
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		UObject* NewAsset = nullptr;
		for (TObjectIterator<UClass> It ; It ; ++It)
		{
			UClass* CurrentClass = *It;
			if (CurrentClass->IsChildOf(UFactory::StaticClass()) && !(CurrentClass->HasAnyClassFlags(CLASS_Abstract)))
			{
				UFactory* Factory = Cast<UFactory>(CurrentClass->GetDefaultObject());
				if (Factory->CanCreateNew() && Factory->ImportPriority >= 0 && Factory->SupportedClass == ULevelSequence::StaticClass())
				{
					NewAsset = AssetTools.CreateAssetWithDialog(NewLevelSequenceAssetName, NewLevelSequenceAssetPath, ULevelSequence::StaticClass(), Factory);
					break;
				}
			}
		}

		if (!NewAsset)
		{
			return nullptr;
		}

		UMovieSceneSequence* NewSequence = Cast<UMovieSceneSequence>(NewAsset);
		UMovieScene* NewMovieScene = NewSequence->GetMovieScene();

		// Add a level sequence actor for this new sequence
		UActorFactory* ActorFactory = GEditor->FindActorFactoryForActorClass(ALevelSequenceActor::StaticClass());
		if (!ensure(ActorFactory))
		{
			return nullptr;
		}

		ALevelSequenceActor* NewActor = CastChecked<ALevelSequenceActor>(GEditor->UseActorFactory(ActorFactory, FAssetData(NewAsset), &FTransform::Identity));

		// Walk through all the interp group data and create corresponding tracks on the new level sequence asset
		if (ActorToConvert->IsA(AMatineeActor::StaticClass()))
		{
			AMatineeActor* MatineeActor = StaticCast<AMatineeActor*>(ActorToConvert.Get());
			MatineeActor->InitInterp();

			// Set the length
			NewMovieScene->SetPlaybackRange(0.0f, MatineeActor->MatineeData->InterpLength);

			// Convert the groups
			for (int32 i=0; i<MatineeActor->GroupInst.Num(); ++i)
			{
				UInterpGroupInst* GrInst = MatineeActor->GroupInst[i];
				UInterpGroup* Group = GrInst->Group;
				AActor* GroupActor = GrInst->GetGroupActor();
				ConvertInterpGroup(Group, GroupActor, NewMovieScene, NewSequence);
			}

			// Director group - convert this after the regular groups to ensure that the camera cut bindings are there
			UInterpGroupDirector* DirGroup = MatineeActor->MatineeData->FindDirectorGroup();
			if (DirGroup)
			{
				UInterpTrackDirector* MatineeDirectorTrack = DirGroup->GetDirectorTrack();
				if (MatineeDirectorTrack && MatineeDirectorTrack->GetNumKeyframes() != 0)
				{
					UMovieSceneCameraCutTrack* CameraCutTrack = Cast<UMovieSceneCameraCutTrack>(NewMovieScene->AddMasterTrack<UMovieSceneCameraCutTrack>());
					FMatineeImportTools::CopyInterpDirectorTrack(MatineeDirectorTrack, CameraCutTrack, MatineeActor, NewSequence);
				}

				UInterpTrackFade* MatineeFadeTrack = DirGroup->GetFadeTrack();
				if (MatineeFadeTrack && MatineeFadeTrack->GetNumKeyframes() != 0)
				{						
					UMovieSceneFadeTrack* FadeTrack = Cast<UMovieSceneFadeTrack>(NewMovieScene->AddMasterTrack<UMovieSceneFadeTrack>());
					FMatineeImportTools::CopyInterpFadeTrack(MatineeFadeTrack, FadeTrack);
				}

				UInterpTrackSlomo* MatineeSlomoTrack = DirGroup->GetSlomoTrack();
				if (MatineeSlomoTrack && MatineeSlomoTrack->GetNumKeyframes() != 0)
				{
					UMovieSceneSlomoTrack* SlomoTrack = Cast<UMovieSceneSlomoTrack>(NewMovieScene->AddMasterTrack<UMovieSceneSlomoTrack>());
					FMatineeImportTools::CopyInterpFloatTrack(MatineeSlomoTrack, SlomoTrack);
				}
				
				//@todo - color scale, audio master
			}

			MatineeActor->TermInterp();
		}
		return NewActor;
	}

private:

	FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors LevelEditorMenuExtenderDelegate;

	FDelegateHandle LevelEditorExtenderDelegateHandle;
};

IMPLEMENT_MODULE(FMatineeToLevelSequenceModule, MatineeToLevelSequence);

#undef LOCTEXT_NAMESPACE
