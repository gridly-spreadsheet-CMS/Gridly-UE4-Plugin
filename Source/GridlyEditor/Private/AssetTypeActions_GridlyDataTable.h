// Copyright (c) 2021 LocalizeDirect AB

#pragma once

#include "AssetTypeActions_CSVAssetBase.h"
#include "GridlyDataTable.h"
#include "Interfaces/IHttpRequest.h"

#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2)
class FAssetTypeActions_GridlyDataTable : public FAssetTypeActions_Base
#else
class FAssetTypeActions_GridlyDataTable : public FAssetTypeActions_CSVAssetBase
#endif
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_GridlyDataTable", "Gridly Data Table"); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual UClass* GetSupportedClass() const override { return UGridlyDataTable::StaticClass(); }
	virtual FColor GetTypeColor() const override { return FColor::Blue; }
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	// End IAssetTypeActions

private:
	void ExecuteImportFromGridly(TArray<TWeakObjectPtr<UObject>> Objects);
	void ExecuteExportAsCSV(TArray<TWeakObjectPtr<UObject>> Objects);
	void ExecuteExportAsJSON(TArray<TWeakObjectPtr<UObject>> Objects);

private:
	void ImportFromGridly(UGridlyDataTable* DataTable);
	void ExportToGridly(UGridlyDataTable* DataTable);
	void AddToolbarButton(FToolBarBuilder& Builder);

	TQueue<TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>> ExportRequestQueue;
	static TMap<uint32, TSharedPtr<FScopedSlowTask, ESPMode::ThreadSafe>> ImportSlowTasks;
};
