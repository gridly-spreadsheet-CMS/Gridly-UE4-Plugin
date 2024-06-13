// Copyright (c) 2021 LocalizeDirect AB

#include "GridlyLocalizationServiceProvider.h"

#include "GridlyEditor.h"
#include "GridlyExporter.h"
#include "GridlyGameSettings.h"
#include "GridlyLocalizedText.h"
#include "GridlyLocalizedTextConverter.h"
#include "GridlyStyle.h"
#include "GridlyTask_DownloadLocalizedTexts.h"
#include "HttpModule.h"
#include "ILocalizationServiceModule.h"
#include "LocalizationCommandletTasks.h"
#include "LocalizationModule.h"
#include "LocalizationTargetTypes.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/IMainFrameModule.h"
#include "Internationalization/Culture.h"
#include "Misc/FeedbackContext.h"
#include "Misc/ScopedSlowTask.h"
#include "Serialization/JsonSerializer.h"

#if LOCALIZATION_SERVICES_WITH_SLATE
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#endif

#define LOCTEXT_NAMESPACE "Gridly"

static FName ProviderName("Gridly");

class FGridlyLocalizationTargetEditorCommands final : public TCommands<FGridlyLocalizationTargetEditorCommands>
{
public:
#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
	FGridlyLocalizationTargetEditorCommands() :
				TCommands<FGridlyLocalizationTargetEditorCommands>("GridlyLocalizationTargetEditor",
					NSLOCTEXT("Gridly", "GridlyLocalizationTargetEditor", "Gridly Localization Target Editor"), NAME_None,
					FAppStyle::GetAppStyleSetName())
	{
	}
#else
	FGridlyLocalizationTargetEditorCommands() :
			TCommands<FGridlyLocalizationTargetEditorCommands>("GridlyLocalizationTargetEditor",
				NSLOCTEXT("Gridly", "GridlyLocalizationTargetEditor", "Gridly Localization Target Editor"), NAME_None,
				FEditorStyle::GetStyleSetName())
	{
	}
#endif

	TSharedPtr<FUICommandInfo> ImportAllCulturesForTargetFromGridly;
	TSharedPtr<FUICommandInfo> ExportNativeCultureForTargetToGridly;
	TSharedPtr<FUICommandInfo> ExportTranslationsForTargetToGridly;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};

void FGridlyLocalizationTargetEditorCommands::RegisterCommands()
{
	UI_COMMAND(ImportAllCulturesForTargetFromGridly, "Import from Gridly",
		"Imports translations for all cultures of this target to Gridly.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ExportNativeCultureForTargetToGridly, "Export to Gridly",
		"Exports native culture and source text of this target to Gridly.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ExportTranslationsForTargetToGridly, "Export All to Gridly",
		"Exports source text and all translations of this target to Gridly.", EUserInterfaceActionType::Button, FInputChord());
}

FGridlyLocalizationServiceProvider::FGridlyLocalizationServiceProvider()
{
}

void FGridlyLocalizationServiceProvider::Init(bool bForceConnection)
{
	FGridlyLocalizationTargetEditorCommands::Register();
}

void FGridlyLocalizationServiceProvider::Close()
{
}

FText FGridlyLocalizationServiceProvider::GetStatusText() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Status"), LOCTEXT("Unknown", "Unknown / not implemented"));

	return FText::Format(LOCTEXT("GridlyStatusText", "Gridly status: {Status}"), Args);
}

bool FGridlyLocalizationServiceProvider::IsEnabled() const
{
	return true;
}

bool FGridlyLocalizationServiceProvider::IsAvailable() const
{
	return true; // Check for server availability
}

const FName& FGridlyLocalizationServiceProvider::GetName(void) const
{
	return ProviderName;
}

const FText FGridlyLocalizationServiceProvider::GetDisplayName() const
{
	return LOCTEXT("GridlyLocalizationService", "Gridly Localization Service");
}

ELocalizationServiceOperationCommandResult::Type FGridlyLocalizationServiceProvider::GetState(
	const TArray<FLocalizationServiceTranslationIdentifier>& InTranslationIds,
	TArray<TSharedRef<ILocalizationServiceState, ESPMode::ThreadSafe>>& OutState,
	ELocalizationServiceCacheUsage::Type InStateCacheUsage)
{
	return ELocalizationServiceOperationCommandResult::Succeeded;
}

ELocalizationServiceOperationCommandResult::Type FGridlyLocalizationServiceProvider::Execute(
	const TSharedRef<ILocalizationServiceOperation, ESPMode::ThreadSafe>& InOperation,
	const TArray<FLocalizationServiceTranslationIdentifier>& InTranslationIds,
	ELocalizationServiceOperationConcurrency::Type InConcurrency /*= ELocalizationServiceOperationConcurrency::Synchronous*/,
	const FLocalizationServiceOperationComplete& InOperationCompleteDelegate /*= FLocalizationServiceOperationComplete()*/)
{
	const TSharedRef<FDownloadLocalizationTargetFile, ESPMode::ThreadSafe> DownloadOperation =
		StaticCastSharedRef<FDownloadLocalizationTargetFile>(InOperation);
	const FString TargetCulture = DownloadOperation->GetInLocale();

	UGridlyTask_DownloadLocalizedTexts* Task = UGridlyTask_DownloadLocalizedTexts::DownloadLocalizedTexts(nullptr);

	// On success

	Task->OnSuccessDelegate.BindLambda(
		[DownloadOperation, InOperationCompleteDelegate, TargetCulture](const TArray<FPolyglotTextData>& PolyglotTextDatas)
		{
			if (PolyglotTextDatas.Num() > 0)
			{
				const FString AbsoluteFilePathAndName = FPaths::ConvertRelativePathToFull(
					FPaths::ProjectDir() / DownloadOperation->GetInRelativeOutputFilePathAndName());

				FGridlyLocalizedTextConverter::WritePoFile(PolyglotTextDatas, TargetCulture, AbsoluteFilePathAndName);

				// Callback

				InOperationCompleteDelegate.Execute(DownloadOperation, ELocalizationServiceOperationCommandResult::Succeeded);
			}
			else
			{
				DownloadOperation->SetOutErrorText(LOCTEXT("GridlyErrorParse", "Failed to parse downloaded content"));
				InOperationCompleteDelegate.Execute(DownloadOperation, ELocalizationServiceOperationCommandResult::Failed);
			}
		});

	// On fail

	Task->OnFailDelegate.BindLambda(
		[DownloadOperation, InOperationCompleteDelegate](const TArray<FPolyglotTextData>& PolyglotTextDatas,
		const FGridlyResult& Error)
		{
			DownloadOperation->SetOutErrorText(FText::FromString(Error.Message));
			InOperationCompleteDelegate.Execute(DownloadOperation, ELocalizationServiceOperationCommandResult::Failed);
		});

	Task->Activate();

	return ELocalizationServiceOperationCommandResult::Succeeded;
}

bool FGridlyLocalizationServiceProvider::CanCancelOperation(
	const TSharedRef<ILocalizationServiceOperation, ESPMode::ThreadSafe>& InOperation) const
{
	return false;
}

void FGridlyLocalizationServiceProvider::CancelOperation(
	const TSharedRef<ILocalizationServiceOperation, ESPMode::ThreadSafe>& InOperation)
{
}

void FGridlyLocalizationServiceProvider::Tick()
{
}

#if LOCALIZATION_SERVICES_WITH_SLATE
void FGridlyLocalizationServiceProvider::CustomizeSettingsDetails(IDetailCategoryBuilder& DetailCategoryBuilder) const
{
	const FText GridlySettingsInfoText = LOCTEXT("GridlySettingsInfo", "Use Project Settings to configure Gridly");
	FDetailWidgetRow& PublicKeyRow = DetailCategoryBuilder.AddCustomRow(GridlySettingsInfoText);
	PublicKeyRow.ValueContent()[SNew(STextBlock).Text(GridlySettingsInfoText)];
	PublicKeyRow.ValueContent().HAlign(EHorizontalAlignment::HAlign_Fill);
}

void FGridlyLocalizationServiceProvider::CustomizeTargetDetails(
	IDetailCategoryBuilder& DetailCategoryBuilder, TWeakObjectPtr<ULocalizationTarget> LocalizationTarget) const
{
	// Not implemented
}

void FGridlyLocalizationServiceProvider::CustomizeTargetToolbar(
	TSharedRef<FExtender>& MenuExtender, TWeakObjectPtr<ULocalizationTarget> LocalizationTarget) const
{
	const TSharedRef<FUICommandList> CommandList = MakeShareable(new FUICommandList());

	MenuExtender->AddToolBarExtension("LocalizationService", EExtensionHook::First, CommandList,
		FToolBarExtensionDelegate::CreateRaw(const_cast<FGridlyLocalizationServiceProvider*>(this),
			&FGridlyLocalizationServiceProvider::AddTargetToolbarButtons, LocalizationTarget, CommandList));
}

void FGridlyLocalizationServiceProvider::CustomizeTargetSetToolbar(
	TSharedRef<FExtender>& MenuExtender, TWeakObjectPtr<ULocalizationTargetSet> LocalizationTargetSet) const
{
	// Not implemented
}

void FGridlyLocalizationServiceProvider::AddTargetToolbarButtons(FToolBarBuilder& ToolbarBuilder,
	TWeakObjectPtr<ULocalizationTarget> LocalizationTarget, TSharedRef<FUICommandList> CommandList)
{
	// Don't add toolbar buttons if target is engine

	if (!LocalizationTarget->IsMemberOfEngineTargetSet())
	{
		const bool bIsTargetSet = false;
		CommandList->MapAction(FGridlyLocalizationTargetEditorCommands::Get().ImportAllCulturesForTargetFromGridly,
			FExecuteAction::CreateRaw(this, &FGridlyLocalizationServiceProvider::ImportAllCulturesForTargetFromGridly,
				LocalizationTarget, bIsTargetSet));
		ToolbarBuilder.AddToolBarButton(FGridlyLocalizationTargetEditorCommands::Get().ImportAllCulturesForTargetFromGridly,
			NAME_None,
			TAttribute<FText>(), TAttribute<FText>(),
			FSlateIcon(FGridlyStyle::GetStyleSetName(), "Gridly.ImportAction"));

		CommandList->MapAction(FGridlyLocalizationTargetEditorCommands::Get().ExportNativeCultureForTargetToGridly,
			FExecuteAction::CreateRaw(this, &FGridlyLocalizationServiceProvider::ExportNativeCultureForTargetToGridly,
				LocalizationTarget, bIsTargetSet));
		ToolbarBuilder.AddToolBarButton(
			FGridlyLocalizationTargetEditorCommands::Get().ExportNativeCultureForTargetToGridly, NAME_None,
			TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FGridlyStyle::GetStyleSetName(),
				"Gridly.ExportAction"));

		CommandList->MapAction(FGridlyLocalizationTargetEditorCommands::Get().ExportTranslationsForTargetToGridly,
			FExecuteAction::CreateRaw(this, &FGridlyLocalizationServiceProvider::ExportTranslationsForTargetToGridly,
				LocalizationTarget, bIsTargetSet));
		ToolbarBuilder.AddToolBarButton(
			FGridlyLocalizationTargetEditorCommands::Get().ExportTranslationsForTargetToGridly, NAME_None,
			TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FGridlyStyle::GetStyleSetName(),
				"Gridly.ExportAllAction"));
	}
}
#endif	  // LOCALIZATION_SERVICES_WITH_SLATE

void FGridlyLocalizationServiceProvider::ImportAllCulturesForTargetFromGridly(
	TWeakObjectPtr<ULocalizationTarget> LocalizationTarget, bool bIsTargetSet)
{
	check(LocalizationTarget.IsValid());

	const EAppReturnType::Type MessageReturn = FMessageDialog::Open(EAppMsgType::YesNo,
		LOCTEXT("ConfirmText",
			"All local translations to non-native languages will be overwritten. Are you sure you wish to update?"));

	if (!bIsTargetSet && MessageReturn == EAppReturnType::Yes)
	{
		TArray<FString> Cultures;

		for (int i = 0; i < LocalizationTarget->Settings.SupportedCulturesStatistics.Num(); i++)
		{
			if (i != LocalizationTarget->Settings.NativeCultureIndex)
			{
				const FCultureStatistics CultureStats = LocalizationTarget->Settings.SupportedCulturesStatistics[i];
				Cultures.Add(CultureStats.CultureName);
			}
		}

		CurrentCultureDownloads.Append(Cultures);
		SuccessfulDownloads = 0;

		const float AmountOfWork = CurrentCultureDownloads.Num();
		ImportAllCulturesForTargetFromGridlySlowTask = MakeShareable(new FScopedSlowTask(AmountOfWork,
			LOCTEXT("ImportAllCulturesForTargetFromGridlyText", "Importing all cultures for target from Gridly")));

		ImportAllCulturesForTargetFromGridlySlowTask->MakeDialog();

		for (const FString& CultureName : Cultures)
		{
			ILocalizationServiceProvider& Provider = ILocalizationServiceModule::Get().GetProvider();
			TSharedRef<FDownloadLocalizationTargetFile, ESPMode::ThreadSafe> DownloadTargetFileOp =
				ILocalizationServiceOperation::Create<FDownloadLocalizationTargetFile>();
			DownloadTargetFileOp->SetInTargetGuid(LocalizationTarget->Settings.Guid);
			DownloadTargetFileOp->SetInLocale(CultureName);

			FString Path = FPaths::ProjectSavedDir() / "Temp" / "Game" / LocalizationTarget->Settings.Name / CultureName /
			               LocalizationTarget->Settings.Name + ".po";
			FPaths::MakePathRelativeTo(Path, *FPaths::ProjectDir());
			DownloadTargetFileOp->SetInRelativeOutputFilePathAndName(Path);

			auto OperationCompleteDelegate = FLocalizationServiceOperationComplete::CreateRaw(this,
				&FGridlyLocalizationServiceProvider::OnImportCultureForTargetFromGridly, bIsTargetSet);

			Provider.Execute(DownloadTargetFileOp, TArray<FLocalizationServiceTranslationIdentifier>(),
				ELocalizationServiceOperationConcurrency::Synchronous, OperationCompleteDelegate);

			ImportAllCulturesForTargetFromGridlySlowTask->EnterProgressFrame(1.f);
		}

		ImportAllCulturesForTargetFromGridlySlowTask.Reset();
	}
}

void FGridlyLocalizationServiceProvider::OnImportCultureForTargetFromGridly(const FLocalizationServiceOperationRef& Operation,
	ELocalizationServiceOperationCommandResult::Type Result, bool bIsTargetSet)
{
	TSharedPtr<FDownloadLocalizationTargetFile, ESPMode::ThreadSafe> DownloadLocalizationTargetOp = StaticCastSharedRef<
		FDownloadLocalizationTargetFile>(Operation);

	CurrentCultureDownloads.Remove(DownloadLocalizationTargetOp->GetInLocale());

	if (Result == ELocalizationServiceOperationCommandResult::Succeeded)
	{
		SuccessfulDownloads++;
	}
	else
	{
		const FText ErrorMessage = DownloadLocalizationTargetOp->GetOutErrorText();
		UE_LOG(LogGridlyEditor, Error, TEXT("%s"), *ErrorMessage.ToString());
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(ErrorMessage.ToString()));
	}

	if (CurrentCultureDownloads.Num() == 0 && SuccessfulDownloads > 0)
	{
		const FString TargetName = FPaths::GetBaseFilename(DownloadLocalizationTargetOp->GetInRelativeOutputFilePathAndName());

		const auto Target = ILocalizationModule::Get().GetLocalizationTargetByName(TargetName, false);
		const FString AbsoluteFilePathAndName = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectDir() / DownloadLocalizationTargetOp->GetInRelativeOutputFilePathAndName());

		UE_LOG(LogGridlyEditor, Log, TEXT("Loading from file: %s"), *AbsoluteFilePathAndName);

		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
		
		if (!bIsTargetSet)
		{
			LocalizationCommandletTasks::ImportTextForTarget(MainFrameParentWindow.ToSharedRef(), Target,
				FPaths::GetPath(FPaths::GetPath(AbsoluteFilePathAndName)));

			Target->UpdateWordCountsFromCSV();
			Target->UpdateStatusFromConflictReport();
		}
	}
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateExportRequest(const TArray<FPolyglotTextData>& PolyglotTextDatas,
	const TSharedPtr<FLocTextHelper>& LocTextHelperPtr, bool bIncludeTargetTranslations)
{
	FString JsonString;
	FGridlyExporter::ConvertToJson(PolyglotTextDatas, bIncludeTargetTranslations, LocTextHelperPtr, JsonString);
	UE_LOG(LogGridlyEditor, Log, TEXT("Creating export request with %d entries"), PolyglotTextDatas.Num());

	const UGridlyGameSettings* GameSettings = GetMutableDefault<UGridlyGameSettings>();
	const FString ApiKey = GameSettings->ExportApiKey;
	const FString ViewId = GameSettings->ExportViewId;

	FStringFormatNamedArguments Args;
	Args.Add(TEXT("ViewId"), *ViewId);
	const FString Url = FString::Format(TEXT("https://api.gridly.com/v1/views/{ViewId}/records"), Args);

	auto HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("ApiKey %s"), *ApiKey));
	HttpRequest->SetContentAsString(JsonString);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetURL(Url);

	return HttpRequest;
}

void FGridlyLocalizationServiceProvider::ExportNativeCultureForTargetToGridly(
	TWeakObjectPtr<ULocalizationTarget> LocalizationTarget, bool bIsTargetSet)
{
	check(LocalizationTarget.IsValid());

	const EAppReturnType::Type MessageReturn = FMessageDialog::Open(EAppMsgType::YesNo,
		LOCTEXT("ConfirmText",
			"This will overwrite your source strings on Gridly with the data in your UE4 project. Are you sure you wish to export?"));

	if (!bIsTargetSet && MessageReturn == EAppReturnType::Yes)
	{
		ULocalizationTarget* InLocalizationTarget = LocalizationTarget.Get();
		if (InLocalizationTarget)
		{
			FHttpRequestCompleteDelegate ReqDelegate = FHttpRequestCompleteDelegate::CreateRaw(this,
				&FGridlyLocalizationServiceProvider::OnExportNativeCultureForTargetToGridly);

			const FText SlowTaskText = LOCTEXT("ExportNativeCultureForTargetToGridlyText",
				"Exporting native culture for target to Gridly");

			ExportForTargetToGridly(InLocalizationTarget, ReqDelegate, SlowTaskText);
		}
	}
}

void FGridlyLocalizationServiceProvider::OnExportNativeCultureForTargetToGridly(FHttpRequestPtr HttpRequestPtr,
	FHttpResponsePtr HttpResponsePtr, bool bSuccess)
{
	if (bSuccess)
	{
		if (HttpResponsePtr->GetResponseCode() == EHttpResponseCodes::Ok ||
		    HttpResponsePtr->GetResponseCode() == EHttpResponseCodes::Created)
		{
			const FString Content = HttpResponsePtr->GetContentAsString();
			const auto JsonStringReader = TJsonReaderFactory<TCHAR>::Create(Content);
			TArray<TSharedPtr<FJsonValue>> JsonValueArray;
			FJsonSerializer::Deserialize(JsonStringReader, JsonValueArray);
			ExportForTargetEntriesUpdated += JsonValueArray.Num();

			if (!IsRunningCommandlet())
			{
				ExportForTargetToGridlySlowTask->EnterProgressFrame(1.f);
			}

			TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest;
			if (ExportFromTargetRequestQueue.Dequeue(HttpRequest))
			{
				HttpRequest->ProcessRequest();
			}
			else
			{
				const FString Message = FString::Printf(TEXT("Number of entries updated: %llu"),
					ExportForTargetEntriesUpdated);
				UE_LOG(LogGridlyEditor, Log, TEXT("%s"), *Message);

				if (!IsRunningCommandlet())
				{
					FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
					ExportForTargetToGridlySlowTask.Reset();
				}

				bExportRequestInProgress = false;
			}
		}
		else
		{
			const FString Content = HttpResponsePtr->GetContentAsString();
			const FString ErrorReason =
				FString::Printf(TEXT("Error: %d, reason: %s"), HttpResponsePtr->GetResponseCode(), *Content);
			UE_LOG(LogGridlyEditor, Error, TEXT("%s"), *ErrorReason);

			if (!IsRunningCommandlet())
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(ErrorReason));
				ExportForTargetToGridlySlowTask.Reset();
			}

			bExportRequestInProgress = false;
		}
	}
	else
	{
		if (!IsRunningCommandlet())
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("GridlyConnectionError", "ERROR: Unable to connect to Gridly"));
			ExportForTargetToGridlySlowTask.Reset();
		}

		bExportRequestInProgress = false;
	}
}

void FGridlyLocalizationServiceProvider::ExportTranslationsForTargetToGridly(TWeakObjectPtr<ULocalizationTarget> LocalizationTarget,
	bool bIsTargetSet)
{
	check(LocalizationTarget.IsValid());

	const EAppReturnType::Type MessageReturn = FMessageDialog::Open(EAppMsgType::YesNo,
		LOCTEXT("ConfirmText",
			"This will overwrite all your source strings AND translations on Gridly with the data in your UE4 project. Are you sure you wish to export?"));

	if (!bIsTargetSet && MessageReturn == EAppReturnType::Yes)
	{
		ULocalizationTarget* InLocalizationTarget = LocalizationTarget.Get();
		if (InLocalizationTarget)
		{
			FHttpRequestCompleteDelegate ReqDelegate = FHttpRequestCompleteDelegate::CreateRaw(this,
				&FGridlyLocalizationServiceProvider::OnExportTranslationsForTargetToGridly);

			const FText SlowTaskText = LOCTEXT("ExportTranslationsForTargetToGridlyText",
					"Exporting source text and translations for target to Gridly");

			ExportForTargetToGridly(InLocalizationTarget, ReqDelegate, SlowTaskText, true);
		}
	}
}

void FGridlyLocalizationServiceProvider::OnExportTranslationsForTargetToGridly(FHttpRequestPtr HttpRequestPtr,
	FHttpResponsePtr HttpResponsePtr, bool bSuccess)
{
	if (bSuccess)
	{
		if (HttpResponsePtr->GetResponseCode() == EHttpResponseCodes::Ok ||
		    HttpResponsePtr->GetResponseCode() == EHttpResponseCodes::Created)
		{
			const FString Content = HttpResponsePtr->GetContentAsString();
			const auto JsonStringReader = TJsonReaderFactory<TCHAR>::Create(Content);
			TArray<TSharedPtr<FJsonValue>> JsonValueArray;
			FJsonSerializer::Deserialize(JsonStringReader, JsonValueArray);
			ExportForTargetEntriesUpdated += JsonValueArray.Num();

			if (!IsRunningCommandlet())
			{
				ExportForTargetToGridlySlowTask->EnterProgressFrame(1.f);
			}

			TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest;
			if (ExportFromTargetRequestQueue.Dequeue(HttpRequest))
			{
				HttpRequest->ProcessRequest();
			}
			else
			{
				const FString Message = FString::Printf(TEXT("Number of entries updated: %llu"),
					ExportForTargetEntriesUpdated);
				UE_LOG(LogGridlyEditor, Log, TEXT("%s"), *Message);

				if (!IsRunningCommandlet())
				{
					FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
					ExportForTargetToGridlySlowTask.Reset();
				}

				bExportRequestInProgress = false;
			}
		}
		else
		{
			const FString Content = HttpResponsePtr->GetContentAsString();
			const FString ErrorReason =
				FString::Printf(TEXT("Error: %d, reason: %s"), HttpResponsePtr->GetResponseCode(), *Content);
			UE_LOG(LogGridlyEditor, Error, TEXT("%s"), *ErrorReason);

			if (!IsRunningCommandlet())
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(ErrorReason));
				ExportForTargetToGridlySlowTask.Reset();
			}

			bExportRequestInProgress = false;
		}
	}
	else
	{
		if (!IsRunningCommandlet())
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("GridlyConnectionError", "ERROR: Unable to connect to Gridly"));
			ExportForTargetToGridlySlowTask.Reset();
		}

		bExportRequestInProgress = false;
	}
}

void FGridlyLocalizationServiceProvider::ExportForTargetToGridly(ULocalizationTarget* InLocalizationTarget, FHttpRequestCompleteDelegate& ReqDelegate, const FText& SlowTaskText, bool bIncTargetTranslation)
{
	TArray<FPolyglotTextData> PolyglotTextDatas;
	TSharedPtr<FLocTextHelper> LocTextHelperPtr;
	if (FGridlyLocalizedText::GetAllTextAsPolyglotTextDatas(InLocalizationTarget, PolyglotTextDatas, LocTextHelperPtr))
	{
		size_t TotalRequests = 0;

		while (PolyglotTextDatas.Num() > 0)
		{
			const size_t ChunkSize = FMath::Min(GetMutableDefault<UGridlyGameSettings>()->ExportMaxRecordsPerRequest, PolyglotTextDatas.Num());
			const TArray<FPolyglotTextData> ChunkPolyglotTextDatas(PolyglotTextDatas.GetData(), ChunkSize);
			PolyglotTextDatas.RemoveAt(0, ChunkSize);
			const auto HttpRequest = CreateExportRequest(ChunkPolyglotTextDatas, LocTextHelperPtr, bIncTargetTranslation);
			HttpRequest->OnProcessRequestComplete() = ReqDelegate;
			ExportFromTargetRequestQueue.Enqueue(HttpRequest);
			TotalRequests++;
		}

		ExportForTargetEntriesUpdated = 0;

		TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest;
		if (ExportFromTargetRequestQueue.Dequeue(HttpRequest))
		{
			if (!IsRunningCommandlet())
			{
				ExportForTargetToGridlySlowTask = MakeShareable(new FScopedSlowTask(static_cast<float>(TotalRequests), SlowTaskText));
				ExportForTargetToGridlySlowTask->MakeDialog();
			}

			bExportRequestInProgress = true;
			HttpRequest->ProcessRequest();
		}
	}
}

bool FGridlyLocalizationServiceProvider::HasRequestsPending() const
{
	return !ExportFromTargetRequestQueue.IsEmpty() || bExportRequestInProgress;
}

FHttpRequestCompleteDelegate FGridlyLocalizationServiceProvider::CreateExportNativeCultureDelegate()
{
	return FHttpRequestCompleteDelegate::CreateRaw(this, &FGridlyLocalizationServiceProvider::OnExportNativeCultureForTargetToGridly);
}

#undef LOCTEXT_NAMESPACE
