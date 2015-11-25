//////////
//
//	File:		QTDataEx.c
//
//	Contains:	Sample code for working with QuickTime's movie importers and exporters (data exchange components).
//
//	Written by:	Tim Monroe
//
//	Copyright:	� 1999 by Apple Computer, Inc., all rights reserved.
//
//	Change History (most recent first):
//	   
//	   <7>	 	05/11/02	rtm		fixed type of gValidFileTypes (now a handle)
//	   <6>	 	01/02/02	rtm		Carbonized a SetGWorld call in QTDX_MovieProgressProc
//	   <5>	 	04/19/01	rtm		added QTDX_SetExportedMovieDimensions
//	   <4>	 	06/02/00	rtm		fixed crashing bug in QTDX_MovieProgressProc
//	   <3>	 	03/20/00	rtm		made changes to get things running under CarbonLib
//	   <2>	 	03/03/00	rtm		finished basic functionality
//	   <1>	 	02/11/00	rtm		first file
//
//	This file contains code that illustrates the most basic ways of using QuickTime's movie importers and exporters.
//	It shows how to import files that are not QuickTime movie files, how to export a movie into any format supported
//	by QuickTime's movie exporters, and how to export a movie as a hinted movie file. It also illustrates how to use
//	a custom movie progress function.
//
//////////


//////////
//
// header files
//
//////////

#include "QTDataEx.h"
#include <TCHAR.H>

//////////
//
// global variables
//
//////////

MovieProgressUPP			gMovieProgressProcUPP = NULL;				// UPP to our custom movie progress dialog box procedure
ICMProgressUPP				gImageProgressProcUPP = NULL;				// UPP to our custom image progress dialog box procedure
UserItemUPP					gProgressUserItemProcUPP = NULL;			// UPP to our custom progress dialog user item procedure
Boolean						gUserCancelled = false;						// did the user cancel a long operation? (Windows only)

extern short 				gAppResFile;								// file reference number for this application's resource file
extern Handle 				gValidFileTypes;							// the list of file types that our application can open

StringPtr					gSettingsFileName;							// the name of our settings preferences file


//////////
//
// QTDX_ImportAnyNonMovie
// Let the user browse for any non-movies; import the selected file as a movie.
//
//////////

OSErr QTDX_ImportAnyNonMovie (void)
{
	QTFrameFileFilterUPP	myFileFilterUPP = NULL;
	QTFrameTypeListPtr 		myTypeListPtr = NULL;
	short					myNumTypes = 0;
	Movie					myMovie = NULL;
	FSSpec					myFileToConvert;
	FSSpec					myConvertedFile;
	StringPtr 				myPrompt = QTUtils_ConvertCToPascalString(kImportSavePrompt);
	OSErr					myErr = noErr;

#if TARGET_OS_WIN32
	myTypeListPtr = (QTFrameTypeListPtr)&gValidFileTypes[1];						// [0] is kQTFileTypeMovie	
	myNumTypes = (short)(GetPtrSize((Ptr)gValidFileTypes) / sizeof(OSType)) - 1;
#endif

	// let the user select an openable file from any files that aren't movie files
	myFileFilterUPP = QTFrame_GetFileFilterUPP((ProcPtr)QTDX_FilterFiles);

	myErr = QTFrame_GetOneFileWithPreview(myNumTypes, myTypeListPtr, &myFileToConvert, (void *)myFileFilterUPP);
	if (myErr != noErr)
		goto bail;

	myConvertedFile = myFileToConvert;

	//////////
	//
	// determine whether the selected file needs to be converted into another file before QuickTime can open it;
	// if so, do the conversion; this is necessary only on MacOS, because on Windows QTFrame_GetOneFileWithPreview
	// calls StandardGetFilePreview, which does this all automatically
	//
	//////////
#if TARGET_OS_MAC	
	if (!QTDX_FileCanBeImportedInPlace(&myFileToConvert)) {
	
		Boolean				myIsSelected = false;
		Boolean				myIsReplacing = false;
		
		// display put-file dialog to save the converted file
		QTFrame_PutFile(myPrompt, myFileToConvert.name, &myConvertedFile, &myIsSelected, &myIsReplacing);
		if (!myIsSelected)
			goto bail;
								
		// delete any existing file of that name
		if (myIsReplacing) {
			myErr = DeleteMovieFile(&myConvertedFile);
			if (myErr != noErr)
				goto bail;
		}

	  	// import the file into a movie	
		myErr = ConvertFileToMovieFile(
							&myFileToConvert,			// the file to convert
							&myConvertedFile,			// the file to convert it into
							FOUR_CHAR_CODE('TVOD'),		// the output file creator
							smSystemScript,				// the script
							NULL,
							0L,
							NULL,
							gMovieProgressProcUPP,
							0L);
	}
#endif
	
	// now open the (possibly) converted file in a window
	if (myErr == noErr)
		QTFrame_OpenMovieInWindow(NULL, &myConvertedFile);
	
bail:
	if (myFileFilterUPP != NULL)
		DisposeNavObjectFilterUPP(myFileFilterUPP);
		
	free(myPrompt);

	return(myErr);
}


//////////
//
// QTDX_ExportMovieAsAnyTypeFile
// Export the specified movie as a file whose type the user selects.
//
//////////

OSErr QTDX_ExportMovieAsAnyTypeFile (Movie theMovie, FSSpec *theFSSpec)
{
	FSSpec					myFSSpec = *theFSSpec;
	long					myFlags = 0L;
	OSErr					myErr = noErr;

	myFlags = createMovieFileDeleteCurFile | showUserSettingsDialog | movieFileSpecValid | movieToFileOnlyExport;
	
	// export the movie into a file
	myErr = ConvertMovieToFile(
						theMovie,					// the movie to convert
						NULL,						// all tracks in the movie
						&myFSSpec,					// the output file
						0L,							// the output file type
						FOUR_CHAR_CODE('TVOD'),		// the output file creator
						smSystemScript,				// the script
						NULL, 						// no resource ID to be returned
						myFlags,					// export flags
						NULL);						// no specific export component
						
	return(myErr);
}


//////////
//
// QTDX_ExportMovieAsHintedMovie
// Add a hint track to a QuickTime movie, using the hinter movie export component.
//
// The thePromptUser parameter determines whether we display the movie exporter
// settings dialog box to allow the user to select export options (true) or whether we
// try to read the export options from an existing preferences file (false).
//
//////////

OSErr QTDX_ExportMovieAsHintedMovie (Movie theMovie, Boolean thePromptUser)
{
	ComponentDescription		myCompDesc;
	MovieExportComponent		myExporter = NULL;
	long						myFlags = createMovieFileDeleteCurFile | movieFileSpecValid;
	FSSpec						myHintedFile;
	FSSpec						myPrefsFile;
	Boolean						myIsSelected = false;
	Boolean						myIsReplacing = false;
	StringPtr 					myPrompt = QTUtils_ConvertCToPascalString(kHintedMovieSavePrompt);
	StringPtr 					myFileName = QTUtils_ConvertCToPascalString(kHintedMovieFileName);
	ComponentResult				myErr = badComponentType;

	// get an output file for the hinted movie
	QTFrame_PutFile(myPrompt, myFileName, &myHintedFile, &myIsSelected, &myIsReplacing);
	if (!myIsSelected) {
		myErr = userCanceledErr;
		goto bail;
	}
	
	if (myIsReplacing) {
		myErr = FSpDelete(&myHintedFile);
		if (myErr != noErr)
			goto bail;
	}
		
	// find and open a movie export component that can hint a movie file
	myCompDesc.componentType = MovieExportType;
	myCompDesc.componentSubType = MovieFileType;
	myCompDesc.componentManufacturer = FOUR_CHAR_CODE('hint');
	myCompDesc.componentFlags = 0;
	myCompDesc.componentFlagsMask = 0;
	myExporter = OpenComponent(FindNextComponent(NULL, &myCompDesc));
	if (myExporter == NULL)
		goto bail;

	// get the preferences file for this application
	QTDX_GetPrefsFileSpec(&myPrefsFile, (void *)&myHintedFile);
	
	// read existing movie exporter settings from a file; if we aren't going to prompt
	// the user for exporter settings, these stored settings will be used; otherwise,
	// these stored settings will be used as initial values in the settings dialog box
	QTDX_GetExporterSettingsFromFile(myExporter, &myPrefsFile);
	
	if (thePromptUser && QTDX_ComponentHasUI(MovieExportType, myExporter)) {
		Boolean		myCancelled = false;
		
		// display a dialog box to prompt the user for desired movie exporter settings		
		myErr = MovieExportDoUserDialog(myExporter, theMovie, NULL, 0, 0, &myCancelled);
		if (myCancelled)
			goto bail;
		
		// save the existing settings into our preferences file
		QTDX_SaveExporterSettingsInFile(myExporter, &myPrefsFile);
	}

	// export the movie into a file
	myErr = ConvertMovieToFile(	theMovie,				// the movie to convert
								NULL,					// all tracks in the movie
								&myHintedFile,			// the output file
								MovieFileType,			// the output file type
								FOUR_CHAR_CODE('TVOD'),	// the output file creator
								smSystemScript,			// the script
								NULL, 					// no resource ID to be returned
								myFlags,				// conversion flags
								myExporter);			// hinter movie export component

bail:
	// close the movie export component
	if (myExporter != NULL)
		CloseComponent(myExporter);
		
	free(myPrompt);
	free(myFileName);

	return((OSErr)myErr);
}


//////////
//
// QTDX_SetExportedMovieDimensions
// Configure the movie exporter to export to a specific height and width.
//
// The basic idea is this: add atoms of type movieExportHeight and movieExportWidth to the
// video settings atom (which is of type kQTSettingsVideo) in the atom container returned by
// MovieExportGetSettingsAsAtomContainer; then install the revised atom container by calling
// MovieExportSetSettingsFromAtomContainer.
//
//////////

OSErr QTDX_SetExportedMovieDimensions (MovieExportComponent theExporter, Fixed theHeight, Fixed theWidth)
{	
	QTAtomContainer		myContainer = NULL;
	QTAtom				myVideoSettingsAtom = 0;
	QTAtom				mySizeAtom = 0;
	Fixed				myHeight, myWidth;
	OSErr				myErr = noErr;
	
	if (theExporter == NULL)
		return(paramErr);
		
	myErr = MovieExportGetSettingsAsAtomContainer(theExporter, &myContainer);
	if (myContainer != NULL) {
		// see if a video settings atom exists; if not, add one
		myVideoSettingsAtom = QTFindChildByID(myContainer, kParentAtomIsContainer, kQTSettingsVideo, 1, NULL);
		if (myVideoSettingsAtom == 0)
			QTInsertChild(myContainer, kParentAtomIsContainer, kQTSettingsVideo, 1, 0, 0, NULL, &myVideoSettingsAtom);
			
		if (myVideoSettingsAtom != 0) {
			// add an atom of type movieExportHeight, or replace data of existing atom
			myHeight = EndianU32_NtoB(theHeight);
			
			mySizeAtom = QTFindChildByID(myContainer, myVideoSettingsAtom, movieExportHeight, 1, NULL);
			if (mySizeAtom != 0)
				myErr = QTSetAtomData(myContainer, mySizeAtom, sizeof(myHeight), &myHeight);
			else
				myErr = QTInsertChild(myContainer, myVideoSettingsAtom, movieExportHeight, 1, 0, sizeof(myHeight), &myHeight, NULL);

			// add an atom of type movieExportWidth, or replace data of existing atom
			myWidth = EndianU32_NtoB(theWidth);
			
			mySizeAtom = QTFindChildByID(myContainer, myVideoSettingsAtom, movieExportWidth, 1, NULL);
			if (mySizeAtom != 0)
				myErr = QTSetAtomData(myContainer, mySizeAtom, sizeof(myWidth), &myWidth);
			else
				myErr = QTInsertChild(myContainer, myVideoSettingsAtom, movieExportWidth, 1, 0, sizeof(myWidth), &myWidth, NULL);
		}
	
		myErr = MovieExportSetSettingsFromAtomContainer(theExporter, myContainer);
			
		QTDisposeAtomContainer(myContainer);
	}
	
	return(myErr);
}


//////////
//
// QTDX_FileCanBeImportedInPlace
// Can the specified file be opened in place (that is, without having
// to create an intermediate file to hold the converted file data)?
//
//////////

Boolean QTDX_FileCanBeImportedInPlace (FSSpec *theFSSpec)
{
	ComponentDescription		myCompDesc;
	Component					myComponent = NULL;
	Boolean						myCanImportInPlace = false;
	OSType						mySubType;
	unsigned long				myFlags = 0;
	OSErr						myErr = noErr;

#if TARGET_OS_MAC
	FInfo						myFileInfo;

	// get the file type of the specified file
	myErr = FSpGetFInfo(theFSSpec, &myFileInfo);
	if (myErr != noErr)
		goto bail;
	
	mySubType = myFileInfo.fdType;
#endif

#if TARGET_OS_WIN32	
	// get the filename extension of the specified file
	myErr = QTGetFileNameExtension(theFSSpec->name, 0L, &mySubType);
	if (myErr != noErr)
		goto bail;

	myFlags = movieImportSubTypeIsFileExtension;
#endif

	myCompDesc.componentType = MovieImportType;
	myCompDesc.componentSubType = mySubType;
	myCompDesc.componentManufacturer = 0;
	myCompDesc.componentFlags = myFlags;
	myCompDesc.componentFlagsMask = myFlags;

	myComponent = FindNextComponent(NULL, &myCompDesc);
	if (myComponent != NULL) {
		GetComponentInfo(myComponent, &myCompDesc, NULL, NULL, NULL);
		if (myCompDesc.componentFlags & canMovieImportInPlace)
			myCanImportInPlace = true;
	}
	
bail:
	return(myCanImportInPlace);
}


//////////
//
// QTDX_ComponentHasUserInterface
// Can the specified movie importer or exporter display a settings dialog?
//
//////////

Boolean QTDX_ComponentHasUI (OSType theType, ComponentInstance theComponent)
{
	ComponentDescription		myCompDesc;
	Boolean						myHasUI = false;
	unsigned long				myTestFlag;
	OSErr						myErr = noErr;

	if (theType == MovieImportType)
		myTestFlag = hasMovieImportUserInterface;
	else if (theType == MovieExportType)
		myTestFlag = hasMovieExportUserInterface;
	else
		return(false);			// not a component type we can handle here

	if (theComponent != NULL) {
		GetComponentInfo((Component)theComponent, &myCompDesc, NULL, NULL, NULL);
		if (myCompDesc.componentFlags & myTestFlag)
			myHasUI = true;
	}
	
	return(myHasUI);
}


//////////
//
// QTDX_FilterFiles
// Filter files for a file-opening dialog box.
//
// The default behavior here is to accept all files that can be opened by QuickTime, whether directly
// or using a movie importer or a graphics importer. For present purposes, we exclude movie files (since
// we are looking for files to import).
//
//////////

#if TARGET_OS_MAC
PASCAL_RTN Boolean QTDX_FilterFiles (AEDesc *theItem, void *theInfo, void *theCallBackUD, NavFilterModes theFilterMode)
{
#pragma unused(theCallBackUD, theFilterMode)
	NavFileOrFolderInfo		*myInfo = (NavFileOrFolderInfo *)theInfo;
	
	if (gValidFileTypes == NULL)
		QTFrame_BuildFileTypeList();

	if (theItem->descriptorType == typeFSS) {
		if (!myInfo->isFolder) {
			OSType			myType = myInfo->fileAndFolder.fileInfo.finderInfo.fdType;
			OSType			*myTypes = (OSType *)*gValidFileTypes;
			long			myCount;
			long			myIndex;
			
			// see whether the file type is in the list of file types that our application can open 
			// but do not allow movie files
			myCount = GetHandleSize(gValidFileTypes) / sizeof(OSType);
			for (myIndex = 0; myIndex < myCount; myIndex++)
				if ((myType == myTypes[myIndex]) && (myType != kQTFileTypeMovie))
					return(true);

			// if we got to here, it's a file we cannot open
			return(false);		
		}
	}
	
	// if we got to here, it's a folder or non-HFS object
	return(true);
}
#endif
#if TARGET_OS_WIN32
PASCAL_RTN Boolean QTDX_FilterFiles (CInfoPBPtr thePBPtr)
{
	Boolean		mySuppressItem = true;
	FSSpec		myFSSpec;
	FInfo		myFileInfo;
	OSErr		myErr = noErr;
	
	if (thePBPtr->hFileInfo.ioFlAttrib & ioDirMask) {
		// it's a directory, so show it in the list
		mySuppressItem = false;
	} else {
		// it's a file, so show it in the list if it's not a movie file
		myErr = FSMakeFSSpec(	thePBPtr->hFileInfo.ioVRefNum,
								thePBPtr->hFileInfo.ioFlParID,
								thePBPtr->hFileInfo.ioNamePtr,
								&myFSSpec);
		if (myErr == noErr) {
			FSpGetFInfo(&myFSSpec, &myFileInfo);
			if (myFileInfo.fdType != kQTFileTypeMovie)
				mySuppressItem = false;
		}
	}

	return(mySuppressItem);
}
#endif


//////////
//
// QTDX_MovieProgressProc
// Handle a custom progress dialog box.
//
// The theRefCon parameter is a window object; we don't actually use it here, however.
//
//////////

PASCAL_RTN OSErr QTDX_MovieProgressProc (Movie theMovie, short theMessage, short theOperation, Fixed thePercentDone, long theRefcon)
{
#pragma unused(theMovie, theRefcon)

	CGrafPtr 				mySavedPort = NULL;
	GDHandle				mySavedDevice = NULL;
	static DialogPtr		myDialog = NULL;
	static ControlHandle	myBar = NULL;
	static UInt32			myTicks = 0;
	short					myItemKind;
	Handle					myItemHandle = NULL;
	Rect					myItemRect;
	Rect					myEraseRect;
	Str255	 				myString;
	EventRecord				myEvent;
	char					myKey;	
	OSErr					myErr = noErr;
	
	GetGWorld(&mySavedPort, &mySavedDevice);
	if (myDialog != NULL)
#if TARGET_API_MAC_CARBON
		SetGWorld(GetDialogPort(myDialog), GetMainDevice());
#else
		SetGWorld((CGrafPtr)myDialog, GetMainDevice());
#endif

	switch (theMessage) {
		case movieProgressOpen:
		
			//////////
			//
			// a lengthy operation is about to begin
			//
			//////////
		
			// display the progress dialog box
			myDialog = GetNewDialog(kProgressDialogResID, NULL, (WindowPtr)-1);
			if (myDialog != NULL) {
			
				// set the dialog box as the current graphics port
#if TARGET_API_MAC_CARBON
				SetGWorld(GetDialogPort(myDialog), GetMainDevice());
#else
				SetGWorld((CGrafPtr)myDialog, GetMainDevice());
#endif

				SetDialogCancelItem(myDialog, kProgressStopButtonItemID);
				
				// configure the progress bar control
				GetDialogItem(myDialog, kProgressBarItemID, &myItemKind, &myItemHandle, &myItemRect);						
				myBar = (ControlHandle)myItemHandle;
				SetControlMinimum(myBar, 0);
				SetControlMaximum(myBar, (SInt16)kProgressBarMaxValue);
				
				// set the dialog box text that describes the current operation
				GetDialogItem(myDialog, kProgressTextItemID, &myItemKind, &myItemHandle, &myItemRect);
				if ((theOperation > 0) && (theOperation <= progressOpExportMovie)) {
					GetIndString(myString, kOperationsStringsResID, theOperation);
					SetDialogItemText(myItemHandle, myString);
				}
				
				// set a user-item drawing procedure for the picture rectangle
				GetDialogItem(myDialog, kProgressPictureItemID, &myItemKind, &myItemHandle, &myItemRect);						
				SetDialogItem(myDialog, kProgressPictureItemID, myItemKind, (Handle)gProgressUserItemProcUPP, &myItemRect);
				
				// show the dialog box and draw the picture in the user item rectangle
				MacShowWindow(GetDialogWindow(myDialog));
				QTDX_ProgressBoxUserItemProcedure(myDialog, kProgressPictureItemID);
				DrawDialog(myDialog);
				
				myTicks = TickCount();

#if TARGET_OS_WIN32
				// set a dialog callback procedure, to notify our progress proc that the user has cancelled
				SetModelessDialogCallbackProc(myDialog, (QTModelessCallbackUPP)QTDX_ModelessCallback);
				
				// initialize the global variable that keeps track of whether the user has cancelled; another way
				// to pass info to the callback procedure would be to set the dialog box' refcon using SetWRefCon
				gUserCancelled = false;	
#endif
			}
			
			break;
			
		case movieProgressUpdatePercent:
		
			//////////
			//
			// a lengthy operation is in progress
			//
			//////////
		
#if TARGET_OS_WIN32
			if (gUserCancelled) {
				myErr = userCanceledErr;			// stop the operation by returning a non-zero value
				goto bail;
			}
#endif
		
			// check to see whether the user wants to cancel the operation; we support user cancelling
			// by (1) clicking the Stop button, (2) pressing the Escape key, or (3) pressing the Command-period
			// key combination
			
			// get the item information for the Stop button
			GetDialogItem(myDialog, kProgressStopButtonItemID, &myItemKind, &myItemHandle, &myItemRect);

			// check for user clicks in the Stop button
			if (WaitNextEvent(mDownMask, &myEvent, 0, NULL)) {
				GlobalToLocal(&myEvent.where);
				if (TrackControl((ControlHandle)myItemHandle, myEvent.where, NULL))
					myErr = userCanceledErr;		// stop the operation by returning a non-zero value
			}

			// check for user presses on the Escape key or on equivalent key combinations
			if (WaitNextEvent(keyDownMask, &myEvent, 0, NULL)) {
				myKey = myEvent.message & charCodeMask;
				
				if (myEvent.modifiers & cmdKey)
					if (IsCmdChar(&myEvent, kPeriod))
						myKey = kEscapeKey;
						
				if (myKey == kEscapeKey) {
					unsigned long		myTicks;

					// simulate a press on the Stop button
					HiliteControl((ControlHandle)myItemHandle, kControlButtonPart);
					Delay(kMyButtonDelay, &myTicks);
					HiliteControl((ControlHandle)myItemHandle, false);
					
					myErr = userCanceledErr;		// stop the operation by returning a non-zero value
				}
			}

			// make sure the thePercentDone is within the expected range
			if ((thePercentDone < 0) || (thePercentDone > fixed1))
				break;

			// update our progress dialog box
			if (myBar != NULL) {
				// thePercentDone is in the range 0 to fixed1 (0x00000000 to 0x00010000);
				// we need to scale it to lie within the range 0 to kProgressBarMaxValue
				SetControlValue(myBar, (SInt16)Fix2Long(FixMul(thePercentDone, Long2Fix(kProgressBarMaxValue))));
			}
			
			// erase the appropriate bottom portion of the picture
			GetDialogItem(myDialog, kProgressPictureItemID, &myItemKind, &myItemHandle, &myItemRect);
			MacSetRect(	&myEraseRect,
						myItemRect.left,
						myItemRect.bottom - (SInt16)Fix2Long(FixMul(thePercentDone, Long2Fix(myItemRect.bottom - myItemRect.top))),
						myItemRect.right,
						myItemRect.bottom);
			EraseRect(&myEraseRect);
									
			// update the estimated time remaining
			GetDialogItem(myDialog, kProgressTimeItemID, &myItemKind, &myItemHandle, &myItemRect);
			QTDX_EstimateRemainingTime(&myItemRect, thePercentDone, TickCount() - myTicks);
									
			break;
			
		case movieProgressClose:
		
			//////////
			//
			// a lengthy operation has completed
			//
			//////////

			// remove our progress dialog box
			if (myDialog != NULL)
				DisposeDialog(myDialog);

			myDialog = NULL;
			myBar = NULL;			
			myTicks = 0;
			
			break;
	}

bail:	
	SetGWorld(mySavedPort, mySavedDevice);
	
	return(myErr);
}


//////////
//
// QTDX_ImageProgressProc
// Handle a custom progress dialog box.
//
//////////

PASCAL_RTN OSErr QTDX_ImageProgressProc (short theMessage, Fixed thePercentDone, long theRefCon)
{
	return(QTDX_MovieProgressProc(NULL, theMessage, 0, thePercentDone, theRefCon));
}


//////////
//
// QTDX_ProgressBoxUserItemProcedure
// A user-item procedure to draw a picture in the progress dialog box.
//
//////////

PASCAL_RTN void QTDX_ProgressBoxUserItemProcedure (DialogPtr theDialog, short theItem)
{
	short 						mySavedResFile;
	short						myItemKind;
	Handle						myItemHandle = NULL;
	Rect						myItemRect;
	PicHandle					myPicture = NULL;
	OSErr						myErr = noErr;
	
	if (theItem != kProgressPictureItemID)
		return;

	MacSetPort((GrafPtr)GetDialogPort(theDialog));
	
	// get the current resource file
	mySavedResFile = CurResFile();

	// set the application's resource file;
	// otherwise, we'd get the dialog's resources from the current resource file,
	// which might not be the correct one....
	UseResFile(gAppResFile);

	// read a picture from our resource file
	myPicture = GetPicture(kProgressPictureID);
	if (myPicture == NULL)
		goto bail;

	// get the rectangle surrounding the user item
	GetDialogItem(theDialog, kProgressPictureItemID, &myItemKind, &myItemHandle, &myItemRect);	
	
	// draw the picture into the desired rectangle
	DrawPicture(myPicture, &myItemRect);
		
	// draw a frame around the picture
	MacInsetRect(&myItemRect, -1, -1);
	MacFrameRect(&myItemRect);
	
bail:
	if (myPicture != NULL)
		ReleaseResource((Handle)myPicture);

	// restore the previous resource file
	UseResFile(mySavedResFile);
}


#if TARGET_OS_WIN32
//////////
//
// QTDX_ModelessCallback
// A callback procedure for our progress dialog box.
//
// Here we pass information back to the progress procedure using the global variable gUserCancelled;
// a better strategy might be to pass a pointer or handle in the dialog box' refcon, which you would
// retrieve here by calling GetWRefCon.
//
//////////

static void QTDX_ModelessCallback (EventRecord *theEvent, DialogPtr theDialog, short theItemHit)
{
#pragma unused(theEvent, theDialog)

	if (theItemHit == kProgressStopButtonItemID)
		gUserCancelled = true;
}
#endif


//////////
//
// QTDX_GetPrefsFileSpec
// Fill in the specified FSSpec with info about this application's preferences file.
//
// The theRefCon parameter is a pointer to some application-specific data, which you
// might use to find the preferences file; here, we assume it's a pointer to an FSSpec
// for the output hinted file. We'll specify a preferences file in the same folder as
// the hinted file that has the name specified by the global variable gSettingsFileName.
//
//////////

OSErr QTDX_GetPrefsFileSpec (FSSpecPtr thePrefsSpecPtr, void *theRefCon)
{
	FSSpecPtr	myFSSpecPtr = (FSSpecPtr)theRefCon;
	OSErr		myErr = noErr;

	if (myFSSpecPtr == NULL)
		return(paramErr);
		
	myErr = FSMakeFSSpec(myFSSpecPtr->vRefNum, myFSSpecPtr->parID, gSettingsFileName, thePrefsSpecPtr);
	
	return(myErr);
}


//////////
//
// QTDX_SaveExporterSettingsInFile
// Get the current settings of the specified movie exporter and save them into a file.
//
//////////

OSErr QTDX_SaveExporterSettingsInFile (MovieExportComponent theExporter, FSSpecPtr theFSSpecPtr)
{	
	QTAtomContainer		myContainer = NULL;
	ComponentResult		myErr = noErr;
		
	myErr = MovieExportGetSettingsAsAtomContainer(theExporter, &myContainer);
	if (myErr != noErr)
		goto bail;
		
	myErr = QTDX_WriteHandleToFile((Handle)myContainer, theFSSpecPtr);

bail:
	if (myContainer != NULL)
		QTDisposeAtomContainer(myContainer);
		
	return((OSErr)myErr);
}


//////////
//
// QTDX_GetExporterSettingsFromFile
// Read the movie exporter settings saved in the specified file.
//
//////////

OSErr QTDX_GetExporterSettingsFromFile (MovieExportComponent theExporter, FSSpecPtr theFSSpecPtr)
{	
	Handle				myHandle = NULL;
	ComponentResult		myErr = fnfErr;		// assume we cannot find the file
		
	myHandle = QTDX_ReadHandleFromFile(theFSSpecPtr);
	if (myHandle == NULL)
		goto bail;
		
	myErr = MovieExportSetSettingsFromAtomContainer(theExporter, (QTAtomContainer)myHandle);
		
bail:
	if (myHandle != NULL)
		DisposeHandle(myHandle);
		
	return((OSErr)myErr);
}


//////////
//
// QTDX_WriteHandleToFile
// Write the data in the specified handle into the specified file;
// if the file already exists, it is overwritten.
//
//////////

OSErr QTDX_WriteHandleToFile (Handle theHandle, FSSpecPtr theFSSpecPtr)
{
	short			myRefNum = 0;
	long			mySize = 0;
	OSErr			myErr = paramErr;
#if TARGET_OS_MAC	
	short			myVolNum;
#endif	

	if (theHandle == NULL)
		goto bail;

	mySize = GetHandleSize(theHandle);
	if (mySize == 0)
		goto bail;

	HLock(theHandle);
	
	// delete the file;
	// if it doesn't exist yet, we'll get an error (fnfErr), which we just ignore
	myErr = FSpDelete(theFSSpecPtr);
	
	// create and open the file
	myErr = FSpCreate(theFSSpecPtr, kSettingsFileCreator, kSettingsFileType, smSystemScript);

	if (myErr == noErr)
		myErr = FSpOpenDF(theFSSpecPtr, fsRdWrPerm, &myRefNum);
	
	// position the file mark to the beginning of the file and write the data
	if (myErr == noErr)
		myErr = SetFPos(myRefNum, fsFromStart, 0);

	if (myErr == noErr)
		myErr = FSWrite(myRefNum, &mySize, *theHandle);

	if (myErr == noErr)
		myErr = SetFPos(myRefNum, fsFromStart, mySize);

	// resize the file to the number of bytes written
	if (myErr == noErr)
		myErr = SetEOF(myRefNum, mySize);
				
	// close the file			 
	if (myErr == noErr)		
		myErr = FSClose(myRefNum);

#if TARGET_OS_MAC	
	// flush the volume
	if (myErr == noErr)		
		myErr = GetVRefNum(myRefNum, &myVolNum);

	if (myErr == noErr)		
		myErr = FlushVol(NULL, myVolNum);
#endif	

bail:
	HUnlock(theHandle);

	return(myErr);
}


//////////
//
// QTDX_ReadHandleFromFile
// Read the data in the specified file into a new handle.
//
//////////

Handle QTDX_ReadHandleFromFile (FSSpecPtr theFSSpecPtr)
{
	Handle			myHandle = NULL;
	short			myRefNum = 0;
	long			mySize = 0;
	OSErr			myErr = noErr;

	// open the file
	myErr = FSpOpenDF(theFSSpecPtr, fsRdWrPerm, &myRefNum);
	
	if (myErr == noErr)
		myErr = SetFPos(myRefNum, fsFromStart, 0);

	// get the size of the file data
	if (myErr == noErr)
		myErr = GetEOF(myRefNum, &mySize);
		
	// allocate a new handle
	if (myErr == noErr)
		myHandle = NewHandleClear(mySize);
	
	if (myHandle == NULL)
		goto bail;

	// read the data from the file into the handle
	if (myErr == noErr)
		myErr = FSRead(myRefNum, &mySize, *myHandle);

bail:
	if (myRefNum != 0)		
		FSClose(myRefNum);

	return(myHandle);
}


//////////
//
// QTDX_EstimateRemainingTime
// Estimate the amount of time remaining in the operation.
//
//////////

void QTDX_EstimateRemainingTime (Rect *theRect, Fixed thePercentDone, UInt32 theTicksElapsed)
{
	char 			myString[32];
	Fixed			myEstTicks;
	UInt32			myRemSeconds;
	Rect			myEraseRect;
	StringPtr		myPString = NULL;
	
	myEstTicks = FixMul(FixDiv(fixed1, thePercentDone), Long2Fix(theTicksElapsed));
	myRemSeconds = Fix2Long(FixDiv(myEstTicks - Long2Fix(theTicksElapsed), Long2Fix(60)));

	TextSize(kTimeRemainingLabelSize);
	TextFont(1);

	myPString = QTUtils_ConvertCToPascalString(kTimeRemainingLabel);

	MoveTo(theRect->left, theRect->bottom);
	DrawString(myPString);
	
	MacSetRect(&myEraseRect,
				theRect->left + StringWidth(myPString),
				theRect->top,
				theRect->right,
				theRect->bottom);
				
	free(myPString);
	
	EraseRect(&myEraseRect);
	MoveTo(myEraseRect.left, myEraseRect.bottom);
	
	// the early percentages give inaccurate estimates, so don't start displaying the
	// time until we've reached a minimum threshold
	if (thePercentDone < kMinimumUsefulPercent)
		return;
	
	if (myRemSeconds == 1)
		sprintf_s(myString, 32,"%u second", myRemSeconds);
	else
		sprintf_s(myString, 32,"%u seconds", myRemSeconds);
		
	myPString = QTUtils_ConvertCToPascalString(myString);
	DrawString(myPString);
	
	free(myPString);
}
