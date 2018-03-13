/*
  Copyright (C) 2006-2007 Chris Cuthbertson

  This file is part of Superfluous.

  Superfluous is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Superfluous is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Superfluous.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "superfluous.h"

extern HWND hDlg;

void About(char **argv, int argc)
{
  DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUT), hDlg, &AboutDlgProc);
}

BOOL CALLBACK AboutDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uiMsg)
  {
    HANDLE_MSG(hwnd, WM_INITDIALOG, OnAboutCreate);
    HANDLE_MSG(hwnd, WM_COMMAND, OnAboutCommand);
    default: return FALSE;
  }

  return TRUE;
}

BOOL OnAboutCreate(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
  HANDLE hIcon;

  if ((hIcon = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAIN_ICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR)))
    SendMessage(hwnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)hIcon);

  if ((hIcon = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAIN_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR)))
    SendMessage(hwnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)hIcon);

  SendDlgItemMessage(hwnd, IDC_ABOUT_TEXT, WM_SETTEXT, 0, (LPARAM)VER_PRODUCTNAME" v" VER_VERSION "\r\n" COPYRIGHT " " AUTHOR "\r\n\r\n" CONTACT);

  return TRUE;
}

void OnAboutCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
  EndDialog(hwnd, IDOK);
}

