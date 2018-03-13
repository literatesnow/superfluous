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

#define _WIN32_WINNT 0x0400

#include <windows.h>
#include <windowsx.h>
#include <winuser.h>
#include <shlwapi.h>
#include <ShellAPI.h>
#include "AggressiveOptimize.h"
#include "resource.h"
#include "hash.h"
#include "md5.h"

#define AUTHOR "bliP"
#define COPYRIGHT "Copyright (C) 2005-2007"
#define CONTACT "http://nisda.net"
#define VER_PRODUCTNAME "Superfluous"
#define VER_VERSION "1.04"
#define VER_FILEVERSION 1,0,0,4
#define VER_PRODUCTVERSION 1,0,0,4
#define VER_COPYRIGHT COPYRIGHT " " AUTHOR
#define VER_DESCRIPTION "Superfluous"
#define VER_INTERNALNAME "superfluous.exe"
#define VER_PRODUCTNAME "Superfluous"

#define CONFIG_FILE "superfluous.conf"

#define HEAP_ALLOC(x) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, x)
#define HEAP_REALLOC(x, y) HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (LPVOID)x, y)
#define HEAP_FREE(x) HeapFree(GetProcessHeap(), 0, (LPVOID)x)

#define UC_AASYNC (WM_APP + 10)

//50 connect
//51 packet
//52 serverchat
#define TIMER_CONNECT_ID 50
#define TIMER_CONNECT_MS (8 * 1000)
#define TIMER_PACKET_ID 51
#define TIMER_PACKET_MS (45 * 1000)
#define TIMER_SERVERCHAT_ID 52
#define TIMER_SERVERCHAT_MS (10 * 1000)
#define TIMER_SERVERCHAT_MIN_MS (3 * 1000)
#define TIMER_SERVERCHAT_MAX_MS (120 * 1000)
#define TIMER_RECONNECT_ID 54
#define TIMER_RECONNECT_MS (15 * 1000)

#define TIMER_KICKBEGIN_ID 100
#define TIMER_KICKEND_ID 250
#define TIMER_KICK_MS (10 * 1000)

#define TIMER_BANBEGIN_ID 300
#define TIMER_BANEND_ID 450
#define TIMER_BAN_MS (10 * 1000)

#define DEFAULT_RCON_PORT 4711
#define DEFAULT_ADMIN_NAME "Admin"
#define DEFAULT_KICK_PREFIX "Kicking"
#define DEFAULT_BAN_PREFIX "Banning"
#define DEFAULT_SEPARATOR ":"

#define MAX_COMMANDS 16
#define MAX_EDIT_LIMIT 30000

#define MAX_BUFFER_SIZE 1024
#define MAX_COMMAND 512
#define MAX_COMMAND_ARGV 5
#define MAX_RECIEVE 4080
#define MAX_WELCOME 17
#define MAX_MD5 32
#define MAX_SEED 16

#define MAX_IP 32
#define MAX_RCON 64

#define MAX_SMATCH 5
#define MAX_PLAYERCHAT 6
#define MAX_PLAYERPREFIX 5
#define MAX_PLAYERCHOP 8

#define PACKET_RAW 1
#define PACKET_HIDE 2
#define PACKET_PRINT 3
#define PACKET_WELCOME 4
#define PACKET_ISLOGIN 5
#define PACKET_LOGGEDIN 6
#define PACKET_CHECK 7
#define PACKET_SMATCH 8
#define PACKET_MATCH 9
#define PACKET_SERVERINFO 10
#define PACKET_SERVERCHAT 11

#define SMATCH_ID 0
#define SMATCH_NAME 1
#define SMATCH_POINT 2
#define SMATCH_IP 3
#define SMATCH_KEY 4

#define SERVERCHAT_PLAYERID 0
#define SERVERCHAT_PLAYERNAME 1
#define SERVERCHAT_TEAM 2
#define SERVERCHAT_CHANNEL 3
#define SERVERCHAT_TIME 4
#define SERVERCHAT_TEXT 5

#define CHARPREFIX_NONE 0
#define CHARPREFIX_TEAM 1
#define CHARPREFIX_SQUAD 2
#define CHARPREFIX_COMMANDER 3
#define CHARPREFIX_DEAD 4

#define GAME_STATUS_PLAYING 1
#define GAME_STATUS_END_GAME 2
#define GAME_STATUS_PRE_GAME 3
#define GAME_STATUS_PAUSED 4
#define GAME_STATUS_RESTART_SERVER 5
#define GAME_STATUS_NOT_CONNECTED 6

#define MAX_CVARS 16
#define MAX_INTERNALS 8
#define MAX_PLAYERS 64

#define MAX_PLAYER_NAME 64
#define MAX_PLAYER_IP 15
#define MAX_PLAYER_PORT 5
#define MAX_PLAYER_HASH 32
#define MAX_PLAYER_POINT 6
#define MAX_PLAYER_VEHICLE 32
#define MAX_PLAYER_KIT 16
#define MAX_PLAYER_POSITION 32

#define MAX_TEAM_NAME 8
#define MAX_SCRIPT_VERSION 8
#define MAX_MAP_NAME 64
#define MAX_HOST_NAME 48
#define MAX_GAME_MODE 16
#define MAX_MOD_DIR 16
#define MAX_WORLD_SIZE 24

#define CFG_RCON_PASSWORD "_rcon_password"
#define CFG_ADMIN_NAME "_admin_name"
#define CFG_KICK_PREFIX "_kick_prefix"
#define CFG_BAN_PREFIX "_ban_prefix"
#define CFG_SEPARATOR "_separator"
#define CFG_RESOLVE_IP "_resolve_ip"

#define INTERNAL_EXT_FILE_NAME "nve_file_name"
#define INTERNAL_EXT_FILE_POS "nve_file_pos"
#define INTERNAL_RCON_PASSWORD "rcon_password"
#define INTERNAL_ADMIN_NAME "admin_name"
#define INTERNAL_SMATCH_NAME "smatch_name"
#define INTERNAL_MATCH_NAME "match_name"
#define INTERNAL_MATCH_FILTER "match_filter"
#define INTERNAL_TARGET "target"
#define INTERNAL_SERVER_INFO "server_info"
#define INTERNAL_BOOT_TARGET "boot_target_"
#define INTERNAL_BOOT_REASON "boot_reason_"
#define INTERNAL_SERVER_IP "server_ip"
#define INTERNAL_SERVER_PORT "server_port"

#define FILTER_MINUSSCORE 1
#define FILTER_ALIVE 2
#define FILTER_DEAD 4
#define FILTER_TEAMKILL 8
#define FILTER_VEHICLE 16
#define FILTER_CONNECTING 32
#define FILTER_CONNECTED 64
#define FILTER_COMMANDER 128
#define FILTER_SQUADLEADER 256
#define FILTER_SQUADMEMBER 512
#define FILTER_LONEWOLF 1024
#define FILTER_HIGHPING 2048

#define LINE_LENGTH 17
#define HEX_CODES "0123456789abcdef"

typedef void (*command_func)(char **argv, int argc);

typedef struct command_s
{
  command_func command;
  char *name;
  char *help;
  int args;
  int connected;
} command_t;

typedef struct request_s
{
  int type;
  int recvlen;
  char *recvbuf;
  struct request_s *next;
} request_t;

typedef struct score_s
{
  int score;
  int frags;
  int deaths;
  int suicides;
  int teamkills;
  int damageassist;
  int passengerassist;
  int targetassist;
  int revives;
  int teamdamage;
  int teamvehicledamage;
  int captures;
  int defends;
  int assists;
  int neutralizes;
  int neutralizeassists;
} score_t;

typedef struct player_s
{
  char name[MAX_PLAYER_NAME+1];
  char ip[MAX_PLAYER_IP+1];
  char port[MAX_PLAYER_PORT+1];
  char hash[MAX_PLAYER_HASH+1];
  char vehiclename[MAX_PLAYER_VEHICLE+1];
  char vehicletype[MAX_PLAYER_VEHICLE+1];
  char kit[MAX_PLAYER_KIT+1];
  char position[MAX_PLAYER_POSITION+1];
  int id;
  int team;
  int ping;
  int connected;
  int valid;
  int remote;
  int aiplayer;
  int alive;
  int mandown;
  int profileid;
  int hasflag;
  int suicide;
  int spawning;
  int squadid;
  int squadleader;
  int commander;
  int spawngroup;
  int connectedat;
  int rank;
  int idletime;
  int ticks;
  struct score_s score;
} player_t;

typedef struct team_s
{
  char name[MAX_TEAM_NAME+1];
  int players;
  int state;
  int starttickets;
  int tickets;
  int ticketrate;
} team_t;

typedef struct serverinfo_s
{
  char scriptver[MAX_SCRIPT_VERSION+1];
  char currentmap[MAX_MAP_NAME+1];
  char nextmap[MAX_MAP_NAME+1];
  char hostname[MAX_HOST_NAME+1];
  char gamemode[MAX_GAME_MODE+1];
  char moddir[MAX_MOD_DIR+1];
  char worldsize[MAX_WORLD_SIZE+1];
  int state;
  int maxplayers;
  int connectedplayers;
  int joiningplayers;
  int maptime;
  int mapleft;
  int timelimit;
  int autobalance;
  int ranked;
  int querytime;
  int reserved;
  struct team_s team1;
  struct team_s team2;
} serverinfo_t;

typedef void (*playerprint_func)(player_t *pl, serverinfo_t *si, int resolve);

void ProcessCommand(void);
void RunCommand(char *line);
command_t *FindCommand(char *name);

void ServerConnect(char **argv, int argc);
void ServerDisconnect(char **argv, int argc);
void SimpleMatchPlayer(char **argv, int argc);
void MatchPlayer(char **argv, int argc);
void RawCommand(char **argv, int argc);
void KickPlayer(char **argv, int argc);
void BanPlayer(char **argv, int argc);
void About(char **argv, int argc);
void ExecCommand(char **argv, int argc);
void ServerInfo(char **argv, int argc);
void SwapPlayer(char **argv, int argc);
void SayAll(char **argv, int argc);
//void SayPlayer(char **argv, int argc);
void ServerChat(char **argv, int argc);
void Quit(char **argv, int argc);

void ReadSocket(void);
void SendSocket(char *data, int sz);
void ServerAuthed(void);
void ServerReconnect(void);
void ServerReconnectAttempt(void);
void ServerConnection(char *ip, int port);
void EndConnection(char *msg);
void CloseSocket(void);

int PRWelcome(char *rv, int sz);
int PRSeed(char *rv, int sz);
int PRLoginReply(char *rv, int sz);
int PRLoggedIn(char *rv, int sz);
int PRCheck(char *rv, int sz);
int PRSmatch(char *rv, int sz);
int PRMatch(char *rv, int sz);
int PRServerInfo(char *rv, int sz);
int PRPrint(char *rv, int sz);
int PRServerChat(char *rv, int sz);

void ClientChat(void);
int UpdatePlayersSimple(char *rv, int sz);
int UpdatePlayersFull(char *rv, int sz);
int PrintMatchPlayer(void *func, char *match, char *filter, int count);
void BootPlayer(int type, char *global, char *message);
void PrintPlayerSimple(player_t *pl, serverinfo_t *si, int resolve);
void PrintPlayerFull(player_t *pl, serverinfo_t *si, int resolve);
void CleanPlayers(int ticks);
player_t *GetTargetPlayer(void);
void SendAnonPlayerMessage(int id, char *msg);
int MatchFilterFlags(char *filter);

void Print(char *text);
void ShowPacket(char *rv, int sz);

void OnSize(HWND hwnd, UINT state, int cx, int cy);
void OnPrintClient(HWND hwnd, HDC hdc);
void OnPaint(HWND hwnd);
void PaintContent(HWND hwnd, PAINTSTRUCT *pps);
BOOL OnCreate(HWND hwnd, HWND hwndFocus, LPARAM lParam);
void OnDestroy(HWND hwnd);
void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);
void OnTimer(HWND hWnd, UINT id);
//void OnKey(HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags);
BOOL CALLBACK MainDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam);

BOOL InitApp(void);
void FinishApp(void);
void LoadConfig(void);

BOOL CALLBACK InputRconDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam);
BOOL OnInputRconCreate(HWND hwnd, HWND hwndFocus, LPARAM lParam);
void OnInputRconCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);

BOOL CALLBACK AboutDlgProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam);
BOOL OnAboutCreate(HWND hwnd, HWND hwndFocus, LPARAM lParam);
void OnAboutCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);

VOID CALLBACK TimedBootProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

int NewServerInfo(void);
player_t *AddPlayer(char *name);
void AddRequest(int type);
void RemoveRequest(void);
void RemoveRequests(void);

#ifdef _DEBUG
void PrintCommands(void);
#endif

char *va(char *fmt, ...);
void md5(char *text, char *md5hex, int sz);
int hexcode(char *str, int *fd);
int validip(char *str);
int lookup(char *ip, char *buf, int sz);
int clamp(int num, int min, int max);

