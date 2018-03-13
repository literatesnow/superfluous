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

HWND hDlg;
HHOOK hKeyHook;
SOCKET sock;
//BOOL connected;
request_t *reqhead;
request_t *reqtail;
hashtable_t *cvars;
hashtable_t *players;
hashtable_t *internal;

#ifdef NDEBUG
void WINAPI WinMainCRTStartup(void)
{
  int r = 0;

  if (InitApp())
    r = DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)MainDlgProc);

  FinishApp();

  ExitProcess(r);
}
#else
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hinstPrev, LPSTR lpreqLine, int nShowreq)
{
  int r = 0;

  if (InitApp())
    r = DialogBox(hinst, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)MainDlgProc);

  FinishApp();

  return r;
}
#endif

void ProcessCommand(void)
{
  command_t *cmd;
  char buf[MAX_COMMAND+1];
  char *s, *argv[MAX_COMMAND_ARGV];
  int i, argc;

  if (!GetDlgItemText(hDlg, IDC_MAIN_INPUT, buf, sizeof(buf)))
    return;

#ifdef _DEBUG
  if (*buf == '!')
  {
    PrintCommands();
    return;
  }
#endif

  //add to drop down
  SendDlgItemMessage(hDlg, IDC_MAIN_INPUT, CB_INSERTSTRING, 0, (LPARAM)buf);
  //add to console
  Print("\r\n]");
  Print((*buf == '/') ? buf + 1 : buf);
  Print("\r\n");

  //clear box
  SetDlgItemText(hDlg, IDC_MAIN_INPUT, "");

  s = (char *)HashGet(cvars, buf);
  if (s)
  {
    StrCpyN(buf, s, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
  }

  s = buf;
  i = 1;
  argc = 1;
  argv[0] = s;

  while (*s)
  {
    if (i && (*s == ' ') && (argc < MAX_COMMAND_ARGV))
    { 
      *s++ = '\0';

      if (*buf != '/' && *s == '\"')
      {
        i = !i;
        s++;
      }
      argv[argc++] = s;
      if (*buf == '/')
        break;
    }

    if (*s == '\"')
    {
      *s = '\0';
      i = !i;
    }

    s++;
  }

  s = (char *)HashGet(cvars, argv[0]);
  if (s)
    argv[0] = s;

  cmd = FindCommand(argv[0]);
  if (!cmd)
  {
    Print(va("Unknown command: %s\r\n", argv[0]));
    return;
  }

  if (argc < (cmd->args + 1))
  {
    Print(va("Usage: %s %s\r\n", cmd->name, cmd->help));
    return;
  }
 
  if (cmd->connected && (sock == INVALID_SOCKET))
  {
    Print("Not connected.\r\n");
    return;
  }

  cmd->command(argv, argc);
}

void SendSocket(char *data, int sz)
{
  int r;

  if (sz == -1)
    sz = lstrlen(data);

  if (!sz || sock == INVALID_SOCKET)
    return;

  r = send(sock, data, sz, 0);
  if (r == SOCKET_ERROR)
    EndConnection(NULL);
}

void ReadSocket(void)
{
  char *p, *rv, buf[MAX_RECIEVE+1];
  int sz, r;

  KillTimer(hDlg, TIMER_PACKET_ID); //redundant?
  p = NULL;
  r = 0;

  sz = recv(sock, buf, MAX_RECIEVE, 0);
  if ((sz == -1) && (WSAGetLastError() != WSAEWOULDBLOCK))
  {
    Print("Socket error.\r\n");
    CloseSocket();
    return;
  }
  buf[sz] = '\0';

  rv = buf;

  if (!reqhead)
  {
#ifdef _DEBUG
    Print(va("ReadSocket(): OOB: %s\r\n", buf));
#endif
    return;
  }

  if ((reqhead->recvbuf && buf[sz - 1] == '\x04') || (reqhead->type != PACKET_WELCOME && sz > 1 && buf[sz - 1] != '\x04'))
  {
    if (!reqhead->recvbuf)
      p = HEAP_ALLOC(sz + 1);
    else
      p = HEAP_REALLOC(reqhead->recvbuf, reqhead->recvlen + sz + 1);

    if (p)
    {
      memcpy(p + reqhead->recvlen, buf, sz);
      reqhead->recvlen += sz;
      reqhead->recvbuf = p;
      reqhead->recvbuf[reqhead->recvlen] = '\0';

      if (buf[sz - 1] != '\x04')
        return;
      else
      {
        rv = reqhead->recvbuf;
        sz = reqhead->recvlen;
      }
    }
  }

  //Print(va("******%d\n", sz));

  r = reqhead->type;
  p = reqhead->recvbuf;
  RemoveRequest();

#ifdef SUPERDEBUG
  Print(va("ReadSocket(): type - %d (next: %d)\r\n", r, reqhead ? reqhead->type : -1));
#endif

  switch (r)
  {
    case PACKET_RAW:
      r = 0;
      break;

    case PACKET_HIDE:
      r = 1;
      break;

    case PACKET_PRINT:
      r = PRPrint(rv, sz);
      break;

    case PACKET_WELCOME:
      r = PRWelcome(rv, sz);
      break;

    case PACKET_ISLOGIN:
      r = PRLoginReply(rv, sz);
      break;

    case PACKET_LOGGEDIN:
      r = PRLoggedIn(rv, sz);
      break;

    case PACKET_CHECK:
      r = PRCheck(rv, sz);
      break;

    case PACKET_MATCH:
      r = PRMatch(rv, sz);
      break;

    case PACKET_SMATCH:
      r = PRSmatch(rv, sz);
      break;

    case PACKET_SERVERINFO:
      r = PRServerInfo(rv, sz);
      break;

    case PACKET_SERVERCHAT:
      r = PRServerChat(rv, sz);
      break;

#ifdef _DEBUG
    default:
      Print("Unknown packet type!\r\n");
      break;
#endif
  }

#ifdef _DEBUG
  if (!r)
    ShowPacket(rv, sz);
#endif

  if (p)
    HEAP_FREE(p);
}

LRESULT OnWinSockNotify(WPARAM wParam, LPARAM lParam)
{
  int e;

  KillTimer(hDlg, TIMER_CONNECT_ID);
	
  e = WSAGETASYNCERROR(lParam);

  if (e != 0)
  {
    switch (e)
    {
      case WSAECONNRESET:
				EndConnection("Connection was reset.\r\n");
        break;

      case WSAECONNREFUSED:
				Print("Connection was refused.\r\n");
        ServerReconnectAttempt();
        break;

      /*case WSAENOTSOCK:
        EndConnection("Connection not sock\'d.\r\n");
        break;*/

      case WSAECONNABORTED:
        Print("Connection broken.\r\n");
        ServerReconnectAttempt();
        break;

      default:
        EndConnection(va("Connection failed: %d.\r\n", e));
        break;
    }

    return 0;
  }

	switch (WSAGETSELECTEVENT(lParam))
  {
    case FD_READ:
      //Print("FD_READ :D\r\n");
      ReadSocket();
		  break;

    /*case FD_WRITE:
      Print("FD_WRITE :D\r\n");
      break;*/

    case FD_CONNECT:
      Print("Connected.\r\n");
      KillTimer(hDlg, TIMER_RECONNECT_ID);
      break;

    case FD_CLOSE:
      EndConnection(NULL);
      break;

    default:
#ifdef _DEBUG
      Print("wtf? :D\r\n");
#endif
      break;
	}

  return 0;
}

BOOL OnCreate(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
  HANDLE hIcon;

  hDlg = hwnd;

  if ((hIcon = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAIN_ICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR)))
    SendMessage(hDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)hIcon);

  if ((hIcon = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAIN_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR)))
    SendMessage(hDlg, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)hIcon);

  LoadConfig();

  Print(VER_PRODUCTNAME " v" VER_VERSION " build " __TIMESTAMP__ "\r\n");
  Print(VER_COPYRIGHT "\r\n");
  Print(CONTACT "\r\n");

  Print("\r\n");

  //connected = FALSE;

  return TRUE;
}

void OnDestroy(HWND hwnd)
{
  EndDialog(hwnd, 0);
}

void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
  switch (id)
  {
    case IDC_MAIN_SEND:
      ProcessCommand();
      break;

    default:
      break;
  }
}

void OnSize(HWND hwnd, UINT state, int cx, int cy)
{
  RECT rc;
  int inh, sdw;

  if (!cx && !cy)
    return;

  GetWindowRect(GetDlgItem(hwnd, IDC_MAIN_INPUT), &rc);
  inh = (rc.bottom - rc.top) + 2;
  GetWindowRect(GetDlgItem(hwnd, IDC_MAIN_SEND), &rc);
  sdw = (rc.right - rc.left);
                                               //LEFT  TOP  RIGHT BOTTOM
                                               //  X   Y    WIDTH HEIGHT
  SetWindowPos(GetDlgItem(hwnd, IDC_MAIN_CONSOLE), NULL, 0, 0, cx, cy - inh - 1, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, IDC_MAIN_INPUT), NULL, 0, cy - inh + 1, cx - sdw, 100, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(hwnd, IDC_MAIN_SEND), NULL, cx - sdw, cy - inh + 1, sdw, inh - 3, SWP_NOZORDER);
}

void OnPrintClient(HWND hwnd, HDC hdc)
{
  PAINTSTRUCT ps;

  ps.hdc = hdc;
  GetClientRect(hwnd, &ps.rcPaint);
  PaintContent(hwnd, &ps);
}

void OnPaint(HWND hwnd)
{
  PAINTSTRUCT ps;

  BeginPaint(hwnd, &ps);
  PaintContent(hwnd, &ps);
  EndPaint(hwnd, &ps);
}

void PaintContent(HWND hwnd, PAINTSTRUCT *pps)
{
}

void OnTimer(HWND hWnd, UINT id)
{
  switch (id)
  {
    case TIMER_CONNECT_ID:
      EndConnection("Connection timed out.\r\n");
      break;

    case TIMER_RECONNECT_ID:
      ServerReconnect();
      break;

    case TIMER_PACKET_ID:
      Print("Command timed out.\r\n");
      RemoveRequest();
      break;

    case TIMER_SERVERCHAT_ID:
      ClientChat();
      break;

    default:
      break;
  }
}

void Print(char *text)
{
  int i, j;

  i = GetWindowTextLength(GetDlgItem(hDlg, IDC_MAIN_CONSOLE));
  j = lstrlen(text);

  if (i + j >= MAX_EDIT_LIMIT)
  {
    SendDlgItemMessage(hDlg, IDC_MAIN_CONSOLE, EM_SETSEL, (WPARAM)0, (LPARAM)j);
    SendDlgItemMessage(hDlg, IDC_MAIN_CONSOLE, EM_REPLACESEL, (WPARAM)0, (LPARAM)"");

    j = SendDlgItemMessage(hDlg, IDC_MAIN_CONSOLE, EM_LINELENGTH, (WPARAM)0, (LPARAM)0);
    if (j)
    {
      SendDlgItemMessage(hDlg, IDC_MAIN_CONSOLE, EM_SETSEL, (WPARAM)0, (LPARAM)j + 2);
      SendDlgItemMessage(hDlg, IDC_MAIN_CONSOLE, EM_REPLACESEL, (WPARAM)0, (LPARAM)"");
    }
  }

  SendDlgItemMessage(hDlg, IDC_MAIN_CONSOLE, EM_SETSEL, (WPARAM)i, (LPARAM)i);
  SendDlgItemMessage(hDlg, IDC_MAIN_CONSOLE, EM_REPLACESEL, (WPARAM)0, (LPARAM)text);
}

#ifdef _DEBUG
void ShowPacket(char *s, int sz)
{
  char fub[8];
  int i, j;

  Print(">>>>> PACKET_BEGIN(");
  wnsprintf(fub, sizeof(fub), "%d", sz);
  Print(fub);
  Print(")\r\n");

  for (i = 0; i < sz; i += LINE_LENGTH)
  {
    for (j = i; (j < (i + LINE_LENGTH)) && (j < sz); j++)
    {
      wnsprintf(fub, sizeof(fub), "%03d ", (unsigned char)s[j]);
      Print(fub);
    }

    if (j < (i + LINE_LENGTH))
    {
      for (; (j < (i + LINE_LENGTH)); j++)
      {
        Print("    ");
      }
    }
    for (j = i; (j < (i + LINE_LENGTH)) && (j < sz); j++)
    {
      wnsprintf(fub, sizeof(fub), "%c", (s[j] < 32) ? '.' : s[j]);
      Print(fub);
    }
    Print("\r\n");
  }
 
  Print("<<<<< PACKET_END\r\n");
}
#endif

BOOL CALLBACK MainDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uiMsg)
  {
    HANDLE_MSG(hwnd, WM_INITDIALOG, &OnCreate);
    HANDLE_MSG(hwnd, WM_SIZE, &OnSize);
    HANDLE_MSG(hwnd, WM_CLOSE, &OnDestroy);
    HANDLE_MSG(hwnd, WM_COMMAND, &OnCommand);
    HANDLE_MSG(hwnd, WM_TIMER, &OnTimer);
    HANDLE_MSG(hwnd, WM_PAINT, &OnPaint);
    //HANDLE_MSG(hwnd, WM_KEYDOWN, &OnKey);

    case WM_PRINTCLIENT:
      OnPrintClient(hwnd, (HDC)wParam);
      return 0;

    case UC_AASYNC:
      OnWinSockNotify(wParam, lParam);
      break;

    default:
      return FALSE;
  }

  return TRUE;
}

__declspec(dllexport) LRESULT CALLBACK KeyEvent(int nCode, WPARAM wParam, LPARAM lParam)
{
  static BOOL shiftDown = FALSE;
  static BOOL hotKey = FALSE;

  if (nCode == HC_ACTION)
  {
    KBDLLHOOKSTRUCT *hook = (KBDLLHOOKSTRUCT *)lParam;

    switch (wParam)
    {
      case WM_KEYDOWN:
        switch (hook->vkCode)
        {
          case VK_LSHIFT:
          case VK_RSHIFT:
            shiftDown = TRUE;
            break;
          case VK_ESCAPE:
            if (hotKey)
            {
              hotKey = FALSE;
              Print("Hook deactivated\r\n");

              return TRUE;
            }
            break;
          case VK_F12:
            if (shiftDown)
            {
              hotKey = !hotKey;
              if (hotKey)
              {
                SetFocus(GetDlgItem(hDlg, IDC_MAIN_INPUT));
                Print("Hook activated\r\n");
                return TRUE;
              }
              else
              {
                Print("Hook deactivated\r\n");
                return TRUE;
              }
            }
            break;            
          default:
            break;
        }
        if (hotKey)
        {
          DWORD dwMsg = 1;

          dwMsg += hook->scanCode << 16;
          dwMsg += hook->flags << 24;
          PostMessage(GetDlgItem(hDlg, IDC_MAIN_INPUT), WM_KEYDOWN, hook->vkCode, dwMsg);

          return TRUE;
        }
        break;

      case WM_KEYUP:
        switch (hook->vkCode)
        {
          case VK_LSHIFT:
          case VK_RSHIFT:
            shiftDown = FALSE;
            break;
          default:
            break;
        }
        break;

      default:
        break;
    }
  }

  return CallNextHookEx(hKeyHook, nCode, wParam, lParam);
}

void CloseSocket(void)
{
  shutdown(sock, 2); //SD_BOTH
  if (sock != INVALID_SOCKET)
    closesocket(sock);
  sock = INVALID_SOCKET;
}

void LoadConfig(void)
{
  char line[256];
  char *p, *q;
  int i;
  HANDLE fp;
  HANDLE mp;
  DWORD sz;

  if ((fp = CreateFile(CONFIG_FILE, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE)
    return;

  sz = GetFileSize(fp, NULL);
  mp = CreateFileMapping(fp, NULL, PAGE_READONLY, 0, 0, NULL);
  if (mp)
  {
    p = (char *)MapViewOfFile(mp, FILE_MAP_READ, 0, 0, sz);

    while (*p)
    {
      i = 0;
      line[0] = '\0';
      q = NULL;

      while (*p && *p != '\n')
      {
        if ((*p != '\r') && (*p != '\t') && (i < sizeof(line)))
        {
          line[i] = *p;
          if (!q && *p == '=')
            q = &line[i];
          i++;
        }

        p++;
      }
      p++;

      if (i && q)
      {
        line[i] = '\0';
        *q++ = '\0';

        HashStrSet(cvars, line, q);
        //Print(va("%s = %s\r\n", line, q));
      }
    }

    UnmapViewOfFile(mp);
  }

  CloseHandle(fp);
}

#ifdef _DEBUG
void PrintCommands(void)
{
  request_t *req;
  int i = 0;

  for (req = reqhead; req; req = req->next)
  {
    Print(va("%d ", req->type));
    i++;
  }
  Print(va(" = %d\r\n", i));
}
#endif

BOOL InitApp(void)
{
  WSADATA wsaData;

  sock = INVALID_SOCKET;
  internal = NULL;
  cvars = NULL;
  players = NULL;
  hKeyHook = NULL;

  if (WSAStartup(MAKEWORD(1,1), &wsaData) != 0)
  {
    MessageBox(NULL, "Failed to start up WinSock.", "bah", MB_OK | MB_ICONWARNING);
    return FALSE;
  }

  cvars = NewHashTable(MAX_CVARS);
  if (!cvars)
    return FALSE;

  internal = NewHashTable(MAX_INTERNALS);
  if (!internal)
    return FALSE;

  players = NewHashTable(MAX_PLAYERS);
  if (!players)
    return FALSE;

  if (!NewServerInfo())
    return FALSE;

  //watch out for GetModuleHandle() in multi threading
  hKeyHook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)KeyEvent, GetModuleHandle(NULL), 0);

  return TRUE;
}

void FinishApp(void)
{
  if (hKeyHook)
    UnhookWindowsHookEx(hKeyHook);

  if (cvars)
    RemoveHashTable(&cvars);

  if (internal)
    RemoveHashTable(&internal);

  if (players)
    RemoveHashTable(&players);

  RemoveRequests();

  CloseSocket();

  WSACleanup();
}

