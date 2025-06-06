#ifndef FSGUIPROGRESSDIALOG_IS_INCLUDED
#define FSGUIPROGRESSDIALOG_IS_INCLUDED

#include <ysclass.h>
#include <fsgui.h>
#include "fsguidialogutil.h"

class FsGuiProgressDialog : public FsGuiDialog
{
private:
    FsGuiStatic *titleLabel;
    FsGuiStatic *currentFileLabel;
    FsGuiStatic *progressBar;
    FsGuiStatic *iconLabel;
    
    int totalFiles;
    int currentFile;
    YsArray<YsWString> failedFiles;
    
    void UpdateProgressBar();
    
public:
    FsGuiProgressDialog();
    virtual ~FsGuiProgressDialog();
    
    void Initialize();
    void Make(const wchar_t title[], const wchar_t iconText[]);
    
    void SetTotalFiles(int total);
    void SetCurrentFile(int current, const wchar_t filename[]);
    void AddFailedFile(const wchar_t filename[]);
    
    void ShowFailedFiles();
    
    virtual void OnButtonClick(FsGuiButton *btn);
};

#endif