#include "fssimpleprogressdisplay.h"
#include "graphics/common/fsopengl.h"
#include "graphics/common/fsfontrenderer.h"
#include "platform/common/fswindow.h"
#include <fssimplewindow.h>
#include <stdio.h>

FsSimpleProgressDisplay fsSimpleProgress;

FsSimpleProgressDisplay::FsSimpleProgressDisplay()
{
    currentStep = 0;
    totalSteps = 0;
    isVisible = YSFALSE;
    currentFileName.Set(L"");
    failedFiles.Clear();
    logoTexture = 0;
    logoLoaded = YSFALSE;
}

FsSimpleProgressDisplay::~FsSimpleProgressDisplay()
{
}

void FsSimpleProgressDisplay::Show(const wchar_t title[])
{
    printf("DEBUG: FsSimpleProgressDisplay::Show called\n");
    isVisible = YSTRUE;
    currentText.Set(title);
    progressText.Set(L"Initializing...");
    currentFileName.Set(L"");
    currentStep = 0;
    totalSteps = 1;
    failedFiles.Clear();

    // Try to load logo image
    if(YSFALSE == logoLoaded)
    {
        // Try different common logo filenames
        const char* logoFiles[] = {
            "misc/logo.png",
            "misc/icon.png",
            "misc/ysflight.png",
            "bitmap/logo.png",
            "bitmap/icon.png",
            NULL
        };

        for(int i = 0; logoFiles[i] != NULL && YSFALSE == logoLoaded; i++)
        {
            if(YSOK == logoBmp.LoadPng(logoFiles[i]))
            {
                printf("DEBUG: Loaded logo: %s\n", logoFiles[i]);

                // Create OpenGL texture
                glGenTextures(1, &logoTexture);
                glBindTexture(GL_TEXTURE_2D, logoTexture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                // Upload texture data
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                           logoBmp.GetWidth(), logoBmp.GetHeight(), 0,
                           GL_RGBA, GL_UNSIGNED_BYTE, logoBmp.GetRGBABitmapPointer());

                logoLoaded = YSTRUE;
                printf("DEBUG: Logo texture created\n");
            }
        }

        if(YSFALSE == logoLoaded)
        {
            printf("DEBUG: No logo file found, using simple icon\n");
        }
    }
}

void FsSimpleProgressDisplay::Hide()
{
    printf("DEBUG: FsSimpleProgressDisplay::Hide called\n");
    isVisible = YSFALSE;
}

void FsSimpleProgressDisplay::SetProgress(int current, int total, const wchar_t description[])
{
    printf("DEBUG: FsSimpleProgressDisplay::SetProgress %d/%d: %ls\n", current, total, description);
    currentStep = current;
    totalSteps = total;

    YsWString newText;
    newText.Set(L"Loading: ");
    newText.Append(description);
    progressText.Set(newText);
    currentFileName.Set(L"");
}

void FsSimpleProgressDisplay::SetCurrentFile(const wchar_t filename[])
{
    printf("DEBUG: FsSimpleProgressDisplay::SetCurrentFile: %ls\n", filename);
    currentFileName.Set(filename);
}

void FsSimpleProgressDisplay::AddFailedFile(const wchar_t filename[])
{
    printf("DEBUG: FsSimpleProgressDisplay::AddFailedFile: %ls\n", filename);
    failedFiles.Append(YsWString(filename));
}

void FsSimpleProgressDisplay::ShowFailedFiles()
{
    // This function will be called from the main application
    // The main application will handle showing the message box
}

void FsSimpleProgressDisplay::Draw()
{
    if(YSFALSE == isVisible)
    {
        return;
    }
    
    int winWid, winHei;
    FsGetWindowSize(winWid, winHei);
    
    // Save current OpenGL state
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushMatrix();
    glLoadIdentity();
    
    // Set up 2D projection
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, winWid, winHei, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    
    // Disable depth testing for overlay
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw semi-transparent background
    glColor4f(0.0f, 0.0f, 0.0f, 0.7f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(winWid, 0);
    glVertex2f(winWid, winHei);
    glVertex2f(0, winHei);
    glEnd();

    // Calculate dialog position (center of screen)
    int dialogW = 520;
    int dialogH = 180;
    int dialogX = (winWid - dialogW) / 2;
    int dialogY = (winHei - dialogH) / 2;

    // Draw dialog background
    glColor4f(0.9f, 0.9f, 0.9f, 0.95f);
    glBegin(GL_QUADS);
    glVertex2f(dialogX, dialogY);
    glVertex2f(dialogX + dialogW, dialogY);
    glVertex2f(dialogX + dialogW, dialogY + dialogH);
    glVertex2f(dialogX, dialogY + dialogH);
    glEnd();

    // Draw logo or aircraft icon
    int iconX = dialogX + 250;
    int iconY = dialogY - 30;
    int maxIconSize = 200;

    if(logoLoaded && logoTexture != 0)
    {
        // Calculate proper dimensions preserving aspect ratio
        int imgW = logoBmp.GetWidth();
        int imgH = logoBmp.GetHeight();
        int drawW, drawH;

        if(imgW > imgH)
        {
            // Wider than tall - fit to width
            drawW = maxIconSize;
            drawH = (imgH * maxIconSize) / imgW;
        }
        else
        {
            // Taller than wide - fit to height
            drawH = maxIconSize;
            drawW = (imgW * maxIconSize) / imgH;
        }

        // Center the image in the icon area
        int drawX = iconX + (maxIconSize - drawW) / 2;
        int drawY = iconY + (maxIconSize - drawH) / 2;

        // Draw logo texture with preserved aspect ratio
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, logoTexture);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(drawX, drawY);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(drawX + drawW, drawY);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(drawX + drawW, drawY + drawH);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(drawX, drawY + drawH);
        glEnd();

        glDisable(GL_TEXTURE_2D);

        printf("DEBUG: Drew logo %dx%d scaled to %dx%d\n", imgW, imgH, drawW, drawH);
    }
    else
    {
        // Draw simple aircraft icon
        int iconSize = 32; // Bigger simple icon to match logo area
        int drawX = iconX + (maxIconSize - iconSize) / 2;
        int drawY = iconY + (maxIconSize - iconSize) / 2;

        glColor4f(0.2f, 0.4f, 0.8f, 1.0f);
        glLineWidth(3.0f);
        glBegin(GL_LINE_STRIP);
        // Simple airplane outline
        glVertex2f(drawX + iconSize/2, drawY);                    // nose
        glVertex2f(drawX + iconSize/2, drawY + iconSize*0.7f);    // body
        glVertex2f(drawX, drawY + iconSize*0.5f);                 // left wing
        glVertex2f(drawX + iconSize/2, drawY + iconSize*0.7f);    // back to body
        glVertex2f(drawX + iconSize, drawY + iconSize*0.5f);      // right wing
        glVertex2f(drawX + iconSize/2, drawY + iconSize*0.7f);    // back to body
        glVertex2f(drawX + iconSize*0.3f, drawY + iconSize);      // left tail
        glVertex2f(drawX + iconSize/2, drawY + iconSize*0.7f);    // back to body
        glVertex2f(drawX + iconSize*0.7f, drawY + iconSize);      // right tail
        glEnd();
    }

    // Draw dialog border
    glColor4f(0.3f, 0.3f, 0.3f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(dialogX, dialogY);
    glVertex2f(dialogX + dialogW, dialogY);
    glVertex2f(dialogX + dialogW, dialogY + dialogH);
    glVertex2f(dialogX, dialogY + dialogH);
    glEnd();

    // Draw progress bar background (make room for icon on left)
    int barX = dialogX + 100;
    int barY = dialogY + 130;
    int barW = dialogW - 120;
    int barH = 20;

    glColor4f(0.9f, 0.9f, 0.9f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(barX, barY);
    glVertex2f(barX + barW, barY);
    glVertex2f(barX + barW, barY + barH);
    glVertex2f(barX, barY + barH);
    glEnd();

    // Draw progress bar fill
    if(totalSteps > 0)
    {
        float progress = (float)(currentStep + 1) / (float)totalSteps;
        int fillW = (int)(barW * progress);

        glColor4f(0.2f, 0.6f, 0.2f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(barX, barY);
        glVertex2f(barX + fillW, barY);
        glVertex2f(barX + fillW, barY + barH);
        glVertex2f(barX, barY + barH);
        glEnd();
    }

    // Draw progress bar border
    glColor4f(0.3f, 0.3f, 0.3f, 1.0f);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(barX, barY);
    glVertex2f(barX + barW, barY);
    glVertex2f(barX + barW, barY + barH);
    glVertex2f(barX, barY + barH);
    glEnd();

    // Draw text using simple raster positioning
    glColor4f(0.1f, 0.1f, 0.1f, 1.0f);

    // Convert wide strings to ASCII for simple rendering
    YsString titleStr, progressStr, currentFileStr, percentStr;
    titleStr.EncodeUTF8<wchar_t>(currentText);
    progressStr.EncodeUTF8<wchar_t>(progressText);
    currentFileStr.EncodeUTF8<wchar_t>(currentFileName);

    if(totalSteps > 0)
    {
        int percent = ((currentStep + 1) * 100) / totalSteps;
        percentStr.Printf("%d/%d (%d%%)", currentStep + 1, totalSteps, percent);
    }
    else
    {
        percentStr.Set("0%");
    }

    // Add title text area (next to icon)
    int titleX = dialogX + 100;
    int titleY = dialogY + 25;

    // Add current file text area
    int fileX = dialogX + 100;
    int fileY = dialogY + 50;

    // Add status text area
    int statusX = dialogX + 100;
    int statusY = dialogY + 75;

    // Print debug info to console since OpenGL text rendering is complex
    printf("Progress: %s | %s", titleStr.Txt(), progressStr.Txt());
    if(currentFileStr.Strlen() > 0)
    {
        printf(" | Current: %s", currentFileStr.Txt());
    }
    printf(" | %s\n", percentStr.Txt());

    // Restore GL state completely
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
    
    // Swap buffers to show the progress
    FsSwapBuffers();
}

YSBOOL FsSimpleProgressDisplay::IsVisible() const
{
    return isVisible;
}
