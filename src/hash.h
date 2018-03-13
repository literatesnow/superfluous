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

#define MAX_HASH_NAME 32

typedef struct any_s
{
  char name[MAX_HASH_NAME+1];
  void *value;
  struct any_s *prev;
  struct any_s *next;
  struct any_s *prevany;
  struct any_s *nextany;
} any_t;

typedef struct hashtable_s
{
  struct any_s **table;
  struct any_s *head;
  int size; //size if hash array
  int count; //items in hash
} hashtable_t;

any_t *HashAdd(hashtable_t *hash, char *name);
void *HashGet(hashtable_t *hash, char *name);
any_t *HashGetItem(hashtable_t *hash, char *name);
void HashRemove(hashtable_t *hash, char *name);
void HashClear(hashtable_t *hash);
void HashStrSet(hashtable_t *hash, char *key, char *value);
void HashIntSet(hashtable_t *hash, char *key, int value);

hashtable_t *NewHashTable(int sz);
any_t *NewAny(any_t **parent, any_t **head);
void RemoveHashTable(hashtable_t **hash);

void SetCvar(hashtable_t *hash, char *key, char *value);

int hashstr(char *str, int sz);
int isstrset(hashtable_t *hash, char *key);
int isstrequals(hashtable_t *hash, char *key, char *value);

