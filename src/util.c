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

char *va(char *fmt, ...)
{
  static char buf[MAX_BUFFER_SIZE+1];
  va_list argptr;

	va_start(argptr, fmt);
	wvnsprintf(buf, MAX_BUFFER_SIZE, fmt, argptr);
	va_end(argptr);
  buf[MAX_BUFFER_SIZE] = '\0';
  
  return buf;
}

void md5(char *text, char *md5hex, int sz)
{
	md5_state_t state;
	md5_byte_t digest[16];
  char *s;
  int i;

  if (sz < 32 + 1)
  {
    *md5hex = '\0';
    return;
  }

  md5_init(&state);
  md5_append(&state, (const md5_byte_t *)text, lstrlen(text));
  md5_finish(&state, digest);

  s = md5hex;
  for (i = 0; i < 16; i++)
  {
    *s++ = HEX_CODES[digest[i] >> 4 & 0xF];
    *s++ = HEX_CODES[digest[i] & 0xF];
  }
  *s = '\0';
}

int validip(char *str)
{
  char *p;
  int i;
  int dot;

  i = 0;
  dot = 0;
  p = str;

  while (*p)
  {
    if (*p == ':')
      break;
    if (!((*p >= 48 && *p <= 57) || (*p == 46)))
      return 0;
    if (*p == 46)
      dot++;
    i++;
    *p++;
  }

  if ((dot == 3) && (i < 16))
    return 1;

  return 0;
}

int lookup(char *ip, char *buf, int sz)
{
	struct hostent *hp;
  unsigned int addr;

  addr = inet_addr(ip);
  hp = gethostbyaddr((char *) &addr, 4, AF_INET);
  buf[0] = '\0';
  if (!hp)
    return 0;

  StrCpyN(buf, hp->h_name, sz);
  buf[sz - 1] = '\0';

  return 1;
}

int hexcode(char *str, int *fd)
{
  char c;
  int i;

  *fd = 1;

  if (*str != '\\' || str[1] != 'x')
    return *str;

  c = str[2];
  CharLower(&c);
  if (c >= '0' && c <= '9')
    i = (c - '0') << 4;
  else if (c >= 'a' && c <= 'f')
	  i = (c - 'a' + 10) << 4;
  else
    return *str;

  c = str[3];
  CharLower(&c);
  if (c >= '0' && c <= '9')
	  i += (c - '0');
  else if (c >= 'a' && c <= 'f')
    i += (c - 'a' + 10);
  else
    return *str;

  *fd = 4;

  return i;
}

int clamp(int num, int min, int max)
{
  if (num > max)
    return max;
  else if (num < min)
    return min;
  else
    return num;
}

#ifdef _DEBUG
void __declspec (naked) __cdecl _chkesp (void)
{
  _asm { jz exit_chkesp };
  _asm { int 3          };
  exit_chkesp:
  _asm { ret            };
}
#endif

