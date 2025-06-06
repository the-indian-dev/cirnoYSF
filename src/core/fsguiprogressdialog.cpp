#include "fsguiprogressdialog.h"
#include "fsgui.h"

FsGuiProgressDialog::FsGuiProgressDialog()
{
    printf("DEBUG: FsGuiProgressDialog constructor called\n");
    titleLabel = nullptr;
    currentFileLabel = nullptr;
    progressBar = nullptr;
    iconLabel = nullptr;
    totalFiles = 0;
    currentFile = 0;
    printf("DEBUG: FsGuiProgressDialog constructor completed\n");
}

FsGuiProgressDialog::~FsGuiProgressDialog()
{
}

void FsGuiProgressDialog::Initialize()
{
    printf("DEBUG: FsGuiProgressDialog::Initialize called\n");
    FsGuiDialog::Initialize();
    failedFiles.Clear();
    totalFiles = 0;
    currentFile = 0;
    printf("DEBUG: FsGuiProgressDialog::Initialize completed\n");
}

void FsGuiProgressDialog::Make(const wchar_t title[], const wchar_t iconText[])
{
    printf("DEBUG: FsGuiProgressDialog::Make called\n");
    SetTextMessage(title);
    SetSize(400, 150);
    SetTopLeftCorner(100, 100);
    
    printf("DEBUG: Adding static text elements\n");
    iconLabel = AddStaticText(0, FSKEY_NULL, iconText, YSTRUE);
    titleLabel = AddStaticText(0, FSKEY_NULL, L"Loading Game Assets...", YSTRUE);
    currentFileLabel = AddStaticText(0, FSKEY_NULL, L"Initializing...", YSTRUE);
    progressBar = AddStaticText(0, FSKEY_NULL, L"[████████████████████████████████] 0%", YSTRUE);
    
    printf("DEBUG: Calling Fit()\n");
    Fit();
    printf("DEBUG: FsGuiProgressDialog::Make completed\n");
}

void FsGuiProgressDialog::SetTotalFiles(int total)
{
    totalFiles = total;
    currentFile = 0;
    UpdateProgressBar();
}

void FsGuiProgressDialog::SetCurrentFile(int current, const wchar_t filename[])
{
    printf("DEBUG: SetCurrentFile called with current=%d, filename=%ls\n", current, filename);
    currentFile = current;
    
    YsWString displayText;
    displayText.Set(L"Loading: ");
    displayText.Append(filename);
    if(nullptr != currentFileLabel)
    {
        currentFileLabel->SetText(displayText);
        printf("DEBUG: Updated currentFileLabel text\n");
    }
    else
    {
        printf("DEBUG: currentFileLabel is NULL!\n");
    }
    
    UpdateProgressBar();
}

void FsGuiProgressDialog::UpdateProgressBar()
{
    printf("DEBUG: UpdateProgressBar called, currentFile=%d, totalFiles=%d\n", currentFile, totalFiles);
    YsWString progressText;
    if (totalFiles > 0)
    {
        int percent = ((currentFile + 1) * 100) / totalFiles;
        int barLength = 32;
        int filledBars = ((currentFile + 1) * barLength) / totalFiles;
        
        YsWString bar;
        for (int i = 0; i < filledBars; i++)
        {
            bar.Append(L"█");
        }
        for (int i = filledBars; i < barLength; i++)
        {
            bar.Append(L"░");
        }
        
        YsString percentStr;
        percentStr.Printf(" %d/%d (%d%%)", currentFile + 1, totalFiles, percent);
        
        YsWString percentWStr;
        percentWStr.SetUTF8String(percentStr);
        
        progressText.Set(L"[");
        progressText.Append(bar);
        progressText.Append(L"]");
        progressText.Append(percentWStr);
    }
    else
    {
        progressText.Set(L"[████████████████████████████████] Preparing...");
    }
    
    if(nullptr != progressBar)
    {
        progressBar->SetText(progressText);
        printf("DEBUG: Updated progress bar text\n");
    }
    else
    {
        printf("DEBUG: progressBar is NULL!\n");
    }
}

void FsGuiProgressDialog::AddFailedFile(const wchar_t filename[])
{
    failedFiles.Append(YsWString(filename));
}

void FsGuiProgressDialog::ShowFailedFiles()
{
    if (failedFiles.GetN() > 0)
    {
        YsWString message;
        message.Set(L"The following files failed to load:\n\n");
        
        for (YSSIZE_T i = 0; i < failedFiles.GetN() && i < 10; i++)
        {
            message.Append(failedFiles[i]);
            message.Append(L"\n");
        }
        
        if (failedFiles.GetN() > 10)
        {
            YsString moreStr;
            moreStr.Printf("\n... and %d more files", (int)(failedFiles.GetN() - 10));
            
            YsWString moreText;
            moreText.SetUTF8String(moreStr);
            message.Append(moreText);
        }
        
        auto errorDlg = FsGuiDialog::CreateSelfDestructiveDialog<FsGuiMessageBoxDialog>();
        errorDlg->Make(L"Asset Loading Errors", message, L"OK", nullptr, 1, 0);
        GetParent()->AppendModalDialog(errorDlg);
    }
}

void FsGuiProgressDialog::OnButtonClick(FsGuiButton *btn)
{
    FsGuiDialog::OnButtonClick(btn);
}