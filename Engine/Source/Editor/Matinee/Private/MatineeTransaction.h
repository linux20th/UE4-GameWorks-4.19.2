// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/** 
 * Matinee specific transaction record that ignores non-matinee objects.
 */
class FMatineeTransaction : public FTransaction
{
public:

	FMatineeTransaction( const FText& InTitle=FText(), bool InFlip=0 )
		:	FTransaction(TEXT("Matinee"), InTitle, InFlip)
	{ }

public:

	// FTransaction interface

	virtual void SaveObject( UObject* Object ) override;
	virtual void SaveArray( UObject* Object, FScriptArray* Array, int32 Index, int32 Count, int32 Oper, int32 ElementSize, STRUCT_DC DefaultConstructor, STRUCT_AR Serializer, STRUCT_DTOR Destructor ) override;
};

