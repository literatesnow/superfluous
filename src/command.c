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
extern SOCKET sock;
//extern BOOL connected;
extern request_t *reqhead;
extern request_t *reqtail;
extern hashtable_t *cvars;
extern hashtable_t *players;
extern hashtable_t *internal;

command_t commands[MAX_COMMANDS] =
{
  //exec command must be #1
  {
    &ExecCommand,
    "exec",
    "<command> [args]",
    0,
    1
  },
  {
    &ServerConnect,
    "connect",
    "<ip>[:<port>] [name]",
    1, //min args
    0 //must be connected?
  },
  {
    &ServerDisconnect,
    "disconnect",
    "",
    0,
    1
  },
  {
    &ServerInfo,
    "serverinfo",
    "",
    0,
    1
  },
  {
    &SimpleMatchPlayer,
    "smatch",
    "<name>",
    0,
    1
  },
  {
    &MatchPlayer,
    "match",
    "match <name[|*]> [filter1[filterN]]",
    0,
    1
  },
  {
    &SwapPlayer,
    "swap",
    "",
    0,
    1
  },
  {
    &KickPlayer,
    "kick",
    "<\"global reason\">", // [\"player reason\"]",
    1,
    1
  },
  {
    &BanPlayer,
    "ban",
    "<\"global reason\">", // [\"player reason\"]",
    1,
    1
  },
  {
    &SayAll,
    "sayall",
    "<\"message\">",
    1,
    1
  },
  /*{
    &SayPlayer,
    "sayplayer",
    "<\"message\">",
    1,
    1
  },*/
  {
    &ServerChat,
    "serverchat",
    "<rate>",
    0,
    1
  },
  {
    &RawCommand,
    "raw",
    "<command>",
    1,
    1
  },
  {
    &About,
    "about",
    "",
    0,
    0
  },
  {
    &Quit,
    "quit",
    "",
    0,
    0
  }
};

command_t *FindCommand(char *name)
{
  int i;

  if (*name == '/')
    return &commands[0];

  for (i = 1; i < MAX_COMMANDS; i++)
  {
    if (!StrCmpI(commands[i].name, name))
      return &commands[i];
  }

  return NULL;
}

void ServerConnect(char **argv, int argc)
{
  char *p, *ip, *rcon, *admin;
  char ipbuf[MAX_IP+1];
  char rconbuf[MAX_RCON+1];
  int port;

  admin = (argc == 3) ? argv[2] : NULL;

  if (validip(argv[1]))
  {
    ip = argv[1];
    rcon = NULL;
    if (!admin)
      admin = (char *)HashGet(cvars, CFG_ADMIN_NAME);
  }
  else
  {
    ip = (char *)HashGet(cvars, argv[1]);
    rcon = (char *)HashGet(cvars, va("%s" CFG_RCON_PASSWORD, argv[1]));
    if (!admin)
      admin = (char *)HashGet(cvars, va("%s" CFG_ADMIN_NAME, argv[1]));
  }

  if (!ip)
  {
    Print("Bad address.\r\n");
    return;
  }

  StrCpyN(ipbuf, ip, sizeof(ipbuf));
  ipbuf[sizeof(ipbuf) - 1] = '\0';
  ip = ipbuf;

  p = StrChr(ipbuf, ':');
  if (p)
  {
    *p++ = '\0';
    port = StrToInt(p);
  }
  else
  {
    port = DEFAULT_RCON_PORT;
  }

  if (!rcon)
  {
    if (DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_INPUTRCON), hDlg, &InputRconDlgProc, (LPARAM)rconbuf) == IDCANCEL)
      return;

    rcon = rconbuf;
  }

  if (sock != INVALID_SOCKET)
    EndConnection(NULL); //this CLEARS INTERNAL!

  HashStrSet(internal, INTERNAL_RCON_PASSWORD, rcon);
  HashStrSet(internal, INTERNAL_ADMIN_NAME, admin ? admin : DEFAULT_ADMIN_NAME);

  HashStrSet(internal, INTERNAL_SERVER_IP, ip);
  HashIntSet(internal, INTERNAL_SERVER_PORT, port);

  //inet_ntoa(network->connectaddr.sin_addr), ntohs(network->connectaddr.sin_port)
  SetWindowText(hDlg, va(VER_PRODUCTNAME " (%s:%d)", ip, port));

  ServerConnection(ip, port);
}

void ServerConnection(char *ip, int port)
{
  struct sockaddr_in addr;

  if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
  {
    Print("Failed to create socket.\r\n");
    return;
  }

	if (WSAAsyncSelect(sock, hDlg, UC_AASYNC, FD_READ | /*FD_WRITE |*/ FD_CONNECT | FD_CLOSE) == SOCKET_ERROR)
  {
    Print("Failed to async socket.\r\n");
		return;
	}

  memset(&(addr.sin_zero), '\0', sizeof(addr.sin_zero));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
 	addr.sin_port = htons((short)port);

  AddRequest(PACKET_WELCOME);
  Print(va("Connecting to %s:%d...\r\n", ip, port));

  if (connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
  {
    int errno = WSAGetLastError();
    if (errno != WSAEWOULDBLOCK)
    {
      Print("Connect failed.\r\n");
      return;
    }

    SetTimer(hDlg, TIMER_CONNECT_ID, TIMER_CONNECT_MS, NULL);
    return;
  }
 
  PostMessage(hDlg, UC_AASYNC, sock, MAKELPARAM(FD_CONNECT, 0));
}

void ServerAuthed(void)
{
  ServerInfo(NULL, 0);
}

void ServerReconnectAttempt(void)
{
  SetTimer(hDlg, TIMER_RECONNECT_ID, TIMER_RECONNECT_MS, NULL);
  CloseSocket();
}

void ServerReconnect(void)
{
  char *ip;
  int *port;

  ip = (char *)HashGet(internal, INTERNAL_SERVER_IP);
  if (!ip)
    return;
  port = (int *)HashGet(internal, INTERNAL_SERVER_PORT);
  if (!port)
    return;

  Print("Reconnecting...\r\n");

  CloseSocket();
  ServerConnection(ip, *port);
}

void ServerDisconnect(char **argv, int argc)
{
  EndConnection(NULL);
}

void EndConnection(char *msg)
{
  CloseSocket();
  RemoveRequests();
  HashClear(internal);
  NewServerInfo();
  SetWindowText(hDlg, VER_PRODUCTNAME);

  KillTimer(hDlg, TIMER_CONNECT_ID);
  KillTimer(hDlg, TIMER_RECONNECT_ID);
  KillTimer(hDlg, TIMER_SERVERCHAT_ID);

  //connected = FALSE;

  Print((msg) ? msg : "Disconnected.\r\n");
}

void SimpleMatchPlayer(char **argv, int argc)
{
  HashStrSet(internal, INTERNAL_SMATCH_NAME, (argc == 2) ? argv[1] : NULL);
  AddRequest(PACKET_SMATCH);
  SendSocket("\x02" "exec admin.listplayers" "\n", -1);
}

void MatchPlayer(char **argv, int argc)
{
  char *match, *filter;
  
  match = NULL;
  filter = NULL;

  if (argc >= 2)
  {
    match = argv[1];
    if (argc == 3)
    {
      filter = argv[2];
      if (*(argv[1]) == '*')
        match = NULL;
    }
  }

  HashStrSet(internal, INTERNAL_MATCH_NAME, match);
  HashStrSet(internal, INTERNAL_MATCH_FILTER, filter);
  AddRequest(PACKET_MATCH);
  SendSocket("\x02" "bf2cc pl" "\n", -1);
}

void ServerChat(char **argv, int argc)
{
  char *p;
  int b, time;

  b = 1;
  time = 0;

  if (argc == 2)
  {
    if (!StrCmpI(argv[1], "stop"))
    {
      KillTimer(hDlg, TIMER_SERVERCHAT_ID);
      return;
    }

    p = argv[1];
    if (*p == '*')
    {
      p++;
      b = 0;
    }

    if (*p)
    {
      time = clamp(StrToInt(p) * 1000, TIMER_SERVERCHAT_MIN_MS, TIMER_SERVERCHAT_MAX_MS);
    }
  }

  if (!time)
    time = TIMER_SERVERCHAT_MS;

  if (b)
  {
    AddRequest(PACKET_SERVERCHAT);
    SendSocket("\x02" "bf2cc serverchatbuffer" "\n", -1);
  }

  SetTimer(hDlg, TIMER_SERVERCHAT_ID, time, NULL);
}

void ClientChat(void)
{
  AddRequest(PACKET_SERVERCHAT);
  SendSocket("\x02" "bf2cc clientchatbuffer" "\n", -1); 
}

void RawCommand(char **argv, int argc)
{
  char buf[MAX_COMMAND+1];
  char *p;
  int i, j;

  for (i = 1; i < argc; i++)
    *(argv[i] - 1) = ' ';

  i = 0;
  p = argv[1];
  while (*p)
  {
    if (i < sizeof(buf))
    {
      buf[i++] = hexcode(p, &j);
      p += j;
    }
    else
    {
      p++;
    }
  }
  //if (i < sizeof(buf))
  //  buf[i++] = '\n';
  buf[i] = '\0';

  AddRequest(PACKET_PRINT);
  SendSocket(buf, -1);
}

void ExecCommand(char **argv, int argc)
{
  AddRequest(PACKET_PRINT);
  if (argc > 1)
    SendSocket(va("\x02" "exec %s %s\n", argv[0] + 1, argv[1]), -1);
  else
    SendSocket(va("\x02" "exec %s\n", argv[0] + 1), -1);
}

void KickPlayer(char **argv, int argc)
{
  int id, time;
  player_t *pl;
  char *reason;
  char *kickprefix, *seperator;

  if (!(pl = GetTargetPlayer()))
    return;

  if (argc >= 3)
  {
    time = StrToInt(argv[1]) * 1000;
    reason = argv[2];
  }
  else
  {
    time = TIMER_KICK_MS;
    reason = argv[1];
  }

  if (!(kickprefix = (char *)HashGet(cvars, CFG_KICK_PREFIX)))
    kickprefix = DEFAULT_KICK_PREFIX;
  if (!(seperator = (char *)HashGet(cvars, CFG_SEPARATOR)))
    seperator = DEFAULT_SEPARATOR;

  AddRequest(PACKET_HIDE);
  //moved to when they're actually kicked
  //Print(va("Kicking %s: %s (%s)\r\n", pl->name, argv[1], (argc == 3) ? argv[2] : "none"));
  //Print(va("Kicking %s: %s\r\n", pl->name, argv[1]));

  SendSocket(va("\x02" "exec game.sayAll \"%s %s%s %s\"\n", kickprefix, pl->name, seperator, reason), -1);

  //if (argc == 3)
  //  SendAnonPlayerMessage(pl->id, argv[2]);

  id = TIMER_KICKBEGIN_ID + pl->id;
  HashStrSet(internal, va(INTERNAL_BOOT_TARGET "%d", id), pl->name);
  HashStrSet(internal, va(INTERNAL_BOOT_REASON "%d", id), reason);
  SetTimer(hDlg, id, time, &TimedBootProc);
}

void BanPlayer(char **argv, int argc)
{
  int id, time;
  player_t *pl;
  char *reason;
  char *banprefix, *seperator;
 
  if (!(pl = GetTargetPlayer()))
    return;

  if (argc >= 3)
  {
    time = StrToInt(argv[1]) * 1000;
    reason = argv[2];
  }
  else
  {
    time = TIMER_BAN_MS;
    reason = argv[1];
  }

  if (!(banprefix = (char *)HashGet(cvars, CFG_BAN_PREFIX)))
    banprefix = DEFAULT_BAN_PREFIX;
  if (!(seperator = (char *)HashGet(cvars, CFG_SEPARATOR)))
    seperator = DEFAULT_SEPARATOR;

  AddRequest(PACKET_HIDE);
  //moved to when they're actually banned
  //Print(va("Banning %s: %s (%s)\r\n", pl->name, argv[1], (argc == 3) ? argv[2] : "none"));
  //Print(va("Banning %s: %s\r\n", pl->name, argv[1]));
  SendSocket(va("\x02" "exec game.sayAll \"%s %s%s %s\"\n", banprefix, pl->name, seperator, reason), -1);

  //if (argc == 3)
  //  SendAnonPlayerMessage(pl->id, argv[2]);

  id = TIMER_BANBEGIN_ID + pl->id;
  HashStrSet(internal, va(INTERNAL_BOOT_TARGET "%d", id), pl->name);
  HashStrSet(internal, va(INTERNAL_BOOT_REASON "%d", id), reason);
  SetTimer(hDlg, id, time, &TimedBootProc);
}

VOID CALLBACK TimedBootProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
  char *p, *reason;
  player_t *pl;
  int id;

  KillTimer(hwnd, idEvent);

  id = idEvent;

  if (!(p = (char *)HashGet(internal, va(INTERNAL_BOOT_TARGET "%d", id))))
    goto _end;
  if (!(pl = (player_t *)HashGet(players, p)))
    goto _end;
  if (!(reason = (char *)HashGet(internal, va(INTERNAL_BOOT_REASON "%d", id))))
    goto _end;

  if ((id >= TIMER_KICKBEGIN_ID) && (id <= TIMER_KICKEND_ID))
  {
    AddRequest(PACKET_HIDE);
    SendSocket(va("\x02" "exec PB_SV_Kick \"%s\" 1 %s\n", pl->name, reason), -1);
    Print(va("Kicked %s: %s\r\n", pl->name, reason));
  }
  else if ((id >= TIMER_BANBEGIN_ID) && (id <= TIMER_BANEND_ID))
  {
    AddRequest(PACKET_HIDE);
    SendSocket(va("\x02" "exec PB_SV_Kick \"%s\" 1 %s\n", pl->name, reason), -1);
    AddRequest(PACKET_HIDE);
    if (*pl->hash)
      SendSocket(va("\x02" "exec admin.addKeyToBanList %s\n", pl->hash), -1);
    else
      SendSocket(va("\x02" "exec admin.addAddressToBanList %s\n", pl->ip), -1);
    Print(va("Banned %s: %s\r\n", pl->name, reason));
  }
#ifdef _DEBUG
  else
  {
    Print("er.. wtf is this id from\r\n");
  }
#endif

_end:
  HashRemove(internal, va(INTERNAL_BOOT_TARGET "%d", id));
  HashRemove(internal, va(INTERNAL_BOOT_REASON "%d", id));
}

void ServerInfo(char **argv, int argc)
{
  AddRequest(PACKET_SERVERINFO);
  SendSocket("\x02" "bf2cc si\n", -1);
}

void SwapPlayer(char **argv, int argc)
{
  player_t *pl;

  if (!(pl = GetTargetPlayer()))
    return;

  AddRequest(PACKET_PRINT);
  SendSocket(va("\x02" "bf2cc switchplayer %d 0\n", pl->id), -1);
}

void SayAll(char **argv, int argc)
{
  AddRequest(PACKET_PRINT);
  SendSocket(va("\x02" "bf2cc sendserverchat %s\n", argv[1]), -1);
}

/*void SayPlayer(char **argv, int argc)
{
  player_t *pl;

  if (!(pl = GetTargetPlayer()))
    return;

  AddRequest(PACKET_PRINT);
  SendSocket(va("\x02" "bf2cc sendplayerchat %d %s\n", pl->id, argv[1]), -1);
}*/

void Quit(char **argv, int argc)
{
  PostMessage(hDlg, WM_CLOSE, (WPARAM)0, (LPARAM)0);
}

/*//////////////////////////////////////////

PRCOMMANDS

//////////////////////////////////////////*/

//### Battlefield 2 ModManager Rcon v1.7.
//### Digest seed: aHXPFlLBbWlomlda.
int PRWelcome(char *rv, int sz)
{
  char md5hex[MAX_MD5+1];
  char *p, *seed, *rcon;

  if (sz < 33)
    return 0;

  p = StrChr(rv, '\n');
  if (p && (p != &rv[sz - 1]))
    *p++ = '\0';
  else
    p = NULL;

  if (!StrCmpN(rv, "### Battlefield 2", 17))
  {
    rv[sz - 1] = '\0';
    PRPrint(rv, sz);
    if (!p)
    {
      AddRequest(PACKET_WELCOME);
      return 1;
    }
    rv = p;
  }

  if (StrCmpN(rv, "### Digest seed", 15))
    return 0;

  rcon = (char *)HashGet(internal, INTERNAL_RCON_PASSWORD);
  if (!rcon)
  {
#ifdef _DEBUG
    Print("Rcon was lost?!\r\n");
#endif
    return 0;
  }

  seed = rv + 17;
  seed[16] = '\0';

  AddRequest(PACKET_ISLOGIN);
  md5(va("%s%s", seed, rcon), md5hex, sizeof(md5hex));
  SendSocket(va("\x02login %s\n", md5hex), -1);

  PRPrint(rv, sz);

  return 1;
}

int PRLoginReply(char *rv, int sz)
{
  char *p;

  if (sz == 1 && *rv == '\n')
  {
    AddRequest(PACKET_ISLOGIN);
    return 1;
  }

  if (sz < 16)
    return 0;

  if (!StrCmpN(rv + 15, "failed", 6))
  {
    //PRPrint(rv, sz);
    EndConnection("Authentication failed.\r\n");
    return 1;
  }

  if (StrCmpN(rv + 15, "successful", 10))
    return 0;

  p = (char *)HashGet(internal, INTERNAL_ADMIN_NAME);
  if (!p)
    return 0;

  AddRequest(PACKET_LOGGEDIN);
  SendSocket(va("\x02" "bf2cc setadminname %s\n", p), -1);

  //AddRequest(PACKET_CHECK);
  //SendSocket("\x02" "bf2cc check\n", -1);

  PRPrint(rv, sz);

  ServerAuthed();

  return 1;
}

int PRLoggedIn(char *rv, int sz)
{
  PRPrint(rv, sz);

  return 1;
}

int PRCheck(char *rv, int sz)
{
  char *p, *s;

  if (!sz)
    return 0;

  s = rv;

  s[sz - 1] = '\0';
  p = StrChr(s, '\t');
  if (p)
    *p++ = '\0';

  Print(va("Version: %s [%s]\r\n", s, (p && (*p == 'r')) ? "ranked" : (p && (*p == 's')) ? "non-ranked" : "unknown"));

  return 1;
}

/*
Id:  1 - !BEST! |Test|Dummy69 is remote ip: 127.0.0.1:1234 ->.  
CD-key hash: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx .
Id:  5 - rockgang78 is remote ip: 127.0.0.1:1234 ->.
CD-key hash: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx .
Id: 10 - geronemo is remote ip: 127.0.0.1:1234 ->. 
CD-key hash: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.
.
*/

int PRSmatch(char *rv, int sz)
{
  char *name;
  int i;

  if (((i = UpdatePlayersSimple(rv, sz)) == -1))
    return 0;

  name = (char *)HashGet(internal, INTERNAL_SMATCH_NAME);
  PrintMatchPlayer(&PrintPlayerSimple, name, NULL, i);

  return 1;
}

int PRMatch(char *rv, int sz)
{
  char *name, *filter;
  int i;

  if (((i = UpdatePlayersFull(rv, sz)) == -1))
    return 0;

  name = (char *)HashGet(internal, INTERNAL_MATCH_NAME);
  filter = (char *)HashGet(internal, INTERNAL_MATCH_FILTER);
  PrintMatchPlayer(&PrintPlayerFull, name, filter, i);

  return 1;
}

int PRServerChat(char *rv, int sz)
{
  char *ch[MAX_PLAYERCHAT];
  char *pfx[MAX_PLAYERPREFIX] = {"", "<TEAM>", "<SQUAD>", "<COMMANDER>", "*DEAD*"};
  char chop[MAX_PLAYERCHOP+1];
  char *chat, *prefix;
  char *s, *p, *q;
  int i, dead;
  serverinfo_t *si;

  si = HashGet(internal, INTERNAL_SERVER_INFO);
  if (!si)
    return 1;

  s = rv;

  while (*s && *s != '\n')
  {
    q = StrChr(s, '\r');
    if (q)
      *q++ = '\0';
    
    for (i = 0; i < 6; i++)
    {
      p = StrChr(s, '\t');
      if (p)
        *p++ = '\0';

      switch (i)
      {
        case 0: ch[SERVERCHAT_PLAYERID] = s; break;
        case 1: ch[SERVERCHAT_PLAYERNAME] = s; break;
        case 2: ch[SERVERCHAT_TEAM] = s; break;
        case 3: ch[SERVERCHAT_CHANNEL] = s; break;
        case 4: ch[SERVERCHAT_TIME] = s; break;
        case 5: ch[SERVERCHAT_TEXT] = s; break;
      }

      s = p;
    }

    i = StrToInt(ch[SERVERCHAT_TEAM]);
    if (i == 1)
      ch[SERVERCHAT_TEAM] = si->team1.name;
    else if (i == 2)
      ch[SERVERCHAT_TEAM] = si->team2.name;

    dead = 0;
    i = 0;

    if (!StrCmpN(ch[SERVERCHAT_TEXT], "HUD_TEXT_CHAT_TEAM", 18))
    {
      prefix = pfx[CHARPREFIX_TEAM];
      chat = ch[SERVERCHAT_TEXT] + 18;
    }
    else if (!StrCmpN(ch[SERVERCHAT_TEXT], "HUD_TEXT_CHAT_SQUAD", 19))
    {
      prefix = pfx[CHARPREFIX_SQUAD];
      chat = ch[SERVERCHAT_TEXT] + 19;
    }
    else if (!StrCmpN(ch[SERVERCHAT_TEXT], "HUD_TEXT_CHAT_COMMANDER", 23))
    {
      prefix = pfx[CHARPREFIX_COMMANDER];
      chat = ch[SERVERCHAT_TEXT] + 23;
    }
    else
    {
      prefix = pfx[CHARPREFIX_NONE];
      chat = ch[SERVERCHAT_TEXT];
    }

    if (!StrCmpN(chat, "HUD_CHAT_DEADPREFIX", 19))
    {
      dead = 1;
      chat += 19;
    }
    else if (!StrCmpN(chat, "*§1DEAD§0*", 10))
    {
      dead = 1;
      chat += 10;
    }

    while (*chat && *chat == ' ')
    {
      i++;
      chat++;
    }

    wnsprintf(chop, sizeof(chop), " (%d)", i);

    Print(va("%s %s %s%s%s%s: %s\r\n",
      ch[SERVERCHAT_TIME],
      ch[SERVERCHAT_TEAM],
      (dead ? pfx[CHARPREFIX_DEAD] : pfx[CHARPREFIX_NONE]),
      prefix,
      ch[SERVERCHAT_PLAYERNAME],
      (i) ? chop : pfx[CHARPREFIX_NONE],
      chat
      ));

    s = ++q;
  }

  return 1;
}

//4.6.1.32.0.0.strike_at_karkand.dalian_plant.Stuff 2002.MEC.0.200.200.0.US.0.220.220.0.65969.-1.gpm_cq.bf2.(1024, 1024).0.0.0.0.0.175049.290022.0}}.
int PRServerInfo(char *rv, int sz)
{
  char *s, *p;
  int i;
  serverinfo_t *si;

  if (!sz)
    return 0;

  si = HashGet(internal, INTERNAL_SERVER_INFO);
  if (!si)
    return 1; //oops...

  s = rv;
  for (i = 0; i < 29; i++)
  {
    p = StrChr(s, '\t');
    if (!p)
      return 0;
    *p++ = '\0';

    switch (i)
    {
      case 0: StrCpyN(si->scriptver, s, sizeof(si->scriptver));
              si->scriptver[sizeof(si->scriptver) - 1] = '\0';
              break;

      case 1: si->state = StrToInt(s);
              break;

      case 2: si->maxplayers = StrToInt(s);
              break;

      case 3: si->connectedplayers = StrToInt(s);
              break;

      case 4: si->joiningplayers = StrToInt(s);
              break;

      case 5: StrCpyN(si->currentmap, s, sizeof(si->currentmap));
              si->currentmap[sizeof(si->currentmap) - 1] = '\0';
              break;

      case 6: StrCpyN(si->nextmap, s, sizeof(si->nextmap));
              si->nextmap[sizeof(si->nextmap) - 1] = '\0';
              break;

      case 7: StrCpyN(si->hostname, s, sizeof(si->hostname));
              si->hostname[sizeof(si->hostname) - 1] = '\0';
              break;

      //team 1
      case 8: StrCpyN(si->team1.name, s, sizeof(si->team1.name));
              si->team1.name[sizeof(si->team1.name) - 1] = '\0';
              break;

      case 9: si->team1.state = StrToInt(s);
              break;

      case 10: si->team1.starttickets = StrToInt(s);
              break;

      case 11: si->team1.tickets = StrToInt(s);
              break;

      case 12: si->team1.ticketrate = StrToInt(s);
              break;

      //team 2
      case 13: StrCpyN(si->team2.name, s, sizeof(si->team2.name));
               si->team2.name[sizeof(si->team2.name) - 1] = '\0';
               break;

      case 14: si->team2.state = StrToInt(s);
               break;

      case 15: si->team2.starttickets = StrToInt(s);
               break;

      case 16: si->team2.tickets = StrToInt(s);
               break;

      case 17: si->team2.ticketrate = StrToInt(s);
               break;

      //maps
      case 18: si->maptime = StrToInt(s);
               break;

      case 19: si->mapleft = StrToInt(s);
               break;

      case 20: StrCpyN(si->gamemode, s, sizeof(si->gamemode));
               si->gamemode[sizeof(si->gamemode) - 1] = '\0';
               break; 

      case 21: StrCpyN(si->moddir, s, sizeof(si->moddir));
               si->moddir[sizeof(si->moddir) - 1] = '\0';
               break;

      case 22: StrCpyN(si->worldsize, s, sizeof(si->worldsize));
               si->worldsize[sizeof(si->worldsize) - 1] = '\0';
               break;

      case 23: si->timelimit = StrToInt(s);
               break;

      case 24: si->autobalance = StrToInt(s);
               break;

      case 25: si->ranked = StrToInt(s);
               break;

      case 26: si->team1.players = StrToInt(s);
               break;

      case 27: si->team2.players = StrToInt(s);
               break;

      case 28: si->querytime = StrToInt(s);
               break;

      case 29: si->reserved = StrToInt(s);
               break;

      default: break;
    }

    s = p;
  }

  Print(va("%s - %d+%d/%d-%d %s (%s) [%s/%s]\r\n",
    si->hostname,
    si->connectedplayers,
    si->joiningplayers,
    si->maxplayers,
    si->reserved,
    si->currentmap,
    (*si->nextmap) ? si->nextmap : "none",
    si->moddir,
    si->gamemode
    ));

  Print(va("  %s ver:%s time:%d left:%d limit:%d auto:%d sz:%s\r\n",
    (si->ranked) ? "ranked" : "non-ranked",
    si->scriptver,
    si->maptime,
    si->mapleft,
    si->timelimit,
    si->autobalance,
    si->worldsize
    ));

  Print(va("  Team1 %3s: %d %d/%d (%d)\r\n",
    si->team1.name,
    si->team1.players,
    si->team1.tickets,
    si->team1.starttickets,
    si->team1.ticketrate
    ));

  Print(va("  Team2 %3s: %d %d/%d (%d)\r\n",
    si->team2.name,
    si->team2.players,
    si->team2.tickets,
    si->team2.starttickets,
    si->team2.ticketrate
    ));

  return 1;
}

int PRPrint(char *rv, int sz)
{
  char *s, *p;

  if (rv[sz - 1] == '\x04')
    rv[sz - 1] = '\0';

  s = rv;
  while (s && *s)
  {
    p = StrChr(s, '\n');
    if (p)
      *p++ = '\0';
    Print(s);
    Print("\r\n");
    s = p;
  }

  return 1;
}

int UpdatePlayersFull(char *rv, int sz)
{
  player_t *pl;
  char *s, *p, *q, *r, *name;
  int i, j, id, ticks;

  q = rv;
  i = 0;

  ticks = (int)GetTickCount();

  while (*q && *q != '\x04')
  {
    r = StrChr(q, '\r');
    if (r)
      *r++ = '\0';

    name = NULL;
    s = q;

    for (j = 0; j < 2; j++)
    {
      p = StrChr(s, '\t');
      if (!p)
        return -1;
      *p++ = '\0';

      switch (j)
      {
        case 0: id = StrToInt(s);
                break;

        case 1: name = s;
                break;
      }

      s = p;
    }

#ifdef _DEBUG
    if (!name)
      continue;
#endif

    pl = AddPlayer(name);
    if (!pl)
      return -1;

    StrCpyN(pl->name, name, sizeof(pl->name));
    pl->name[sizeof(pl->name) - 1] = '\0';
    pl->id = id;
    pl->ticks = ticks;

    for (j = 0; j < 41; j++)
    {
      p = StrChr(s, '\t');
      if (p)
        *p++ = '\0';

      switch (j)
      {
        case 0: pl->team = StrToInt(s);
                break;

        case 1: pl->ping = StrToInt(s);
                break;

        case 2: pl->connected = StrToInt(s);
                break;

        case 3: pl->valid = StrToInt(s);
                break;

        case 4: pl->remote = StrToInt(s);
                break;

        case 5: pl->aiplayer = StrToInt(s);
                break;

        case 6: pl->alive = StrToInt(s);
                break;

        case 7: pl->mandown = StrToInt(s);
                break;

        case 8: pl->profileid = StrToInt(s);
                 break;

        case 9: pl->hasflag = StrToInt(s);
                 break;

        case 10: pl->suicide = StrToInt(s);
                 break;

        case 11: pl->spawning = StrToInt(s);
                 break;

        case 12: pl->squadid = StrToInt(s);
                 break;

        case 13: pl->squadleader = StrToInt(s);
                 break;

        case 14: pl->commander = StrToInt(s);
                 break;

        case 15: pl->spawngroup = StrToInt(s);
                 break;

        case 16: StrCpyN(pl->ip, s, sizeof(pl->ip));
                 pl->ip[sizeof(pl->ip) - 1] = '\0';
                 break;

        case 17: pl->score.damageassist = StrToInt(s);
                 break;

        case 18: pl->score.passengerassist = StrToInt(s);
                 break;

        case 19: pl->score.targetassist = StrToInt(s);
                 break;

        case 20: pl->score.revives = StrToInt(s);
                 break;

        case 21: pl->score.teamdamage = StrToInt(s);
                 break;

        case 22: pl->score.teamvehicledamage = StrToInt(s);
                 break;

        case 23: pl->score.captures = StrToInt(s);
                 break;

        case 24: pl->score.defends = StrToInt(s);
                 break;

        case 25: pl->score.assists = StrToInt(s);
                 break;

        case 26: pl->score.neutralizes = StrToInt(s);
                 break;

        case 27: pl->score.neutralizeassists = StrToInt(s);
                 break;

        case 28: pl->score.suicides = StrToInt(s);
                 break;

        case 29: pl->score.frags = StrToInt(s);
                 break;

        case 30: pl->score.teamkills = StrToInt(s);
                 break;

        case 31: StrCpyN(pl->vehicletype, s, sizeof(pl->vehicletype));
                 pl->vehicletype[sizeof(pl->vehicletype) - 1] = '\0';
                 break;

        case 32: StrCpyN(pl->kit, s, sizeof(pl->kit));
                 pl->kit[sizeof(pl->kit) - 1] = '\0';
                 break;

        case 33: pl->connectedat = StrToInt(s);
                 break;

        case 34: pl->score.deaths = StrToInt(s);
                 break;

        case 35: pl->score.score = StrToInt(s);
                 break;

        case 36: StrCpyN(pl->vehiclename, s, sizeof(pl->vehiclename));
                 pl->vehiclename[sizeof(pl->vehiclename) - 1] = '\0';
                 break;

        case 37: pl->rank = StrToInt(s);
                 break;

        case 38: StrCpyN(pl->position, s, sizeof(pl->position));
                 pl->position[sizeof(pl->position) - 1] = '\0';
                 break;

        case 39: pl->idletime = StrToInt(s);
                 break;

        case 40: StrCpyN(pl->hash, s, sizeof(pl->hash));
                 pl->hash[sizeof(pl->hash) - 1] = '\0';
                 break;
      }

      s = p;
    }

    i++;

    q = r;
  }

  CleanPlayers(ticks);

  return i;
}

int UpdatePlayersSimple(char *rv, int sz)
{
  player_t *pl;
  char *ppl[MAX_SMATCH];
  char *s, *p, *mid;
  int i, ticks;

  p = rv;
  i = 0;

  if (*p == '\n' && sz == 2)
    return 0;

  ticks = (int)GetTickCount();

  while (*p && *p != '\x04')
  {
    mid = StrChr(p, '\n');
    if (!mid)
      return -1;

    *mid++ = '\0';

    p += 4;
    while (*p == ' ')
      p++;

    s = StrChr(p, ' ');
    if (!s)
      return -1;

    *s++ = '\0';

    ppl[SMATCH_ID] = p;

    s += 2;

    ppl[SMATCH_NAME] = s;

    if (*(mid - 2) == '>' && *(mid - 3) == '-')
    {
      p = mid - 4;
      *p-- = '\0';
    }
    else
    {
      p = mid - 2;
    }

    while (*p && *p != ' ')
      p--;

    ppl[SMATCH_IP] = p + 1;

    p -= 4;
    *p-- = '\0';
    while (*p && *p != ' ')
      p--;

    ppl[SMATCH_POINT] = p + 1;

    p -= 3;
    *p = '\0';

    if (*(mid - 2) == '>' && *(mid - 3) == '-')
    {
      p = StrChr(mid, '\n');
      if (!p)
        return -1;

      *(p++ - 1) = '\0';

      ppl[SMATCH_KEY] = mid + 22;
    }
    else
    {
      ppl[SMATCH_KEY] = NULL;
    }

    pl = AddPlayer(ppl[SMATCH_NAME]);
    if (!pl)
      continue;

    pl->id = StrToInt(ppl[SMATCH_ID]);

    if (ppl[SMATCH_KEY])
    {
      StrCpyN(pl->hash, ppl[SMATCH_KEY], sizeof(pl->hash));
      pl->hash[sizeof(pl->hash) - 1] = '\0';
    }
    else
    {
      pl->hash[0] = '\0';
    }

    s = StrChr(ppl[SMATCH_IP], ':');
    if (s)
      *s++ = '\0';

    StrCpyN(pl->ip, ppl[SMATCH_IP], sizeof(pl->ip));
    pl->ip[sizeof(pl->ip) - 1] = '\0';

    if (s)
    {
      StrCpyN(pl->port, s, sizeof(pl->port));
      pl->port[sizeof(pl->port) - 1] = '\0';
    }

    //StrCpyN(pl->point, ppl[SMATCH_POINT], sizeof(pl->point));
    //pl->point[sizeof(pl->point) - 1] = '\0';

    pl->ticks = ticks;

    i++;
  }

  CleanPlayers(ticks);

  return i;
}

int PrintMatchPlayer(void *func, char *match, char *filter, int count)
{
  playerprint_func display;
  serverinfo_t *si;
  player_t *pl, *best;
  any_t *a;
  int i, j, fl, flags, resolve = 0;
  char *s;

  if (count)
  {
    display = *(playerprint_func *)&func;
    best = NULL;
    i = 0;

    si = HashGet(internal, INTERNAL_SERVER_INFO);
    if (!si)
      return 1; //oops...

    if ((s = (char *)HashGet(cvars, CFG_RESOLVE_IP)) && ((*s == 'T') || (*s == 't')))
      resolve = 1;

    flags = MatchFilterFlags(filter);

    for (a = players->head; a; a = a->nextany)
    {
      pl = (player_t *)a->value;

      if (match && !StrStrI(pl->name, match))
        continue;
      else if (flags)
      {
        fl = 0;

        if ((flags & FILTER_MINUSSCORE) && (pl->score.score < 0))
          fl += FILTER_MINUSSCORE;
        if ((flags & FILTER_ALIVE) && (pl->alive || pl->mandown))
          fl += FILTER_ALIVE;
        if ((flags & FILTER_DEAD) && (!pl->alive && !pl->mandown))
          fl += FILTER_DEAD;
        if ((flags & FILTER_TEAMKILL) && pl->score.teamkills)
          fl += FILTER_TEAMKILL;
        if (flags & FILTER_VEHICLE)
        {
          j = StrToInt(pl->vehicletype);
          if (j != 11 && j != 10)
            fl += FILTER_VEHICLE;
        }
        if ((flags & FILTER_CONNECTING) && !pl->connected)
          fl += FILTER_CONNECTING;
        if ((flags & FILTER_CONNECTED) && pl->connected)
          fl += FILTER_CONNECTED;
        if ((flags & FILTER_COMMANDER) && pl->commander)
          fl += FILTER_COMMANDER;
        if ((flags & FILTER_SQUADLEADER) && pl->squadleader)
          fl += FILTER_SQUADLEADER;
        if ((flags & FILTER_SQUADMEMBER) && pl->squadid)
          fl += FILTER_SQUADMEMBER;
        if ((flags & FILTER_LONEWOLF) && (!pl->squadid && !pl->commander))
          fl += FILTER_LONEWOLF;
        if ((flags & FILTER_HIGHPING) && (pl->ping >= 150))
          fl += FILTER_HIGHPING;

        if (!(flags & fl))
          continue;
      }

      best = pl;
      display(pl, si, resolve);

      i++;
    }

    if (match || flags)
    {
      if (i == 1 && best)
      {
        HashStrSet(internal, INTERNAL_TARGET, best->name);
        Print(va("  -- Target: %s\r\n", best->name));
      }
      else
      {      
        Print(va("%d matches, ", i));
      }
    }
  }

  Print(va("%d players.\r\n", count));

  return 1;
}

void CleanPlayers(int ticks)
{
  any_t *a, *n;
  player_t *pl;

  a = players->head;
  while (a)
  {
    pl = (player_t *)a->value;
#ifdef _DEBUG
    if (!pl)
      continue;
#endif
    n = a->nextany;
    if (pl->ticks != ticks)
      HashRemove(players, va("%s", pl->name));
    a = n;
  }
}

void PrintPlayerSimple(player_t *pl, serverinfo_t *si, int resolve)
{
  char dns[128] = {'\0'};

  if (resolve) {
    lookup(pl->ip, dns, sizeof(dns));
  }

  Print(va("  %02d %s @ %s %s %s\r\n",
      pl->id,
      pl->name,
      pl->ip,
      (*dns) ? dns : "unknown",
      (*pl->hash) ? pl->hash : "none"
      ));
}

void PrintPlayerFull(player_t *pl, serverinfo_t *si, int resolve)
{
  char dns[128] = {'\0'};

  if (resolve) {
    lookup(pl->ip, dns, sizeof(dns));
  }

  Print(va("  %02d %s %d/%d/%d [%s] @ %s %s\r\n",
      pl->id,
      pl->name,
      pl->score.score,
      pl->score.frags,
      pl->score.deaths,
      pl->kit,
      pl->ip,
      (*pl->hash) ? pl->hash : "none"
      ));

  Print(va("  %s  %s%s%s%s%d %s tk:%d/%d/%d [%s(%s)] rn:%d sc:%d/%d/%d as:%d/%d %dms %d %s\r\n",
      (pl->score.teamkills >= 3) ? "*" : "",
      (pl->connected) ? "P" : "J",
      (pl->commander) ? "C" : (pl->squadleader) ? "L" : (pl->squadid) ? "S" : "W",
      (pl->alive) ? "A" : (pl->mandown) ? "M" : "D",
      (pl->hasflag) ? "F" : "",
      pl->spawning,
      ((pl->team == 1) ? si->team1.name : (pl->team == 2) ? si->team2.name : "none"),
      pl->score.teamkills,
      pl->score.teamdamage,
      pl->score.teamvehicledamage,
      pl->vehiclename,
      pl->vehicletype,
      pl->rank,
      pl->score.captures,
      pl->score.neutralizes,
      pl->score.defends,
      pl->score.assists,
      (pl->score.damageassist + pl->score.passengerassist + pl->score.targetassist + pl->score.neutralizeassists),
      pl->ping,
      pl->profileid,
      (*dns) ? dns : "unknown"
      ));
}

/*//////////////////////////////////////////

MISC

//////////////////////////////////////////*/

player_t *GetTargetPlayer(void)
{
  char *p;
  player_t *pl;

  p = HashGet(internal, INTERNAL_TARGET);
  if (!p)
  {
#ifdef _DEBUG
    Print("Missing target.\r\n");
#endif
    return NULL;
  }

  pl = (player_t *)HashGet(players, p);
  if (!pl)
  {
#ifdef _DEBUG
    Print("Missing player.\r\n");
#endif
    return NULL;
  }

  HashRemove(internal, INTERNAL_TARGET);

  return pl;
}

void SendAnonPlayerMessage(int id, char *msg)
{
  char *p;

  AddRequest(PACKET_HIDE);
  SendSocket(va("\x02" "bf2cc setadminname " DEFAULT_ADMIN_NAME "\n"), -1);

  AddRequest(PACKET_HIDE);
  SendSocket(va("\x02" "bf2cc sendplayerchat %d %s\n", id, msg), -1);

  if ((p = (char *)HashGet(internal, INTERNAL_ADMIN_NAME)))
  {
    AddRequest(PACKET_HIDE);
    SendSocket(va("\x02" "bf2cc setadminname %s\n", p), -1);
  }
}

int MatchFilterFlags(char *filter)
{
  char *s;
  int flags;

  if (!filter)
    return 0;

  flags = 0;
  s = filter;

  while (*s)
  {
    switch (*s)
    {
      case 'm': flags |= FILTER_MINUSSCORE; break;
      case 'a': flags |= FILTER_ALIVE; break;
      case 'd': flags |= FILTER_DEAD; break;
      case 't': flags |= FILTER_TEAMKILL; break;
      case 'v': flags |= FILTER_VEHICLE; break;
      case 'j': flags |= FILTER_CONNECTING; break;
      case 'p': flags |= FILTER_CONNECTED; break;
      case 'c': flags |= FILTER_COMMANDER; break;
      case 'l': flags |= FILTER_SQUADLEADER; break;
      case 's': flags |= FILTER_SQUADMEMBER; break;
      case 'w': flags |= FILTER_LONEWOLF; break;
      case 'h': flags |= FILTER_HIGHPING; break;
    }
    s++;
  }

  return flags;
}

/*//////////////////////////////////////////

OBJECTS

//////////////////////////////////////////*/

int NewServerInfo(void)
{
  serverinfo_t *si;
  any_t *a;

  si = (serverinfo_t *)HEAP_ALLOC(sizeof(serverinfo_t));
  if (!si)
    return FALSE;

  a = HashAdd(internal, INTERNAL_SERVER_INFO);
  if (!a)
    return FALSE;

  a->value = (void *)si;

  return TRUE;
}

player_t *AddPlayer(char *name)
{
  player_t *pl;
  any_t *a;

  a = HashAdd(players, name);
  if (!a)
    return NULL;

  if (a->value)
    return (player_t *)a->value;

  pl = (player_t *)HEAP_ALLOC(sizeof(player_t));
  if (!pl)
    return NULL;

  StrCpyN(pl->name, name, sizeof(pl->name));
  pl->name[sizeof(pl->name) - 1] = '\0';

  pl->id = -1;
  pl->ip[0] = '\0';
  pl->port[0] = '\0';
  pl->hash[0] = '\0';
  //pl->point[0] = '\0';

  a->value = (void *)pl;

  return pl;
}

void AddRequest(int type)
{
  request_t *req;

  req = (request_t *)HEAP_ALLOC(sizeof(request_t));
  if (!req)
    return;

  req->type = type;
  req->recvlen = 0;
  req->recvbuf = NULL;
  req->next = NULL;

  if (reqtail)
    reqtail->next = req;
  reqtail = req;

  if (!reqhead)
    reqhead = req;

  SetTimer(hDlg, TIMER_PACKET_ID, TIMER_PACKET_MS, NULL);
}

void RemoveRequest(void)
{
  request_t *req;

  KillTimer(hDlg, TIMER_PACKET_ID);

  if (!reqhead)
    return;

  req = reqhead;
  reqhead = reqhead->next;

  if (reqtail && !reqtail->next)
    reqtail = NULL;

  HEAP_FREE(req);
}

void RemoveRequests(void)
{
  request_t *req;

  KillTimer(hDlg, TIMER_PACKET_ID);

  while (reqhead)
  {
    req = reqhead;
    reqhead = reqhead->next;
    if (req->recvbuf)
      HEAP_FREE(req);

    HEAP_FREE(req);
  }

  reqhead = NULL;
  reqtail = NULL;
}

