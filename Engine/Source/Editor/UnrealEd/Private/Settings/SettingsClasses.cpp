// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "ISourceControlModule.h"
#include "Components/BillboardComponent.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Components/ArrowComponent.h"
#include "AutoReimport/AutoReimportUtilities.h"

/* UContentBrowserSettings interface
 *****************************************************************************/

UContentBrowserSettings::FSettingChangedEvent UContentBrowserSettings::SettingChangedEvent;

UContentBrowserSettings::UContentBrowserSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


void UContentBrowserSettings::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr)
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (!FUnrealEdMisc::Get().IsDeletePreferences())
	{
		SaveConfig();
	}

	SettingChangedEvent.Broadcast(Name);
}


/* UDestructableMeshEditorSettings interface
 *****************************************************************************/

UDestructableMeshEditorSettings::UDestructableMeshEditorSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	AnimPreviewLightingDirection = FRotator(-45.0f, 45.0f, 0);
	AnimPreviewSkyColor = FColor::Blue;
	AnimPreviewFloorColor = FColor(51, 51, 51);
	AnimPreviewSkyBrightness = 0.2f * PI;
	AnimPreviewDirectionalColor = FColor::White;
	AnimPreviewLightBrightness = 1.0f * PI;
}


/* UEditorExperimentalSettings interface
 *****************************************************************************/

UEditorExperimentalSettings::UEditorExperimentalSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, bUnifiedBlueprintEditor(true)
	, bBlueprintableComponents(true)

{
}

void UEditorExperimentalSettings::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent )
{
	static const FName NAME_EQS = GET_MEMBER_NAME_CHECKED(UEditorExperimentalSettings, bEQSEditor);

	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (Name == FName(TEXT("ConsoleForGamepadLabels")))
	{
		EKeys::SetConsoleForGamepadLabels(ConsoleForGamepadLabels);
	}
	else if (Name == NAME_EQS)
	{
		if (bEQSEditor)
		{
			FModuleManager::Get().LoadModule(TEXT("EnvironmentQueryEditor"));
		}
	}

	if (!FUnrealEdMisc::Get().IsDeletePreferences())
	{
		SaveConfig();
	}

	SettingChangedEvent.Broadcast(Name);
}


/* UEditorLoadingSavingSettings interface
 *****************************************************************************/

UEditorLoadingSavingSettings::UEditorLoadingSavingSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, bMonitorContentDirectories(false)
	, bAutoCreateAssets(true)
	, bAutoDeleteAssets(true)
	, bDetectChangesOnRestart(false)
	, bDeleteSourceFilesWithAssets(false)
{
	TextDiffToolPath.FilePath = TEXT("P4Merge.exe");

	FAutoReimportDirectoryConfig Default;
	Default.SourceDirectory = TEXT("/Game/");
	AutoReimportDirectorySettings.Add(Default);
}


// @todo thomass: proper settings support for source control module
void UEditorLoadingSavingSettings::SccHackInitialize()
{
	bSCCUseGlobalSettings = ISourceControlModule::Get().GetUseGlobalSettings();
}

void UEditorLoadingSavingSettings::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Use MemberProperty here so we report the correct member name for nested changes
	const FName Name = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if (Name == FName(TEXT("bSCCUseGlobalSettings")))
	{
		// unfortunately we cant use UserSettingChangedEvent here as the source control module cannot depend on the editor
		ISourceControlModule::Get().SetUseGlobalSettings(bSCCUseGlobalSettings);
	}

	if (!FUnrealEdMisc::Get().IsDeletePreferences())
	{
		SaveConfig();
	}

	SettingChangedEvent.Broadcast(Name);
}

void UEditorLoadingSavingSettings::PostInitProperties()
{
	if (AutoReimportDirectories_DEPRECATED.Num() != 0)
	{
		AutoReimportDirectorySettings.Empty();
		for (const auto& String : AutoReimportDirectories_DEPRECATED)
		{
			FAutoReimportDirectoryConfig Config;
			Config.SourceDirectory = String;
			AutoReimportDirectorySettings.Add(Config);
		}
	}
	Super::PostInitProperties();
}

FAutoReimportDirectoryConfig::FParseContext::FParseContext(bool bInEnableLogging)
	: bEnableLogging(bInEnableLogging)
{
	TArray<FString> RootContentPaths;
	FPackageName::QueryRootContentPaths( RootContentPaths );
	for (FString& RootPath : RootContentPaths)
	{
		FString ContentFolder = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(RootPath));
		MountedPaths.Add( TPairInitializer<FString, FString>(MoveTemp(ContentFolder), MoveTemp(RootPath)) );
	}
}

bool FAutoReimportDirectoryConfig::ParseSourceDirectoryAndMountPoint(FString& SourceDirectory, FString& MountPoint, const FParseContext& InContext)
{
	SourceDirectory.ReplaceInline(TEXT("\\"), TEXT("/"));

	// Check if the source directory is actually a mount point
	if (!FPackageName::GetPackageMountPoint(SourceDirectory).IsNone())
	{
		MountPoint = SourceDirectory;
		SourceDirectory = FString();
	}

	if (!SourceDirectory.IsEmpty() && !MountPoint.IsEmpty())
	{
		// We have both a source directory and a mount point. Verify that the source dir exists, and that the mount point is valid.
		if (!IFileManager::Get().DirectoryExists(*SourceDirectory))
		{
			UE_CLOG(InContext.bEnableLogging, LogAutoReimportManager, Warning, TEXT("Unable to watch directory %s as it doesn't exist."), *SourceDirectory);
			return false;
		}

		if (FPackageName::GetPackageMountPoint(MountPoint).IsNone())
		{
			UE_CLOG(InContext.bEnableLogging, LogAutoReimportManager, Warning, TEXT("Unable to setup directory %s to map to %s, as it's not a valid mounted path. Continuing without mounted path (auto reimports will still work, but auto add won't)."), *SourceDirectory, *MountPoint);
		}
	}
	else if(!MountPoint.IsEmpty())
	{
		// We have just a mount point - validate it, and find its source directory
		if (FPackageName::GetPackageMountPoint(MountPoint).IsNone())
		{
			UE_CLOG(InContext.bEnableLogging, LogAutoReimportManager, Warning, TEXT("Unable to setup directory monitor for %s, as it's not a valid mounted path."), *MountPoint);
			return false;
		}

		SourceDirectory = FPackageName::LongPackageNameToFilename(MountPoint);
	}
	else if(!SourceDirectory.IsEmpty())
	{
		// We have just a source directory - verify whether it's a mounted path, and set up the mount point if so
		if (!IFileManager::Get().DirectoryExists(*SourceDirectory))
		{
			UE_CLOG(InContext.bEnableLogging, LogAutoReimportManager, Warning, TEXT("Unable to watch directory %s as it doesn't exist."), *SourceDirectory);
			return false;
		}

		// Set the mounted path if necessary
		auto* Pair = InContext.MountedPaths.FindByPredicate([&](const TPair<FString, FString>& Pair){
			return SourceDirectory.StartsWith(Pair.Key);
		});

		if (Pair)
		{
			MountPoint = Pair->Value / SourceDirectory.RightChop(Pair->Key.Len());
			MountPoint.ReplaceInline(TEXT("\\"), TEXT("/"));
		}
	}
	else
	{
		// Don't have any valid settings
		return false;
	}

	return true;
}

/* UEditorMiscSettings interface
 *****************************************************************************/

UEditorMiscSettings::UEditorMiscSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


/* ULevelEditorMiscSettings interface
 *****************************************************************************/

ULevelEditorMiscSettings::ULevelEditorMiscSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	bAutoApplyLightingEnable = true;
}


void ULevelEditorMiscSettings::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr)
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (Name == FName(TEXT("bNavigationAutoUpdate")))
	{
		FWorldContext &EditorContext = GEditor->GetEditorWorldContext();
		UNavigationSystem::SetNavigationAutoUpdateEnabled(bNavigationAutoUpdate, EditorContext.World()->GetNavigationSystem());
	}

	if (!FUnrealEdMisc::Get().IsDeletePreferences())
	{
		SaveConfig();
	}
}


/* ULevelEditorPlaySettings interface
 *****************************************************************************/

ULevelEditorPlaySettings::ULevelEditorPlaySettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	ClientWindowWidth = 640;
	ClientWindowHeight = 480;
	PlayNumberOfClients = 1;
	PlayNetDedicated = false;
	RunUnderOneProcess = true;
	RouteGamepadToSecondWindow = false;
}


/* ULevelEditorViewportSettings interface
 *****************************************************************************/

ULevelEditorViewportSettings::ULevelEditorViewportSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	bLevelStreamingVolumePrevis = false;
	BillboardScale = 1.0f;
	TransformWidgetSizeAdjustment = 0.0f;
	MeasuringToolUnits = MeasureUnits_Centimeters;

	// Set a default preview mesh
	PreviewMeshes.Add(FStringAssetReference("/Engine/EditorMeshes/ColorCalibrator/SM_ColorCalibrator.SM_ColorCalibrator"));
}

void ULevelEditorViewportSettings::PostInitProperties()
{
	Super::PostInitProperties();
	UBillboardComponent::SetEditorScale(BillboardScale);
	UArrowComponent::SetEditorScale(BillboardScale);
}

void ULevelEditorViewportSettings::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr)
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (Name == FName(TEXT("bAllowTranslateRotateZWidget")))
	{
		if (bAllowTranslateRotateZWidget)
		{
			GLevelEditorModeTools().SetWidgetMode(FWidget::WM_TranslateRotateZ);
		}
		else if (GLevelEditorModeTools().GetWidgetMode() == FWidget::WM_TranslateRotateZ)
		{
			GLevelEditorModeTools().SetWidgetMode(FWidget::WM_Translate);
		}
	}
	else if (Name == FName(TEXT("bHighlightWithBrackets")))
	{
		GEngine->SetSelectedMaterialColor(bHighlightWithBrackets
			? FLinearColor::Black
			: GetDefault<UEditorStyleSettings>()->SelectionColor);
	}
	else if (Name == FName(TEXT("HoverHighlightIntensity")))
	{
		GEngine->HoverHighlightIntensity = HoverHighlightIntensity;
	}
	else if (Name == FName(TEXT("SelectionHighlightIntensity")))
	{
		GEngine->SelectionHighlightIntensity = SelectionHighlightIntensity;
	}
	else if (Name == FName(TEXT("BSPSelectionHighlightIntensity")))
	{
		GEngine->BSPSelectionHighlightIntensity = BSPSelectionHighlightIntensity;
	}
	else if ((Name == FName(TEXT("UserDefinedPosGridSizes"))) || (Name == FName(TEXT("UserDefinedRotGridSizes"))) || (Name == FName(TEXT("ScalingGridSizes"))) || (Name == FName(TEXT("GridIntervals"))))
	{
		const float MinGridSize = (Name == FName(TEXT("GridIntervals"))) ? 4.0f : 0.0001f;
		TArray<float>* ArrayRef = nullptr;
		int32* IndexRef = nullptr;

		if (Name == FName(TEXT("ScalingGridSizes")))
		{
			ArrayRef = &(ScalingGridSizes);
			IndexRef = &(CurrentScalingGridSize);
		}

		// Don't allow an empty array of grid sizes
		if (ArrayRef->Num() == 0)
		{
			ArrayRef->Add(MinGridSize);
		}

		// Don't allow negative numbers
		for (int32 SizeIdx = 0; SizeIdx < ArrayRef->Num(); ++SizeIdx)
		{
			if ((*ArrayRef)[SizeIdx] < MinGridSize)
			{
				(*ArrayRef)[SizeIdx] = MinGridSize;
			}
		}
	}
	else if (Name == FName(TEXT("bUsePowerOf2SnapSize")))
	{
		const float BSPSnapSize = bUsePowerOf2SnapSize ? 128.0f : 100.0f;
		UModel::SetGlobalBSPTexelScale(BSPSnapSize);
	}
	else if (Name == FName(TEXT("BillboardScale")))
	{
		UBillboardComponent::SetEditorScale(BillboardScale);
		UArrowComponent::SetEditorScale(BillboardScale);
	}

	if (!FUnrealEdMisc::Get().IsDeletePreferences())
	{
		SaveConfig();
	}

	GEditor->RedrawAllViewports();

	SettingChangedEvent.Broadcast(Name);
}

/* UProjectPackagingSettings interface
 *****************************************************************************/

UProjectPackagingSettings::UProjectPackagingSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
}


void UProjectPackagingSettings::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr)
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (Name == FName((TEXT("DirectoriesToAlwaysCook"))))
	{
		// fix up paths
		for(int32 PathIndex = 0; PathIndex < DirectoriesToAlwaysCook.Num(); PathIndex++)
		{
			FString Path = DirectoriesToAlwaysCook[PathIndex].Path;
			FPaths::MakePathRelativeTo(Path, FPlatformProcess::BaseDir());
			DirectoriesToAlwaysCook[PathIndex].Path = Path;
		}
	}
	else if (Name == FName((TEXT("StagingDirectory"))))
	{
		// fix up path
		FString Path = StagingDirectory.Path;
		FPaths::MakePathRelativeTo(Path, FPlatformProcess::BaseDir());
		StagingDirectory.Path = Path;
	}
	else if (Name == FName(TEXT("ForDistribution")) || Name == FName(TEXT("BuildConfiguration")))
	{
		if (ForDistribution)
		{
			BuildConfiguration = EProjectPackagingBuildConfigurations::PPBC_Shipping;
		}
	}
}

bool UProjectPackagingSettings::CanEditChange( const UProperty* InProperty ) const
{
	if (InProperty->GetFName() == FName(TEXT("BuildConfiguration")) && ForDistribution)
	{
		return false;
	}

	return Super::CanEditChange(InProperty);
}