// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once


/**
 * Local file system based title file implementation (for testing).
 * This code was salvaged from the deprecated EpicContent plug-in.
 */
class FLocalNewsFeedTitleFile
	: public IOnlineTitleFile
{
public:

	static IOnlineTitleFilePtr Create( const FString& RootDirectory );

public:

	// IOnlineTitleFile interface

	virtual bool GetFileContents(const FString& DLName, TArray<uint8>& FileContents) override;
	virtual bool ClearFiles() override;
	virtual bool ClearFile(const FString& DLName) override;
	virtual bool EnumerateFiles() override;
	virtual bool EnumerateFiles(int32 Start, int32 Count) override;
	virtual void GetFileList(TArray<FCloudFileHeader>& InFileHeaders) override;
	virtual bool ReadFile(const FString& DLName) override;

protected:

	FString GetFileNameFromDLName( const FString& DLName ) const;

private:

	FLocalNewsFeedTitleFile( const FString& InRootDirectory );

private:

	FString RootDirectory;
	TArray<FCloudFileHeader> FileHeaders;
	TMap< FString, TArray<uint8> > DLNameToFileContents;
};
