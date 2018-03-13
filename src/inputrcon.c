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

BOOL CALLBACK InputRconDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uiMsg)
  {
    HANDLE_MSG(hwnd, WM_INITDIALOG, &OnInputRconCreate);
    HANDLE_MSG(hwnd, WM_COMMAND, &OnInputRconCommand);
    default: return FALSE;
  }

  return TRUE;
}

BOOL OnInputRconCreate(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
  HANDLE hIcon;

  if ((hIcon = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAIN_ICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR)))
    SendMessage(hwnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)hIcon);

  if ((hIcon = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAIN_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR)))
    SendMessage(hwnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)hIcon);

  SetWindowLong(hwnd, GWL_USERDATA, (LPARAM)lParam);

  return TRUE;
}

void OnInputRconCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
  char *p;

  switch (id)
  {
    case IDOK:
      p = (char *)GetWindowLong(hwnd, GWL_USERDATA);
      if (p)
      {
        if (GetDlgItemText(hwnd, IDC_INPUTRCON_EDIT, p, MAX_RCON))
        {
          EndDialog(hwnd, IDOK);
          break;
        }
      }

    case IDCANCEL:
      EndDialog(hwnd, IDCANCEL);
      break;

    default:
      break;
  }
}

