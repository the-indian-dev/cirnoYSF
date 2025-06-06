#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <time.h>

#include <ysclass.h>
#include <ysport.h>

#include <fsgui.h>
#include <fsguifiledialog.h>

#include <fsdialog.h>
#include <fsworld.h>
#include <fsfilename.h>

#include <fstextresource.h>

#include "fsrunloop.h"
#include "fsmenu.h"
#include "fsguicommondialog.h"
#include "fsguimaincanvas.h"



void FsGuiMainCanvas::File_Open(FsGuiPopUpMenuItem *)
{
	auto world=runLoop->GetWorld();
	if(world->SimulationIsPrepared()==YSTRUE)
	{
		auto dlg=StartConfirmDeleteFlightDialog((int)YSOK);
		dlg->BindCloseModalCallBack(&THISCLASS::File_Open_DeletedConfirmed,this);
		return;
	}
	File_Open_DeletedConfirmed(nullptr,(int)YSOK);
}

void FsGuiMainCanvas::File_Open_DeletedConfirmed(FsGuiDialog *,int returnCode)
{
	if(returnCode!=(int)YSOK)
	{
		return;
	}

	auto world=runLoop->GetWorld();
	if(world->SimulationIsPrepared()==YSTRUE)
	{
		world->TerminateSimulation();
	}

	YsWString defFn;
	defFn.Set(FsGetUserYsflightDir());
	defFn.Append(L"/*.yfs");

	auto fdlg=FsGuiDialog::CreateSelfDestructiveDialog <FsGuiFileDialog>();
	fdlg->Initialize();
	fdlg->mode=FsGuiFileDialog::MODE_OPEN;
	fdlg->title.Set(L"Open Flight");
	fdlg->fileExtensionArray.Append(L".yfs");
	fdlg->defaultFileName.Set(defFn);
	fdlg->BindCloseModalCallBack(&THISCLASS::File_Open_FileSelected,this);
	AttachModalDialog(fdlg);
}

void FsGuiMainCanvas::File_Open_FileSelected(FsGuiDialog *closedDialog,int returnCode)
{
	auto fdlg=dynamic_cast <FsGuiFileDialog *>(closedDialog);
	if(nullptr!=fdlg && (int)YSOK==returnCode && 0<fdlg->selectedFileArray.GetN())
	{
		printf("[%ls]\n",fdlg->selectedFileArray[0].Txt());

		auto world=runLoop->GetWorld();
		YSRESULT res=world->Load(fdlg->selectedFileArray[0]);
		if(res==YSOK)
		{
			mainMenu->AddRecentlyUsedFile(fdlg->selectedFileArray[0]);
		}
		else
		{
			StartMessageBox(
			    FSGUI_COMMON_ERROR,
			    FSGUI_FILEDLG_CANNOT_LOAD_FILE,
			    FSGUI_COMMON_OK,NULL,0,0);
		}
	}
}

////////////////////////////////////////////////////////////

void FsGuiMainCanvas::File_Save(FsGuiPopUpMenuItem *)
{
	auto world=runLoop->GetWorld();
	if(world->SimulationIsPrepared()!=YSTRUE)
	{
		StartMessageBox(
		    FSGUI_COMMON_ERROR,
		    FSERRMSG_NEEDCREATEFLIGHT,
		    FSGUI_COMMON_OK,NULL,0,0);
		return;
	}

	YsWString defFn;
	defFn.Set(FsGetUserYsflightDir());
	defFn.Append(L"/untitled.yfs");

	auto fdlg=FsGuiDialog::CreateSelfDestructiveDialog<FsGuiFileDialog>();
	fdlg->Initialize();
	fdlg->mode=FsGuiFileDialog::MODE_SAVE;
	fdlg->title.Set(L"Save Flight");
	fdlg->fileExtensionArray.Append(L".yfs");
	fdlg->defaultFileName.Set(defFn);
	fdlg->BindCloseModalCallBack(&THISCLASS::File_SaveStep2,this);
	AttachModalDialog(fdlg);
}

void FsGuiMainCanvas::File_SaveStep2(FsGuiDialog *closedDialog,int returnCode)
{
	auto fdlg=dynamic_cast<FsGuiFileDialog *>(closedDialog);
	if(nullptr!=fdlg && (int)YSOK==returnCode && 0<fdlg->selectedFileArray.GetN())
	{
		printf("[%ls]\n",fdlg->selectedFileArray[0].Txt());

		const YsWString ful=fdlg->selectedFileArray[0];
		printf("[%ls]\n",ful.Txt());

		YsFileIO::File fp(ful,"rb");
		if(NULL==fp)
		{
			File_Save_Save(ful);
		}
		else
		{
			auto msgDlg=FsGuiDialog::CreateSelfDestructiveDialog <FsGuiMessageBoxDialogWithPayload <YsWString> >();
			msgDlg->payload=fdlg->selectedFileArray[0];
			msgDlg->Make(
			    L"",
			    FSGUI_FILEDLG_ASK_OVERWRITE,
			    FSGUI_COMMON_YES,FSGUI_COMMON_NO);
			msgDlg->BindCloseModalCallBack(&THISCLASS::File_SaveStep3,this);
			AttachModalDialog(msgDlg);
		}
	}
}

void FsGuiMainCanvas::File_SaveStep3(FsGuiDialog *closedDialog,int returnCode)
{
	auto msgDlg=dynamic_cast <FsGuiMessageBoxDialogWithPayload <YsWString> *>(closedDialog);
	if(nullptr!=msgDlg && (int)YSOK==returnCode)
	{
		File_Save_Save(msgDlg->payload);
	}
}

void FsGuiMainCanvas::File_Save_Save(YsWString ful)
{
	auto world=runLoop->GetWorld();
	if(world->Save(ful,3,4,2,2,2,2,0.0)==YSOK)
	{
		StartMessageBox(
		    L"",
		    FSGUI_FILEDLG_SAVED,
		    FSGUI_COMMON_CLOSE,NULL,
		    0,0);
	}
	else
	{
		StartMessageBox(
		    FSGUI_COMMON_ERROR,
		    FSGUI_FILEDLG_CANNOT_WRITE_FILE,
		    FSGUI_COMMON_CLOSE,NULL,
		    0,0);
	}
}

////////////////////////////////////////////////////////////

void FsGuiMainCanvas::File_OpenMission(FsGuiPopUpMenuItem *)
{
	auto world=runLoop->GetWorld();
	if(world->SimulationIsPrepared()==YSTRUE)
	{
		auto dlg=StartConfirmDeleteFlightDialog((int)YSOK);
		dlg->BindCloseModalCallBack(&THISCLASS::File_OpenMission_DeleteConfirmed,this);
		return;
	}
	File_OpenMission_DeleteConfirmed(nullptr,(int)YSOK);
}

void FsGuiMainCanvas::File_OpenMission_DeleteConfirmed(FsGuiDialog *,int returnCode)
{
	if(returnCode!=(int)YSOK)
	{
		return;
	}

	auto world=runLoop->GetWorld();
	if(world->SimulationIsPrepared()==YSTRUE)
	{
		world->TerminateSimulation();
	}

	auto dlg=StartSelectMissionDialog();
	dlg->BindCloseModalCallBack(&THISCLASS::File_OpenMission_MissionSelected,this);
}

void FsGuiMainCanvas::File_OpenMission_MissionSelected(FsGuiDialog *dlg,int returnCode)
{
	auto openMissionDialog=dynamic_cast <FsGuiSelectMissionDialog *>(dlg);

	if(nullptr==openMissionDialog)
	{
		printf("Fatal internal error!\n");
		printf("%s %d\n",__FUNCTION__,__LINE__);
		exit(1);
	}

	YSBOOL flyNow=YSFALSE;
	if(openMissionDialog->lastClicked==openMissionDialog->okBtn)
	{
		flyNow=YSFALSE;
	}
	else if(openMissionDialog->lastClicked==openMissionDialog->flyNowBtn)
	{
		flyNow=YSTRUE;
	}
	else // if(openMissionDialog->lastClicked==openMissionDialog->cancelBtn)
	{
		return;
	}

	YsWString missionSel;
	if(YSOK==openMissionDialog->GetSelectedMission(missionSel))
	{
		YsWString ful;
		ful.Set(L"mission/");
		ful.Append(missionSel);

		YsString cFul;
		cFul.EncodeUTF8 <wchar_t> (ful);
		printf("[%s]\n",cFul.Txt());

		auto world=runLoop->GetWorld();
		if(YSOK==world->Load(ful))
		{
			if(flyNow==YSTRUE)
			{
				Sim_Fly(nullptr);
			}
		}
		else
		{
			StartMessageBox(
			    L"ERROR",
			    L"Cannot Load File",
			    FSGUI_COMMON_OK,NULL,0,0);
		}
	}
}

////////////////////////////////////////////////////////////

void FsGuiMainCanvas::File_OpenPrevFlight(FsGuiPopUpMenuItem *)
{
	auto world=runLoop->GetWorld();
	if(world->SimulationIsPrepared()==YSTRUE)
	{
		auto dlg=StartConfirmDeleteFlightDialog((int)YSOK);
		dlg->BindCloseModalCallBack(&THISCLASS::File_OpenPrevFlight_DeleteConfirmed,this);
		return;
	}
	File_OpenPrevFlight_DeleteConfirmed(nullptr,(int)YSOK);
}

void FsGuiMainCanvas::File_OpenPrevFlight_DeleteConfirmed(FsGuiDialog *,int returnCode)
{
	if((int)YSOK!=returnCode)
	{
		return;
	}

	YsString versionStr;
	{
		YsFileIO::File fp(FsGetPrevFlightFile(),"r");
		if(fp!=NULL)
		{
			versionStr.Fgets(fp);
		}
		else
		{
			StartMessageBox(
			    L"ERROR",
			    FSERRMSG_NOPREVFLIGHT,
			    FSGUI_COMMON_OK,NULL,0,0);
			return;
		}
	}

	if(YSTRUE==versionStr.DOESSTARTWITH("YFSVERSI"))
	{
		auto world=runLoop->GetWorld();
		world->TerminateSimulation();

		auto res=world->Load(FsGetPrevFlightFile());

		if(res!=YSOK)
		{
			StartMessageBox(
			    L"Error",
			    L"Failed to load the previous mission.",
			    FSGUI_COMMON_OK,NULL,0,0);
		}
	}
	else if(YSTRUE==versionStr.DOESSTARTWITH("ENDURANCE") ||
	        YSTRUE==versionStr.DOESSTARTWITH("INTERCEPT") ||
	        YSTRUE==versionStr.DOESSTARTWITH("CLSAIRSPT"))
	{
		StartMessageBox(
		    L"Error",
		    L"Endurance Mode and Intercept Mission cannot be loaded.\n"
		    L"Use \"Flight\"->\"Retry Previous Mission\"",
		    FSGUI_COMMON_OK,NULL,0,0);
		return;
	}
	else
	{
		StartMessageBox(
		    L"ERROR",
		    FSERRMSG_NOPREVFLIGHT,
		    FSGUI_COMMON_OK,NULL,0,0);
	}
}

////////////////////////////////////////////////////////////

void FsGuiMainCanvas::File_CloseFlight(FsGuiPopUpMenuItem *)
{
	auto world=runLoop->GetWorld();
	if(world->SimulationIsPrepared()==YSTRUE)
	{
		auto dlg=StartConfirmDeleteFlightDialog((int)YSOK);
		dlg->BindCloseModalCallBack(&THISCLASS::File_CloseFlight_DeleteConfirmed,this);
		return;
	}
	File_CloseFlight_DeleteConfirmed(nullptr,(int)YSOK);
}

void FsGuiMainCanvas::File_CloseFlight_DeleteConfirmed(FsGuiDialog *,int returnCode)
{
	if((int)YSOK!=returnCode)
	{
		return;
	}

	auto world=runLoop->GetWorld();
	if(world->SimulationIsPrepared()==YSTRUE)
	{
		world->TerminateSimulation();
	}

	world->UnprepareAllTemplate();
	YsShell::CleanUpVertexPolygonStore();
}

////////////////////////////////////////////////////////////

void FsGuiMainCanvas::File_Exit(FsGuiPopUpMenuItem *)
{
	auto world=runLoop->GetWorld();
	if(world->SimulationIsPrepared()==YSTRUE)
	{
		auto dlg=StartConfirmDeleteFlightDialog((int)YSOK);
		dlg->BindCloseModalCallBack(&THISCLASS::File_Exit_DeleteConfirmed,this);
		return;
	}
	File_Exit_DeleteConfirmed(nullptr,(int)YSOK);
}

void FsGuiMainCanvas::File_Exit_DeleteConfirmed(FsGuiDialog *,int returnCode)
{
	if((int)YSOK==returnCode)
	{
		auto dlg=StartMessageBox(
		    L"",
		    FSGUI_EXITDLG_MESSAGE,
		    FSGUI_EXITDLG_EXIT,FSGUI_EXITDLG_BACKTOMENU,
		    (int)YSOK,(int)YSERR);
		dlg->BindCloseModalCallBack(&THISCLASS::File_Exit_ReallyExit,this);
	}
}

void FsGuiMainCanvas::File_Exit_ReallyExit(FsGuiDialog *,int returnCode)
{
	if((int)YSOK==returnCode)
	{
		runLoop->SetTerminateFlag(YSTRUE);
	}
}

////////////////////////////////////////////////////////////

void FsGuiMainCanvas::File_Recent(FsGuiPopUpMenuItem *itm)
{
	const wchar_t *wfn=itm->GetString();

	auto world=runLoop->GetWorld();
	if(world->SimulationIsPrepared()==YSTRUE)
	{
		auto dlg=StartConfirmDeleteFlightDialog((int)YSOK);
		dlg->BindCloseModalCallBack(&THISCLASS::File_Recent_DeleteConfirmed,this);
		dlg->whatToOpenNext=wfn;
		return;
	}

	File_Recent_Load(wfn);
}

void FsGuiMainCanvas::File_Recent_DeleteConfirmed(FsGuiDialog *closedDialog,int returnCode)
{
	auto dlg=dynamic_cast <FsGuiConfirmDeleteFlightDialog *>(closedDialog);
	if((int)YSOK==returnCode && nullptr!=dlg)
	{
		File_Recent_Load(dlg->whatToOpenNext);
	}
}

void FsGuiMainCanvas::File_Recent_Load(const YsWString &fn)
{
printf("%s %d\n",__FUNCTION__,__LINE__);
printf("%ls\n",fn.Txt());
	auto world=runLoop->GetWorld();

	if(world->SimulationIsPrepared()==YSTRUE)
	{
		world->TerminateSimulation();
	}

	YSRESULT res=world->Load(fn);

	if(res==YSOK)
	{
		mainMenu->AddRecentlyUsedFile(fn);
	}
	else
	{
		StartMessageBox(
		    FSGUI_COMMON_ERROR,
		    FSGUI_FILEDLG_CANNOT_LOAD_FILE,
		    FSGUI_COMMON_OK,NULL,0,0);
	}
}

////////////////////////////////////////////////////////////

void FsGuiMainCanvas::InstallAddon(void)
{
	// First show requirements dialog
	YsWString requirementsMsg = L"Add-on Installation Requirements:\n\n";
#ifdef _WIN32
	requirementsMsg.Append(L"For best compatibility, please install 7-Zip from:\nhttps://www.7-zip.org/\n\n");
	requirementsMsg.Append(L"Supported formats:\n");
	requirementsMsg.Append(L"• ZIP files (Windows 10+ built-in or 7-Zip)\n");
	requirementsMsg.Append(L"• 7Z files (requires 7-Zip)\n");
	requirementsMsg.Append(L"• RAR files (requires 7-Zip or WinRAR)\n\n");
#else
	requirementsMsg.Append(L"Please ensure these tools are installed:\n");
	requirementsMsg.Append(L"• unzip (for ZIP files)\n");
	requirementsMsg.Append(L"• p7zip (for 7Z files)\n");
	requirementsMsg.Append(L"• unrar (for RAR files)\n\n");
#endif
	requirementsMsg.Append(L"Continue to file selection?");

	auto reqDialog = StartMessageBox(
	    L"Add-on Installation",
	    requirementsMsg.data(),
	    FSGUI_COMMON_YES, FSGUI_COMMON_NO,
	    (int)YSOK, (int)YSERR);
	reqDialog->BindCloseModalCallBack(&THISCLASS::InstallAddon_RequirementsChecked, this);
}

void FsGuiMainCanvas::InstallAddon_RequirementsChecked(FsGuiDialog *, int returnCode)
{
	if((int)YSOK == returnCode)
	{
		auto fdlg=FsGuiDialog::CreateSelfDestructiveDialog <FsGuiFileDialog>();
		fdlg->Initialize();
		fdlg->mode=FsGuiFileDialog::MODE_OPEN;
		fdlg->title.Set(L"Select Add-on Archive");
		fdlg->fileExtensionArray.Append(L".zip");
		fdlg->fileExtensionArray.Append(L".7z");
		fdlg->fileExtensionArray.Append(L".rar");
		fdlg->multiSelect=YSFALSE;
		fdlg->BindCloseModalCallBack(&THISCLASS::InstallAddon_FileSelected,this);
		AttachModalDialog(fdlg);
	}
}

YsWString FsGuiMainCanvas::JoinPath(const YsWString &dir, const YsWString &file)
{
	YsWString result = dir;
#ifdef _WIN32
	if(result.Strlen() > 0 && result[result.Strlen()-1] != L'\\')
	{
		result.Append(L"\\");
	}
#else
	if(result.Strlen() > 0 && result[result.Strlen()-1] != L'/')
	{
		result.Append(L"/");
	}
#endif
	result.Append(file);
	return result;
}

void FsGuiMainCanvas::InstallAddon_FileSelected(FsGuiDialog *closedDialog, int returnCode)
{
	auto fdlg=dynamic_cast <FsGuiFileDialog *>(closedDialog);
	if(nullptr!=fdlg && (int)YSOK==returnCode && 0<fdlg->selectedFileArray.GetN())
	{
		YsWString selectedFile=fdlg->selectedFileArray[0];
		printf("Add-on file selected: [%ls]\n",selectedFile.Txt());

		// Start installation process
		InstallAddon_ProcessArchive(selectedFile);
	}
}

void FsGuiMainCanvas::InstallAddon_ProcessArchive(const YsWString &archivePath)
{
	// Create temporary directory for extraction
	YsWString tempDir;
	//YsSpecialPath::GetUserDir(tempDir);
	//tempDir = JoinPath(tempDir, L"ysf_addon_temp");
	// We create a temporary directory in the current working directory
	YsWString ysflightDir;
	YsSpecialPath::GetProgramBaseDir(ysflightDir);
	tempDir = JoinPath(ysflightDir, L"ysf_addon_temp");

	// Remove old temp directory if exists
	YsFileIO::Remove(tempDir.data());

	// Create temp directory with better error handling
	printf("Creating temp directory: %ls\n", tempDir.data());

	// Check if user directory exists first
	YsWString userDir;
	YsSpecialPath::GetUserDir(userDir);
	printf("User directory: %ls\n", userDir.data());

	// Try to create directory and verify it exists
	YsFileIO::MkDir(tempDir.data());
	if(YSFALSE == YsFileIO::CheckFileExist(tempDir.data()))
	{
		printf("Failed to create temp directory in user dir\n");
		// Try alternative temp directory
		YsWString altTempDir;
#ifdef _WIN32
		altTempDir.Set(L"C:\\Temp\\ysf_addon_temp");
		// Also try creating C:\Temp first
		YsFileIO::MkDir(L"C:\\Temp");
#else
		altTempDir.Set(L"/tmp/ysf_addon_temp");
#endif
		printf("Trying alternative temp directory: %ls\n", altTempDir.data());
		YsFileIO::Remove(altTempDir.data());
		YsFileIO::MkDir(altTempDir.data());

		if(YSFALSE == YsFileIO::CheckFileExist(altTempDir.data()))
		{
			printf("Failed to create alternative temp directory\n");
			// Try one more simple approach - use current directory
			YsWString simpleTempDir = L"ysf_addon_temp";
			YsFileIO::Remove(simpleTempDir.data());
			YsFileIO::MkDir(simpleTempDir.data());

			if(YSFALSE == YsFileIO::CheckFileExist(simpleTempDir.data()))
			{
				YsWString errorMsg = L"Failed to create temporary directory for extraction.\n\n";
				errorMsg.Append(L"Tried locations:\n");
				errorMsg.Append(tempDir);
				errorMsg.Append(L"\n");
				errorMsg.Append(altTempDir);
				errorMsg.Append(L"\n");
				errorMsg.Append(simpleTempDir);
				errorMsg.Append(L"\n\nPlease check write permissions.");
				StartMessageBox(L"Error", errorMsg.data(),
				               FSGUI_COMMON_OK, NULL, 0, 0);
				return;
			}
			tempDir = simpleTempDir;
		}
		else
		{
			tempDir = altTempDir;
		}
	}
	printf("Using temp directory: %ls\n", tempDir.data());

	// Extract archive based on extension
	YsWString ext = YsWString(archivePath).GetExtension();
	ext.Capitalize();

	YSBOOL extractSuccess = YSFALSE;
	if(0 == ext.Strcmp(L".ZIP"))
	{
		extractSuccess = ExtractZip(archivePath, tempDir);
	}
	else if(0 == ext.Strcmp(L".7Z"))
	{
		extractSuccess = Extract7z(archivePath, tempDir);
	}
	else if(0 == ext.Strcmp(L".RAR"))
	{
		extractSuccess = ExtractRar(archivePath, tempDir);
	}

	if(YSTRUE != extractSuccess)
	{
		YsFileIO::Remove(tempDir.data());
		YsWString errorMsg = L"Failed to extract archive.\n\n";
#ifdef _WIN32
		if(0 == ext.Strcmp(L".ZIP"))
		{
			errorMsg.Append(L"For ZIP files: Windows 10+ should work automatically.\nFor older Windows, install 7-Zip from https://www.7-zip.org/");
		}
		else if(0 == ext.Strcmp(L".7Z"))
		{
			errorMsg.Append(L"For 7Z files: Please install 7-Zip from https://www.7-zip.org/");
		}
		else if(0 == ext.Strcmp(L".RAR"))
		{
			errorMsg.Append(L"For RAR files: Please install 7-Zip (https://www.7-zip.org/) or WinRAR (https://www.rarlab.com/)");
		}
#else
		errorMsg.Append(L"Please install the required extraction tools:\n- unzip (for ZIP files)\n- p7zip (for 7Z files)\n- unrar (for RAR files)");
#endif
		StartMessageBox(L"Extraction Failed", errorMsg.data(), FSGUI_COMMON_OK, NULL, 0, 0);
		return;
	}

	// Find addon directory containing user folder
	YsWString addonDir;
	printf("About to search for addon directory in: %ls\n", tempDir.data());
	if(YSOK != FindAddonDirectory(tempDir, addonDir))
	{
		YsFileIO::Remove(tempDir.data());
		StartMessageBox(L"Error", L"No valid add-on found. Add-on must contain a 'user' folder.",
		               FSGUI_COMMON_OK, NULL, 0, 0);
		return;
	}
	printf("Found addon directory: %ls\n", addonDir.data());

	// Install addon
	YsArray<YsWString> replacedFiles;
	if(YSOK == InstallAddonFiles(addonDir, replacedFiles))
	{
		// Clean up temp directory
		YsFileIO::Remove(tempDir.data());

		// Show success message
		YsWString message = L"Add-on installed successfully!";
		if(0 < replacedFiles.GetN())
		{
			message.Append(L"\n\nReplaced files:");
			for(auto i = 0; i < replacedFiles.GetN() && i < 10; ++i)
			{
				message.Append(L"\n- ");
				message.Append(replacedFiles[i]);
			}
			if(replacedFiles.GetN() > 10)
			{
				YsString moreFilesStr;
				moreFilesStr.Printf("\n... and %d more files", (int)(replacedFiles.GetN() - 10));
				YsWString moreFilesWStr;
				moreFilesWStr.SetUTF8String(moreFilesStr.data());
				message.Append(moreFilesWStr);
			}
		}

		StartMessageBox(L"Add-on Installation", message.data(), FSGUI_COMMON_OK, NULL, 0, 0);
	}
	else
	{
		StartMessageBox(L"Error", L"Failed to install add-on files.", FSGUI_COMMON_OK, NULL, 0, 0);
	}

	// Clean up temp directory
	printf("Cleaning up temp directory: %ls\n", tempDir.data());
	YsFileIO::Remove(tempDir.data());
}

YSBOOL FsGuiMainCanvas::ExtractZip(const YsWString &zipPath, const YsWString &destDir)
{
#ifdef _WIN32
	// Try PowerShell first (Windows 10+)
	YsString command;
	YsString zipPathStr, destDirStr;
	zipPathStr.EncodeUTF8(zipPath.data());
	destDirStr.EncodeUTF8(destDir.data());
	command.Printf("powershell -Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\" 2>nul",
	               zipPathStr.data(), destDirStr.data());
	if(0 == system(command.data()))
	{
		return YSTRUE;
	}

	// Try 7z as fallback
	command.Printf("7z x \"%s\" -o\"%s\" -y >nul 2>nul", zipPathStr.data(), destDirStr.data());
	if(0 == system(command.data()))
	{
		return YSTRUE;
	}

	return YSFALSE;
#else
	// Use unzip on Linux
	YsString command;
	YsString zipPathStr, destDirStr;
	zipPathStr.EncodeUTF8(zipPath.data());
	destDirStr.EncodeUTF8(destDir.data());
	command.Printf("unzip -q -o \"%s\" -d \"%s\"", zipPathStr.data(), destDirStr.data());
	return (0 == system(command.data())) ? YSTRUE : YSFALSE;
#endif
}

YSBOOL FsGuiMainCanvas::Extract7z(const YsWString &archivePath, const YsWString &destDir)
{
	YsString command;
	YsString archivePathStr, destDirStr;
	archivePathStr.EncodeUTF8(archivePath.data());
	destDirStr.EncodeUTF8(destDir.data());

#ifdef _WIN32
	// Try 7z from Program Files first
	command.Printf("\"C:\\Program Files\\7-Zip\\7z.exe\" x \"%s\" -o\"%s\" -y >nul 2>nul",
	               archivePathStr.data(), destDirStr.data());
	if(0 == system(command.data()))
	{
		return YSTRUE;
	}

	// Try 7z from PATH
	command.Printf("7z x \"%s\" -o\"%s\" -y >nul 2>nul", archivePathStr.data(), destDirStr.data());
	return (0 == system(command.data())) ? YSTRUE : YSFALSE;
#else
	// Use 7z command line tool on Linux
	command.Printf("7z x \"%s\" -o\"%s\" -y", archivePathStr.data(), destDirStr.data());
	return (0 == system(command.data())) ? YSTRUE : YSFALSE;
#endif
}

YSBOOL FsGuiMainCanvas::ExtractRar(const YsWString &archivePath, const YsWString &destDir)
{
	YsString command;
	YsString archivePathStr, destDirStr;
	archivePathStr.EncodeUTF8(archivePath.data());
	destDirStr.EncodeUTF8(destDir.data());

#ifdef _WIN32
	// Try 7z first (can handle RAR files)
	command.Printf("\"C:\\Program Files\\7-Zip\\7z.exe\" x \"%s\" -o\"%s\" -y >nul 2>nul",
	               archivePathStr.data(), destDirStr.data());
	if(0 == system(command.data()))
	{
		return YSTRUE;
	}

	// Try 7z from PATH
	command.Printf("7z x \"%s\" -o\"%s\" -y >nul 2>nul", archivePathStr.data(), destDirStr.data());
	if(0 == system(command.data()))
	{
		return YSTRUE;
	}

	// Try WinRAR unrar
	command.Printf("\"C:\\Program Files\\WinRAR\\unrar.exe\" x \"%s\" \"%s\\\" -y >nul 2>nul",
	               archivePathStr.data(), destDirStr.data());
	if(0 == system(command.data()))
	{
		return YSTRUE;
	}

	// Try unrar from PATH
	command.Printf("unrar x \"%s\" \"%s/\" -y >nul 2>nul", archivePathStr.data(), destDirStr.data());
	return (0 == system(command.data())) ? YSTRUE : YSFALSE;
#else
	// Use unrar command line tool on Linux
	command.Printf("unrar x \"%s\" \"%s/\" -y", archivePathStr.data(), destDirStr.data());
	return (0 == system(command.data())) ? YSTRUE : YSFALSE;
#endif
}

YSRESULT FsGuiMainCanvas::FindAddonDirectory(const YsWString &tempDir, YsWString &addonDir)
{
	// Search for directory containing 'user' folder
	YsArray <YsWString> dirsToCheck;
	dirsToCheck.Append(tempDir);

	printf("Starting search for user folder in: %ls\n", tempDir.data());
	printf("Initial dirs to check: %d\n", (int)dirsToCheck.GetN());

	// First check the root temp directory directly
	YsFileList rootFileList;
	printf("Checking root temp directory for files...\n");
	if(YSOK == rootFileList.FindFileList(tempDir.data(), NULL, NULL))
	{
		printf("Root directory has %d items:\n", (int)rootFileList.GetN());
		for(YSSIZE_T i = 0; i < rootFileList.GetN(); ++i)
		{
			YsWString baseFileName = rootFileList.GetBaseNameW(i);
			YsWString ext = baseFileName.GetExtension();
			ext.Capitalize();
			YsWString lstType = L"none";
			YSBOOL isLst = YSFALSE;

			if(0 == ext.Strcmp(L".LST"))
			{
				isLst = YSTRUE;
				if(0 == baseFileName.STRNCMP(L"air", 3))
				{
					lstType = L"air";
				}
				else if(0 == baseFileName.STRNCMP(L"sce", 3))
				{
					lstType = L"sce";
				}
				else if(0 == baseFileName.STRNCMP(L"gro", 3))
				{
					lstType = L"gro";
				}
			}

			printf("  %d: %ls (isDir: %s, isLst: %s, LstType: %ls)\n", (int)i, baseFileName.data(),
			       rootFileList.IsDirectory(i) ? "yes" : "no", isLst ? "yes" : "no", lstType.data());
		}
	}
	else
	{
		printf("Failed to list root temp directory\n");
	}

	while(0 < dirsToCheck.GetN())
	{
		YsWString currentDir = dirsToCheck[0];
		dirsToCheck.DeleteBySwapping(0);

		printf("Checking directory: %ls\n", currentDir.data());

		YsFileList fileList;
		if(YSOK == fileList.FindFileList(currentDir.data(), NULL, NULL))
		{
			YSBOOL hasUserFolder = YSFALSE;
			printf("Found %d items in directory\n", (int)fileList.GetN());

			// First pass: look for user folder
			for(YSSIZE_T i = 0; i < fileList.GetN(); ++i)
			{
				YsWString baseFileName = fileList.GetBaseNameW(i);
				YsWString ext = baseFileName.GetExtension();
				ext.Capitalize();
				YsWString lstType = L"none";
				YSBOOL isLst = YSFALSE;

				if(0 == ext.Strcmp(L".LST"))
				{
					isLst = YSTRUE;
					if(0 == baseFileName.STRNCMP(L"air", 3))
					{
						lstType = L"air";
					}
					else if(0 == baseFileName.STRNCMP(L"sce", 3))
					{
						lstType = L"sce";
					}
					else if(0 == baseFileName.STRNCMP(L"gro", 3))
					{
						lstType = L"gro";
					}
				}

				printf("Item %d: %ls (isDir: %s, isLst: %s, LstType: %ls)\n", (int)i, baseFileName.data(),
				       fileList.IsDirectory(i) ? "yes" : "no", isLst ? "yes" : "no", lstType.data());

				if(YSTRUE == fileList.IsDirectory(i))
				{
					if(0 == baseFileName.STRCMP(L"user"))  // Case-insensitive comparison
					{
						hasUserFolder = YSTRUE;
						printf("Found user folder (case-insensitive) in: %ls\n", currentDir.data());
						addonDir = currentDir;
						printf("Setting addon directory to: %ls\n", addonDir.data());
						return YSOK;
					}
				}
			}

			// Second pass: add subdirectories to search list (but don't go too deep)
			if(dirsToCheck.GetN() < 20) // Prevent infinite recursion
			{
				for(YSSIZE_T i = 0; i < fileList.GetN(); ++i)
				{
					if(YSTRUE == fileList.IsDirectory(i))
					{
						YsWString baseFileName = fileList.GetBaseNameW(i);
						if(0 != baseFileName.Strcmp(L".") && 0 != baseFileName.Strcmp(L".."))
						{
							YsWString subDir = JoinPath(currentDir, baseFileName);
							dirsToCheck.Append(subDir);
							printf("Added subdirectory to search: %ls\n", subDir.data());
						}
					}
				}
			}
			else
			{
				printf("Reached maximum search depth, stopping recursion\n");
			}
		}
		else
		{
			printf("Failed to list files in directory: %ls\n", currentDir.data());
		}
		printf("Remaining directories to check: %d\n", (int)dirsToCheck.GetN());
	}

	printf("No user folder found in any directory\n");
	return YSERR;
}

YSRESULT FsGuiMainCanvas::InstallAddonFiles(const YsWString &addonDir, YsArray<YsWString> &replacedFiles)
{
	// Get YSFlight installation directory (same as executable directory)
	YsWString ysflightDir;
	YsSpecialPath::GetProgramBaseDir(ysflightDir);
	printf("YSFlight directory: %ls\n", ysflightDir.data());

	// First, handle .lst files with prefixes and move them to appropriate directories
	if(YSOK != ProcessLstFiles(addonDir, ysflightDir, replacedFiles))
	{
		printf("ERROR: Failed to process .lst files\n");
		return YSERR;
	}

	// First, find the actual user folder name (case-sensitive preservation)
	YsWString actualUserFolderName;
	YSBOOL hasUserFolder = YSFALSE;
	YsFileList addonFileList;
	if(YSOK == addonFileList.FindFileList(addonDir.data(), NULL, NULL))
	{
		for(YSSIZE_T j = 0; j < addonFileList.GetN(); ++j)
		{
			if(YSTRUE == addonFileList.IsDirectory(j))
			{
				YsWString baseName = addonFileList.GetBaseNameW(j);
				if(0 == baseName.STRCMP(L"user"))  // Case-insensitive search
				{
					actualUserFolderName = baseName;  // Preserve exact case
					hasUserFolder = YSTRUE;
					printf("Found user folder with exact case: %ls\n", baseName.data());
					break;
				}
			}
		}
	}

	// Folders to check and copy
	YsArray<YsWString> folderNames;
	if(YSTRUE == hasUserFolder)
	{
		folderNames.Append(actualUserFolderName);  // Use exact case from addon
	}
	folderNames.Append(L"aircraft");
	folderNames.Append(L"scenery");
	folderNames.Append(L"ground");
	printf("Will check %d folders for installation\n", (int)folderNames.GetN());

	for(auto i = 0; i < folderNames.GetN(); ++i)
	{
		YsWString folderName = folderNames[i];
		YsWString srcFolder = JoinPath(addonDir, folderName);
		YsWString destFolder = JoinPath(ysflightDir, folderName);  // Preserve exact case

		printf("Checking folder %d: %ls\n", (int)i, folderName.data());

		printf("  Source: %ls\n", srcFolder.data());
		printf("  Destination: %ls\n", destFolder.data());

		// Check if source folder exists
		YsFileList fileList;
		if(YSOK == fileList.FindFileList(srcFolder.data(), NULL, NULL))
		{
			printf("  Source folder exists with %d items\n", (int)fileList.GetN());

			// Create destination folder if it doesn't exist
			YsFileIO::MkDir(destFolder.data());
			printf("  Created/verified destination folder\n");

			// Copy files recursively
			if(YSOK != CopyDirectoryRecursive(srcFolder, destFolder, replacedFiles))
			{
				printf("  ERROR: Failed to copy directory recursively\n");
				return YSERR;
			}
			printf("  Successfully copied folder contents\n");
		}
		else
		{
			printf("  Source folder does not exist, skipping\n");
		}
	}

	printf("InstallAddonFiles completed successfully\n");
	return YSOK;
}

YSRESULT FsGuiMainCanvas::CopyDirectoryRecursive(const YsWString &srcDir, const YsWString &destDir, YsArray<YsWString> &replacedFiles)
{
	printf("CopyDirectoryRecursive: %ls -> %ls\n", srcDir.data(), destDir.data());

	YsFileList fileList;
	if(YSOK != fileList.FindFileList(srcDir.data(), NULL, NULL))
	{
		printf("ERROR: Failed to list files in source directory: %ls\n", srcDir.data());
		return YSERR;
	}

	printf("Found %d items to copy\n", (int)fileList.GetN());

	// Ensure destination directory exists
	YsFileIO::MkDir(destDir.data());

	for(auto i = 0; i < fileList.GetN(); ++i)
	{
		YsWString baseFileName = fileList.GetBaseNameW(i);
		if(0 == baseFileName.Strcmp(L".") || 0 == baseFileName.Strcmp(L".."))
		{
			continue;
		}

		YsWString srcPath = JoinPath(srcDir, baseFileName);
		YsWString destPath = JoinPath(destDir, baseFileName);

		printf("Processing item %d: %ls\n", (int)i, baseFileName.data());

		if(YSTRUE == fileList.IsDirectory(i))
		{
			printf("  Directory - recursing\n");
			// Recursively copy subdirectory
			if(YSOK != CopyDirectoryRecursive(srcPath, destPath, replacedFiles))
			{
				printf("ERROR: Failed to copy subdirectory: %ls\n", srcPath.data());
				return YSERR;
			}
		}
		else
		{
			printf("  File - copying from %ls to %ls\n", srcPath.data(), destPath.data());

			// Copy file
			if(YSTRUE == YsFileIO::CheckFileExist(destPath.data()))
			{
				replacedFiles.Append(destPath);
				printf("  Will replace existing file\n");
			}

			if(YSOK != CopyFile(srcPath, destPath))
			{
				printf("ERROR: Failed to copy file: %ls\n", srcPath.data());
				return YSERR;
			}
			printf("  File copied successfully\n");
		}
	}

	printf("CopyDirectoryRecursive completed successfully\n");
	return YSOK;
}

YSRESULT FsGuiMainCanvas::CopyFile(const YsWString &srcPath, const YsWString &destPath)
{
	printf("CopyFile: %ls -> %ls\n", srcPath.data(), destPath.data());

	YsFileIO::File srcFile(srcPath.data(), "rb");
	if(nullptr == srcFile)
	{
		printf("ERROR: Cannot open source file for reading: %ls\n", srcPath.data());
		return YSERR;
	}

	auto data = srcFile.Fread();
	srcFile.Close();
	printf("Read %d bytes from source file\n", (int)data.GetN());

	YsFileIO::File destFile(destPath.data(), "wb");
	if(nullptr == destFile)
	{
		printf("ERROR: Cannot open destination file for writing: %ls\n", destPath.data());
		return YSERR;
	}

	if(YSOK != destFile.Fwrite(data))
	{
		printf("ERROR: Failed to write data to destination file\n");
		return YSERR;
	}

	printf("File copied successfully (%d bytes)\n", (int)data.GetN());
	return YSOK;
}

YSRESULT FsGuiMainCanvas::ProcessLstFiles(const YsWString &addonDir, const YsWString &ysflightDir, YsArray<YsWString> &replacedFiles)
{
	printf("Processing .lst files in addon directory: %ls\n", addonDir.data());

	YsFileList fileList;
	if(YSOK != fileList.FindFileList(addonDir.data(), NULL, NULL))
	{
		printf("ERROR: Failed to list files in addon directory\n");
		return YSERR;
	}

	for(YSSIZE_T i = 0; i < fileList.GetN(); ++i)
	{
		if(YSTRUE == fileList.IsDirectory(i))
		{
			continue; // Skip directories
		}

		YsWString baseFileName = fileList.GetBaseNameW(i);
		YsWString ext = baseFileName.GetExtension();
		ext.Capitalize();

		if(0 == ext.Strcmp(L".LST"))
		{
			YsWString destSubDir;
			YSBOOL shouldMove = YSFALSE;

			// Check prefix and determine destination (case-insensitive)
			if(0 == baseFileName.STRNCMP(L"air", 3))
			{
				destSubDir = L"aircraft";
				shouldMove = YSTRUE;
				printf("Found aircraft .lst file: %ls\n", baseFileName.data());
			}
			else if(0 == baseFileName.STRNCMP(L"sce", 3))
			{
				destSubDir = L"scenery";
				shouldMove = YSTRUE;
				printf("Found scenery .lst file: %ls\n", baseFileName.data());
			}
			else if(0 == baseFileName.STRNCMP(L"gro", 3))
			{
				destSubDir = L"ground";
				shouldMove = YSTRUE;
				printf("Found ground .lst file: %ls\n", baseFileName.data());
			}
			else
			{
				printf("Found .lst file with unknown prefix: %ls\n", baseFileName.data());
			}

			if(YSTRUE == shouldMove)
			{
				YsWString srcPath = JoinPath(addonDir, baseFileName);
				YsWString destDir = JoinPath(ysflightDir, destSubDir);
				YsWString destPath = JoinPath(destDir, baseFileName);

				printf("Moving .lst file:\n");
				printf("  From: %ls\n", srcPath.data());
				printf("  To: %ls\n", destPath.data());

				// Create destination directory if it doesn't exist
				YsFileIO::MkDir(destDir.data());

				// Check if destination file already exists
				if(YSTRUE == YsFileIO::CheckFileExist(destPath.data()))
				{
					replacedFiles.Append(destPath);
					printf("  Will replace existing .lst file\n");
				}

				// Copy the .lst file
				if(YSOK != CopyFile(srcPath, destPath))
				{
					printf("ERROR: Failed to copy .lst file: %ls\n", srcPath.data());
					return YSERR;
				}
				printf("  Successfully moved .lst file\n");
			}
		}
	}

	printf("ProcessLstFiles completed successfully\n");
	return YSOK;
}
