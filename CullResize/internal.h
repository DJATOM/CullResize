// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
// http://www.avisynth.org

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.


#ifndef __Internal_H__
#define __Internal_H__

#include <cstdlib>
#include <avs/config.h>
#include <avs/minmax.h>
#include <stdint.h>
#include <tchar.h>

#define AVS_CLASSIC_VERSION 2.60  // Note: Used by VersionNumber() script function
#define AVS_COPYRIGHT "\n\xA9 2000-2015 Ben Rudiak-Gould, et al.\nhttp://avisynth.nl\n\xA9 2013-2016 AviSynth+ Project\nhttp://avs-plus.net"
#define BUILTIN_FUNC_PREFIX "AviSynth"

enum MANAGE_CACHE_KEYS
{
  MC_RegisterCache     = 0xFFFF0004,
  MC_UnRegisterCache   = 0xFFFF0006,
  MC_NodCache          = 0xFFFF0007,
  MC_NodAndExpandCache = 0xFFFF0008,
  MC_RegisterMTGuard,
  MC_UnRegisterMTGuard
};

#include <avisynth.h>
#include <emmintrin.h>


class Expression {
public:
	Expression() : refcnt(0) {}
	virtual AVSValue Evaluate(IScriptEnvironment* env) = 0;
	virtual const char* GetLvalue() { return 0; }
	virtual ~Expression() {}

private:
	friend class PExpression;
	int refcnt;
	void AddRef() { ++refcnt; }
	void Release() { if (--refcnt <= 0) delete this; }
};

class PExpression
{
public:
	PExpression() { Init(0); }
	PExpression(Expression* p) { Init(p); }
	PExpression(const PExpression& p) { Init(p.e); }
	PExpression& operator=(Expression* p) { Set(p); return *this; }
	PExpression& operator=(const PExpression& p) { Set(p.e); return *this; }
	int operator!() const { return !e; }
	operator void*() const { return e; }
	Expression* operator->() const { return e; }
	~PExpression() { Release(); }

private:
	Expression* e;
	void Init(Expression* p) { e = p; if (e) e->AddRef(); }
	void Set(Expression* p) { if (p) p->AddRef(); if (e) e->Release(); e = p; }
	void Release() { if (e) e->Release(); }
};

class ScriptFunction
	/**
	* Executes a script
	**/
{
public:
	ScriptFunction(const PExpression& _body, const bool* _param_floats, const char** _param_names, int param_count);
	virtual ~ScriptFunction()
	{
		delete[] param_floats;
		delete[] param_names;
	}

	static AVSValue Execute(AVSValue args, void* user_data, IScriptEnvironment* env);
	static void Delete(void* self, IScriptEnvironment*);

private:
	const PExpression body;
	bool *param_floats;
	const char** param_names;
};

class AVSFunction {

public:

  typedef AVSValue (__cdecl *apply_func_t)(AVSValue args, void* user_data, IScriptEnvironment* env);

  const apply_func_t apply;
  char* name;
  char* canon_name;
  char* param_types;
  void* user_data;
  char* dll_path;

  AVSFunction(void*);
  AVSFunction(const char* _name, const char* _plugin_basename, const char* _param_types, apply_func_t _apply);
  AVSFunction(const char* _name, const char* _plugin_basename, const char* _param_types, apply_func_t _apply, void *_user_data);
  AVSFunction(const char* _name, const char* _plugin_basename, const char* _param_types, apply_func_t _apply, void *_user_data, const char* _dll_path);
  ~AVSFunction();

  AVSFunction() = delete;
  AVSFunction(const AVSFunction&) = delete;
  AVSFunction& operator=(const AVSFunction&) = delete;
  AVSFunction(AVSFunction&&) = delete;
  AVSFunction& operator=(AVSFunction&&) = delete;

  bool empty() const;
  bool IsScriptFunction() const;
#ifdef DEBUG_GSCRIPTCLIP_MT
  bool IsRuntimeScriptFunction() const;
#endif

  static bool ArgNameMatch(const char* param_types, size_t args_names_count, const char* const* arg_names);
  static bool TypeMatch(const char* param_types, const AVSValue* args, size_t num_args, bool strict, IScriptEnvironment* env);
  static bool SingleTypeMatch(char type, const AVSValue& arg, bool strict);
};


int RGB2YUV(int rgb);
const char *GetPixelTypeName(const int pixel_type); // in script.c
const int GetPixelTypeFromName(const char *pixeltypename); // in script.c

PClip Create_MessageClip(const char* message, int width, int height,
  int pixel_type, bool shrink, int textcolor, int halocolor, int bgcolor,
  IScriptEnvironment* env);

PClip new_Splice(PClip _child1, PClip _child2, bool realign_sound, IScriptEnvironment* env);
PClip new_SeparateFields(PClip _child, IScriptEnvironment* env);
PClip new_AssumeFrameBased(PClip _child);


/* Used to clip/clamp a byte to the 0-255 range.
   Uses a look-up table internally for performance.
*/
class _PixelClip {
  enum { buffer=320 };
  BYTE lut[256+buffer*2];
public:
  _PixelClip() {  
    memset(lut, 0, buffer);
    for (int i=0; i<256; ++i) lut[i+buffer] = (BYTE)i;
    memset(lut+buffer+256, 255, buffer);
  }
  BYTE operator()(int i) const { return lut[i+buffer]; }
};

extern const _PixelClip PixelClip;


template<class ListNode>
static __inline void Relink(ListNode* newprev, ListNode* me, ListNode* newnext) {
  if (me == newprev || me == newnext) return;
  me->next->prev = me->prev;
  me->prev->next = me->next;
  me->prev = newprev;
  me->next = newnext;
  me->prev->next = me->next->prev = me;
}

class CWDChanger 
/**
  * Class to change the current working directory
 **/
{  
public:
  CWDChanger(const char* new_cwd);  
  ~CWDChanger(void);  

private:
  char *old_working_directory;
  bool restore;
};

class DllDirChanger 
{  
public:
  DllDirChanger(const char* new_cwd);  
  ~DllDirChanger(void);  

private:
  char *old_directory;
  bool restore;
};


class NonCachedGenericVideoFilter : public GenericVideoFilter 
/**
  * Class to select a range of frames from a longer clip
 **/
{
public:
  NonCachedGenericVideoFilter(PClip _child);
  int __stdcall SetCacheHints(int cachehints, int frame_range);
};



/*** Inline helper methods ***/


static __inline BYTE ScaledPixelClip(int i) {
  // return PixelClip((i+32768) >> 16);
  // PF: clamp is faster than lut
  return (uint8_t)clamp((i + 32768) >> 16, 0, 255);
}

static __inline uint16_t ScaledPixelClip(__int64 i) {
    return (uint16_t)clamp((i + 32768) >> 16, 0LL, 65535LL);
}

static __inline uint16_t ScaledPixelClipEx(__int64 i, int max_value) {
  return (uint16_t)clamp((int)((i + 32768) >> 16), 0, max_value);
}

static __inline bool IsClose(int a, int b, unsigned threshold) 
  { return (unsigned(a-b+threshold) <= threshold*2); }

static __inline bool IsCloseFloat(float a, float b, float threshold) 
{ return (a-b+threshold <= threshold*2); }

// useful SIMD helpers

// sse2 replacement of _mm_mullo_epi32 in SSE4.1
// use it after speed test, may have too much overhead and C is faster
__forceinline __m128i _MM_MULLO_EPI32(const __m128i &a, const __m128i &b)
{
  // for SSE 4.1: return _mm_mullo_epi32(a, b);
  __m128i tmp1 = _mm_mul_epu32(a,b); // mul 2,0
  __m128i tmp2 = _mm_mul_epu32( _mm_srli_si128(a,4), _mm_srli_si128(b,4)); // mul 3,1
  // shuffle results to [63..0] and pack. a2->a1, a0->a0
  return _mm_unpacklo_epi32(_mm_shuffle_epi32(tmp1, _MM_SHUFFLE (0,0,2,0)), _mm_shuffle_epi32(tmp2, _MM_SHUFFLE (0,0,2,0)));
}

// fake _mm_packus_epi32 (orig is SSE4.1 only)
__forceinline __m128i _MM_PACKUS_EPI32( __m128i a, __m128i b )
{
  a = _mm_slli_epi32 (a, 16);
  a = _mm_srai_epi32 (a, 16);
  b = _mm_slli_epi32 (b, 16);
  b = _mm_srai_epi32 (b, 16);
  a = _mm_packs_epi32 (a, b);
  return a;
}

// unsigned short div 255
#define SSE2_DIV255_U16(x) _mm_srli_epi16(_mm_mulhi_epu16(x, _mm_set1_epi16((short)0x8081)), 7)
#define AVX2_DIV255_U16(x) _mm256_srli_epi16(_mm256_mulhi_epu16(x, _mm256_set1_epi16((short)0x8081)), 7)

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
                ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))
#endif

#endif  // __Internal_H__
