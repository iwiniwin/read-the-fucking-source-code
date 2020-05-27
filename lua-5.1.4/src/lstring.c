/*
** $Id: lstring.c,v 2.8.1.1 2007/12/27 13:02:25 roberto Exp $
** String table (keeps all strings handled by Lua)
** See Copyright Notice in lua.h
*/


#include <string.h>

#define lstring_c
#define LUA_CORE

#include "lua.h"

#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"


// �Ա����ַ����Ĺ�ϣͰ����resize
void luaS_resize (lua_State *L, int newsize) {
  GCObject **newhash;
  stringtable *tb;
  int i;
  if (G(L)->gcstate == GCSsweepstring)  // �����������ڻ����ַ�����������resize
    return;  /* cannot resize during GC traverse */
  newhash = luaM_newvector(L, newsize, GCObject *);
  tb = &G(L)->strt;
  for (i=0; i<newsize; i++) newhash[i] = NULL;
  /* rehash */
  for (i=0; i<tb->size; i++) {
    GCObject *p = tb->hash[i];
    while (p) {  /* for each node in the list */
      GCObject *next = p->gch.next;  /* save next */
      unsigned int h = gco2ts(p)->hash;
      int h1 = lmod(h, newsize);  /* new position */
      lua_assert(cast_int(h%newsize) == lmod(h, newsize));
      p->gch.next = newhash[h1];  /* chain it */
      newhash[h1] = p;
      p = next;
    }
  }
  luaM_freearray(L, tb->hash, tb->size, TString *);
  tb->size = newsize;
  tb->hash = newhash;
}

// �����µ��ַ���
static TString *newlstr (lua_State *L, const char *str, size_t l,
                                       unsigned int h) {
  TString *ts;
  stringtable *tb;
  if (l+1 > (MAX_SIZET - sizeof(TString))/sizeof(char))  // �ַ�������
    luaM_toobig(L);
  ts = cast(TString *, luaM_malloc(L, (l+1)*sizeof(char)+sizeof(TString)));
  ts->tsv.len = l;  // �����ַ�������
  ts->tsv.hash = h;  // �����ַ���hashֵ
  ts->tsv.marked = luaC_white(G(L));
  ts->tsv.tt = LUA_TSTRING;  // �����ַ�������
  ts->tsv.reserved = 0;
  memcpy(ts+1, str, l*sizeof(char));  // �����ַ������� str ���� l*sizeof(char) ���ֽڵ� ts+1
  ((char *)(ts+1))[l] = '\0';  /* ending 0 */  // ����c����ţ��Լ���c�ӿ�
  tb = &G(L)->strt;
  h = lmod(h, tb->size);  // ��hashֵ������Ϊ����
  ts->tsv.next = tb->hash[h];  /* chain new entry */
  tb->hash[h] = obj2gco(ts);  // ���´������ַ�����ӵ�ȫ���ַ�������
  tb->nuse++;
  // ���ȫ���ַ������е�Ԫ������������hashͰ�����С�������resize�ᷢ����ͻ
  // ����Ͱ����δ����MAX_INT��һ�룬�ͳɱ�����
  if (tb->nuse > cast(lu_int32, tb->size) && tb->size <= MAX_INT/2)
    luaS_resize(L, tb->size*2);  /* too crowded */
  return ts;
}

// �����µ��ַ���
TString *luaS_newlstr (lua_State *L, const char *str, size_t l) {
  GCObject *o;
  unsigned int h = cast(unsigned int, l);  /* seed */
  size_t step = (l>>5)+1;  /* if string is too long, don't hash all its chars */  // ����ַ���̫���Ͳ����ַ��Ƚϣ�����step
  size_t l1;
  for (l1=l; l1>=step; l1-=step)  /* compute hash */
    h = h ^ ((h<<5)+(h>>2)+cast(unsigned char, str[l1-1]));  // ����hashֵ
  for (o = G(L)->strt.hash[lmod(h, G(L)->strt.size)];  // ��global_State��stringtable�ṹ��hash���в����ַ����Ƿ��Ѿ�����
       o != NULL;
       o = o->gch.next) {
    TString *ts = rawgco2ts(o);
	// �Ƚ��Ƿ���Ŀ���ַ�����memcmp�Ƚ��ڴ��ǰl���ֽ�
    if (ts->tsv.len == l && (memcmp(str, getstr(ts), l) == 0)) {
      /* string may be dead */
	  // ����lua�������ռ������Ƿֲ���ɵģ���������ַ������κβ���֮�䶼�п��ܷ���
      if (isdead(G(L), o)) changewhite(o); // ����������ַ�������ָ��׵����
      return ts;
    }
  }
  // δ��ȫ�ֱ����ҵ����������ַ���
  return newlstr(L, str, l, h);  /* not found */
}

// �����µ�userdata
// userdata�ڴ洢��ʽ�Ϻ��ַ������ƣ����Կ�����ӵ�ж���Ԫ�������ڲ�������Ҳ����Ҫ׷��\0���ַ���
Udata *luaS_newudata (lua_State *L, size_t s, Table *e) {
  Udata *u;
  if (s > MAX_SIZET - sizeof(Udata))
    luaM_toobig(L);
  u = cast(Udata *, luaM_malloc(L, s + sizeof(Udata)));
  u->uv.marked = luaC_white(G(L));  /* is not finalized */
  u->uv.tt = LUA_TUSERDATA;
  u->uv.len = s;
  u->uv.metatable = NULL;
  u->uv.env = e;
  /* chain it on udata list (after main thread) */
  u->uv.next = G(L)->mainthread->next;
  G(L)->mainthread->next = obj2gco(u);
  return u;
}

