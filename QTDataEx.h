//////////
//
//	File:		QTDataEx.h
//
//	Contains:	Sample code for working with QuickTime's movie importers and exporters (data exchange components).
//
//	Written by:	Tim Monroe
//
//	Copyright:	� 1999 by Apple Computer, Inc., all rights reserved.
//
//	Change History (most recent first):
//	   
//	   <1>	 	02/11/00	rtm		first file
//
//////////

#pragma once


//////////
//
// header files
//
//////////

#include "ComApplication.h"

#ifndef _STDIO_H
#include <stdio.h>
#endif	


//////////
//
// compiler flags
//
//////////


//////////
//
// constants
//
//////////

#define kImportSavePrompt					"Save converted file as:"

// constants for our custom progress dialog box
#define kProgressDialogResID				1000
#define kProgressStopButtonItemID			1
#define kProgressBarItemID					2
#define kProgressPictureItemID				3
#define kProgressTextItemID					4
#define kProgressTimeItemID					5

#define kProgressPictureID					128

#define kPICTFileHeaderSize					512

#define kProgressBarMaxValue				5000

#define kOperationsStringsResID				1001

// type and creator for our sample settings preferences file
#define kSettingsFileType					FOUR_CHAR_CODE('Pref')
#define kSettingsFileCreator				FOUR_CHAR_CODE('RTM ')

// the name of our preferences file
#define kSettingsFileName					"HintPrefs.rtm"

// constants for displaying the remaining time
#define kTimeRemainingLabel					"Time remaining: "
#define kMinimumUsefulPercent				(Fixed)0x00000600
#if TARGET_OS_MAC
#define kTimeRemainingLabelSize				10
#endif
#if TARGET_OS_WIN32
#define kTimeRemainingLabelSize				8
#endif

// constants for exporting hinted movies
#define kHintedMovieSavePrompt				"Save hinted movie as: "
#define kHintedMovieFileName				"hinted.mov"


//////////
//
// function prototypes
//
//////////

OSErr						QTDX_ImportAnyNonMovie (void);
OSErr						QTDX_ExportMovieAsAnyTypeFile (Movie theMovie, FSSpec *theFSSpec);
OSErr						QTDX_ExportMovieAsHintedMovie (Movie theMovie, Boolean thePromptUser);

OSErr						QTDX_SetExportedMovieDimensions (MovieExportComponent theExporter, Fixed theHeight, Fixed theWidth);

Boolean						QTDX_FileCanBeImportedInPlace (FSSpec *theFSSpec);
Boolean						QTDX_ComponentHasUI (OSType theType, ComponentInstance theComponent);

#if TARGET_OS_MAC
PASCAL_RTN Boolean			QTDX_FilterFiles (AEDesc *theItem, void *theInfo, void *theCallBackUD, NavFilterModes theFilterMode);
#endif
#if TARGET_OS_WIN32
PASCAL_RTN Boolean			QTDX_FilterFiles (CInfoPBPtr thePBPtr);
#endif

PASCAL_RTN OSErr			QTDX_MovieProgressProc (Movie theMovie, short theMessage, short theOperation, Fixed thePercentDone, long theRefcon);
PASCAL_RTN OSErr			QTDX_ImageProgressProc (short theMessage, Fixed thePercentDone, long theRefCon);
PASCAL_RTN void				QTDX_ProgressBoxUserItemProcedure (DialogPtr theDialog, short theItem);

#if TARGET_OS_WIN32
static void					QTDX_ModelessCallback (EventRecord *theEvent, DialogPtr theDialog, short theItemHit);
#endif

OSErr						QTDX_GetPrefsFileSpec (FSSpecPtr thePrefsSpecPtr, void *theRefCon);

OSErr						QTDX_SaveExporterSettingsInFile (MovieExportComponent theExporter, FSSpecPtr theFSSpecPtr);
OSErr						QTDX_GetExporterSettingsFromFile (MovieExportComponent theExporter, FSSpecPtr theFSSpecPtr);
OSErr						QTDX_WriteHandleToFile (Handle theHandle, FSSpecPtr theFSSpecPtr);
Handle						QTDX_ReadHandleFromFile (FSSpecPtr theFSSpecPtr);

void						QTDX_EstimateRemainingTime (Rect *theRect, Fixed thePercentDone, UInt32 theTicksElapsed);
