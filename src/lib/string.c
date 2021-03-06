#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "instr.h"
#include "object.h"
#include "gwion.h"
#include "operator.h"
#include "import.h"
#include "emit.h"
#include "specialid.h"
#include "gwi.h"
#include "gack.h"

ANN static void push_string(const VM_Shred shred, const M_Object obj, const m_str c) {
  STRING(obj) = s_name(insert_symbol(shred->info->vm->gwion->st, c));
  *(M_Object*)REG(-SZ_INT) = (M_Object)obj;
  _release(obj, shred);
}

#define describe_string_logical(name, action)    \
static INSTR(String_##name) {                    \
  POP_REG(shred, SZ_INT);                        \
  const M_Object lhs = *(M_Object*)REG(-SZ_INT); \
  const M_Object rhs = *(M_Object*)REG(0);       \
  *(m_int*)REG(-SZ_INT) = action;                \
  release(lhs, shred);                           \
  release(rhs, shred);                           \
}
describe_string_logical(eq, (lhs && rhs && STRING(lhs) == STRING(rhs)) || (!lhs && !rhs))
describe_string_logical(neq, !(lhs && rhs && STRING(lhs) == STRING(rhs)) || (!lhs && !rhs))

static INSTR(String_Assign) {
  POP_REG(shred, SZ_INT);
  const M_Object lhs = *(M_Object*)REG(-SZ_INT);
  const M_Object rhs = *(M_Object*)REG(0);
  release(lhs, shred);
  push_string(shred, rhs, lhs ? STRING(lhs) : "");
}

static CTOR(string_ctor) {
  STRING(o) = "";
}

ID_CHECK(check_funcpp) {
  ((Exp_Primary*)prim)->prim_type = ae_prim_str;
  ((Exp_Primary*)prim)->d.str = env->func ? env->func->name : env->class_def ?
    env->class_def->name : env->name;
  ((Exp_Primary*)prim)->value = global_string(env, prim->d.str);
  return prim->value->type;
}

static GACK(gack_string) {
  const M_Object obj = *(M_Object*)VALUE;
  INTERP_PRINTF("%s", obj ? STRING(obj) : "(null string)");
}

static inline m_bool bounds(const m_str str, const m_int i) {
  CHECK_BB(i)
  return (m_uint)i < strlen(str) ? GW_OK : GW_ERROR;
}

static INSTR(StringSlice) {
  shred->reg -= SZ_INT *2;
  const M_Object obj = *(M_Object*)REG(-SZ_INT);
  m_str str = STRING(obj);
  const m_int start = *(m_uint*)REG(0);
  const size_t strsz = strlen(str);
  m_int end   = *(m_uint*)REG(SZ_INT);
  if(end < 0)
    end = strsz + end;
  if(bounds(str, start) < 0 || bounds(str, end) < 0)
    Except(shred, "OutOfBoundsStringSlice");
  const m_int op    = start < end ? 1 : -1;
  const m_uint sz    = op > 0 ? end - start : start - end;
  char c[sz + 1];
  for(m_int i = start, j = 0; i != end; i += op, ++j)
    c[j] = str[i];
  c[sz] = '\0';
  *(M_Object*)REG(-SZ_INT) = new_string(shred->info->vm->gwion->mp, shred, c);
}

static MFUN(string_len) {
  *(m_uint*)RETURN = strlen(STRING(o));
}

static MFUN(string_upper) {
  char c[strlen(STRING(o)) + 1];
  strcpy(c, STRING(o));
  for(m_uint i = 0; i < strlen(c); i++)
    if(c[i]  >= 'a' && c[i] <= 'z')
      c[i] += 'A' - 'a';
  *(M_Object*)RETURN = new_string(shred->info->vm->gwion->mp, shred, c);
}

static MFUN(string_lower) {
  char c[strlen(STRING(o)) + 1];
  strcpy(c, STRING(o));
  for(m_uint i = 0; i < strlen(c); i++)
    if(c[i]  >= 'A' && c[i] <= 'Z')
      c[i] -= 'A' - 'a';
  *(M_Object*)RETURN = new_string(shred->info->vm->gwion->mp, shred, c);
}

static MFUN(string_ltrim) {
  m_uint i = 0;
  const m_str str = STRING(o);
  while(str[i] == ' ')
    i++;
  char c[strlen(str) - i + 1];
  strcpy(c, STRING(o) + i);
  *(M_Object*)RETURN = new_string(shred->info->vm->gwion->mp, shred, c);
}

static MFUN(string_rtrim) {
  const m_str str = STRING(o);
  m_uint len = strlen(str) - 1;
  while(str[len] == ' ')
    len--;
  char c[len + 2];
  strncpy(c, str, len + 1);
  c[len + 1] = '\0';
  *(M_Object*)RETURN = new_string(shred->info->vm->gwion->mp, shred, c);
}

static MFUN(string_trim) {
  const m_str str = STRING(o);
  m_int i, start = 0, end = 0, len = 0;
  while(str[len] != '\0')
    len++;
  for(i = 0; i < len; i++) {
    if(str[i] == ' ')
      start++;
    else break;
  }
  for(i = len - 1; i >= 0; i--) {
    if(str[i] == ' ')
      end++;
    else break;
  }
  if(len - start - end <= 0) {
    *(m_uint*)RETURN = 0;
    return;
  }
  char c[len - start - end + 1];
  for(i = start; i < len - end; i++)
    c[i - start] = str[i];
  c[len - start - end ] = '\0';
  *(M_Object*)RETURN = new_string(shred->info->vm->gwion->mp, shred, c);
}

static MFUN(string_charAt) {
  const m_str str = STRING(o);
  const m_int i = *(m_int*)MEM(SZ_INT);
  const m_uint len = strlen(str);
  if(i < 0 || (m_uint)i >= len)
    *(m_uint*)RETURN = -1;
  else
    *(m_uint*)RETURN = str[i];
}

static MFUN(string_setCharAt) {
  const m_str str = STRING(o);
  const m_int i = *(m_int*)MEM(SZ_INT);
  const m_int c = *(m_int*)MEM(SZ_INT * 2);
  const m_uint len = strlen(str);
  if(i < 0 || (m_uint)i >= len)
    *(m_uint*)RETURN = -1;
  else {
    str[i] = c;
    STRING(o) = s_name(insert_symbol(shred->info->vm->gwion->st, str));
    *(m_uint*)RETURN = c;
  }
}

static MFUN(string_insert) {
  char str[strlen(STRING(o)) + 1];
  strcpy(str, STRING(o));
  m_int i, len_insert = 0, index = *(m_int*)MEM(SZ_INT);
  const M_Object arg = *(M_Object*)MEM(SZ_INT * 2);

  if(!arg) {
    *(M_Object*)RETURN = NULL;
    return;
  }
  char insert[strlen(STRING(arg)) + 1];
  strcpy(insert, STRING(arg));
  const m_uint len = strlen(str);
  len_insert =  strlen(insert);
  char c[len + len_insert + 1];
  for(i = 0; i < index; i++)
    c[i] = str[i];
  for(i = 0; i < len_insert; i++)
    c[i + index] = insert[i];
  for(i = index; i < (m_int)len; i++)
    c[i + len_insert] = str[i];
  c[len + len_insert] = '\0';
  release(arg, shred);
  *(M_Object*)RETURN = new_string(shred->info->vm->gwion->mp, shred, c);;
}

static MFUN(string_replace) {
  char str[strlen(STRING(o)) + 1];
  strcpy(str, STRING(o));
  m_int i, len_insert = 0, index = *(m_int*)MEM(SZ_INT);
  const M_Object arg = *(M_Object*)MEM(SZ_INT * 2);
  if(!arg) {
    *(M_Object*)RETURN = o;
    return;
  }
  char insert[strlen(STRING(arg)) + 1];
  strcpy(insert, STRING(arg));
  const m_uint len = strlen(str);
  len_insert =  strlen(insert);
  if(index >= (m_int)len  || index < 0 || (index + len_insert + 1) <= 0) {
    release(arg, shred);
    *(M_Object*)RETURN = NULL;
    return;
  }
  char c[index + len_insert + 1];
  for(i = 0; i < index; i++)
    c[i] = str[i];
  for(i = 0; i < len_insert; i++)
    c[i + index] = insert[i];
  c[index + len_insert] = '\0';
  release(arg, shred);
  *(M_Object*)RETURN = new_string(shred->info->vm->gwion->mp, shred, c);;
}

static MFUN(string_replaceN) {
  char str[strlen(STRING(o)) + 1];
  strcpy(str, STRING(o));
  m_int i, index = *(m_int*)MEM(SZ_INT);
  const M_Object arg = *(M_Object*)MEM(SZ_INT * 3);
  const m_int _len = *(m_int*)MEM(SZ_INT * 2);
  if(!arg || index > (m_int)strlen(STRING(o)) || _len > (m_int)strlen(STRING(arg))) {
    *(M_Object*)RETURN = NULL;
    return;
  }
  char insert[strlen(STRING(arg)) + 1];
  const m_uint len = strlen(str);
  memset(insert, 0, len + 1);
  strcpy(insert, STRING(arg));
  str[len] = '\0';
  if(index > (m_int)len)
    index = len - 1;
  char c[len + _len];
  memset(c, 0, len + _len);
  for(i = 0; i < index; i++)
    c[i] = str[i];
  for(i = 0; i < _len; i++)
    c[i + index] = insert[i];
  for(i = index + _len; i < (m_int)len; i++)
    c[i] = str[i];
  c[len + _len - 1] = '\0';
  release(arg, shred);
  *(M_Object*)RETURN = new_string(shred->info->vm->gwion->mp, shred, c);;
}

static MFUN(string_find) {
  const m_str str = STRING(o);
  m_int i = 0, ret = -1;
  char arg = *(m_int*)MEM(SZ_INT);
  while(str[i] != '\0') {
    if(str[i] == arg) {
      ret = i;
      break;
    }
    i++;
  }
  *(m_uint*)RETURN = ret;
}

static MFUN(string_findStart) {
  const m_str str = STRING(o);
  const char pos = *(m_int*)MEM(SZ_INT);
  const char arg = *(m_int*)MEM(SZ_INT * 2);
  m_int i = pos, ret = -1;
  if(!strlen(str)) {
    *(M_Object*)RETURN = NULL;
    return;
  }
  while(str[i] != '\0') {
    if(str[i] == arg) {
      ret = i;
      break;
    }
    i++;
  }
  *(m_uint*)RETURN = ret;
}

static MFUN(string_findStr) {
  if(!strlen(STRING(o))) {
    *(M_Object*)RETURN = NULL;
    return;
  }
  char str[strlen(STRING(o)) + 1];
  strcpy(str, STRING(o));
  m_int ret = -1;
  const M_Object obj = *(M_Object*)MEM(SZ_INT);
  if(!obj) {
    *(m_uint*)RETURN = 0;
    return;
  }
  const m_str arg = STRING(obj);
  const m_int len  = strlen(str);
  m_int i = 0;
  const m_int arg_len = strlen(arg);
  while(i < len) {
    if(!strncmp(str + i, arg, arg_len)) {
      ret = i;
      break;
    }
    i++;
  }
  release(obj, shred);
  *(m_uint*)RETURN = ret;
}

static MFUN(string_findStrStart) {
  if(!strlen(STRING(o))) {
    *(M_Object*)RETURN = NULL;
    return;
  }
  char str[strlen(STRING(o)) + 1];
  strcpy(str, STRING(o));
  m_int ret = -1;
  const m_int start = *(m_int*)MEM(SZ_INT);
  const M_Object obj = *(M_Object*)MEM(SZ_INT * 2);
  if(!obj) {
    *(M_Object*)RETURN = NULL;
    return;
  }
  const m_str arg = STRING(obj);
  const m_int len  = strlen(str);
  m_int i = start;
  const m_int arg_len = strlen(arg);
  while(i < len) {
    if(!strncmp(str + i, arg, arg_len)) {
      ret = i;
      break;
    }
    i++;
  }
  release(obj, shred);
  *(m_uint*)RETURN = ret;
}

static MFUN(string_rfind) {
  const m_str str = STRING(o);
  m_int i = strlen(str) - 1, ret = -1;
  const char arg = *(m_int*)MEM(SZ_INT);
  while(i > -1 && str[i] != '\0') {
    if(str[i] == arg) {
      ret = i;
      break;
    }
    i--;
  }
  *(m_uint*)RETURN = ret;
}

static MFUN(string_rfindStart) {
  if(!strlen(STRING(o))) {
    *(M_Object*)RETURN = NULL;
    return;
  }
  char str[strlen(STRING(o)) + 1];
  strcpy(str, STRING(o));
  const char pos = *(m_int*)MEM(SZ_INT);
  const char arg = *(m_int*)MEM(SZ_INT * 2);
  m_int i = pos, ret = -1;
  while(i > 0 && str[i] != '\0') {
    if(str[i] == arg) {
      ret = i;
      break;
    }
    i--;
  }
  *(m_uint*)RETURN = ret;
}

static MFUN(string_rfindStr) {
  if(!strlen(STRING(o))) {
    *(M_Object*)RETURN = NULL;
    return;
  }
  char str[strlen(STRING(o)) + 1];
  strcpy(str, STRING(o));
  m_int ret = -1;
  const M_Object obj = *(M_Object*)MEM(SZ_INT);
  const m_str arg = STRING(o);
  const m_int len  = strlen(str);
  m_int i = len - 1;
  const m_int arg_len = strlen(arg);
  while(i) {
    if(!strncmp(str + i, arg, arg_len)) {
      ret = i;
      break;
    }
    i--;
  }
  release(obj, shred);
  *(m_uint*)RETURN = ret;
}

static MFUN(string_rfindStrStart) {
  if(!strlen(STRING(o))) {
    *(M_Object*)RETURN = NULL;
    return;
  }
  char str[strlen(STRING(o)) + 1];
  strcpy(str, STRING(o));
  m_int ret = -1;
  m_int start = *(m_int*)MEM(SZ_INT);
  const M_Object obj = *(M_Object*)MEM(SZ_INT * 2);
  if(!obj) {
    *(m_uint*)RETURN = 0;
    return;
  }
  m_str arg = STRING(obj);

  m_int i = start;
  const m_int arg_len = strlen(arg);
  while(i > -1) {
    if(!strncmp(str + i, arg, arg_len)) {
      ret = i;
      break;
    }
    i--;
  }
  release(obj, shred);
  *(m_uint*)RETURN = ret;
}

static MFUN(string_erase) {
  const m_str str = STRING(o);
  const m_int start = *(m_int*)MEM(SZ_INT);
  const m_int rem = *(m_int*)MEM(SZ_INT * 2);
  const m_int len = strlen(str);
  const m_int size = len - rem + 1;
  if(start >= len || size <= 0) {
    *(M_Object*)RETURN = NULL;
    return;
  }
  char c[size];
  c[size - 1] = '\0';
  for(m_int i = 0; i < start; i++)
    c[i] = str[i];
  for(m_int i = start + rem; i < len; i++)
    c[i - rem] = str[i];
  STRING(o) = s_name(insert_symbol(shred->info->vm->gwion->st, c));
}

GWION_IMPORT(string) {
  const Type t_string = gwi_class_ini(gwi, "string", NULL);
  gwi_class_xtor(gwi, string_ctor, NULL);
  GWI_BB(gwi_gack(gwi, t_string, gack_string))
  gwi->gwion->type[et_string] = t_string; // use func

  gwi_item_ini(gwi, "@internal", "@data");
  GWI_BB(gwi_item_end(gwi,   ae_flag_const, NULL))

  gwi_func_ini(gwi, "int", "size");
  GWI_BB(gwi_func_end(gwi, string_len, ae_flag_none))

  gwi_func_ini(gwi, "string", "upper");
  GWI_BB(gwi_func_end(gwi, string_upper, ae_flag_none))

  gwi_func_ini(gwi, "string", "lower");
  GWI_BB(gwi_func_end(gwi, string_lower, ae_flag_none))

  gwi_func_ini(gwi, "string", "ltrim");
  GWI_BB(gwi_func_end(gwi, string_ltrim, ae_flag_none))

  gwi_func_ini(gwi, "string", "rtrim");
  GWI_BB(gwi_func_end(gwi, string_rtrim, ae_flag_none))

  gwi_func_ini(gwi, "string", "trim");
  GWI_BB(gwi_func_end(gwi, string_trim, ae_flag_none))

  gwi_func_ini(gwi, "int", "charAt");
  gwi_func_arg(gwi, "int", "pos");
  GWI_BB(gwi_func_end(gwi, string_charAt, ae_flag_none))

  gwi_func_ini(gwi, "int", "charAt");
  gwi_func_arg(gwi, "int", "pos");
  gwi_func_arg(gwi, "char", "c");
  GWI_BB(gwi_func_end(gwi, string_setCharAt, ae_flag_none))

  gwi_func_ini(gwi, "string", "insert");
  gwi_func_arg(gwi, "int", "pos");
  gwi_func_arg(gwi, "string", "str");
  GWI_BB(gwi_func_end(gwi, string_insert, ae_flag_none))

  gwi_func_ini(gwi, "string", "replace");
  gwi_func_arg(gwi, "int", "pos");
  gwi_func_arg(gwi, "string", "str");
  GWI_BB(gwi_func_end(gwi, string_replace, ae_flag_none))

  gwi_func_ini(gwi, "string", "replace");
  gwi_func_arg(gwi, "int", "pos");
  gwi_func_arg(gwi, "int", "n");
  gwi_func_arg(gwi, "string", "str");
  GWI_BB(gwi_func_end(gwi, string_replaceN, ae_flag_none))

  gwi_func_ini(gwi, "int", "find");
  gwi_func_arg(gwi, "char", "c");
  GWI_BB(gwi_func_end(gwi, string_find, ae_flag_none))

  gwi_func_ini(gwi, "int", "find");
  gwi_func_arg(gwi, "int", "pos");
  gwi_func_arg(gwi, "char", "c");
  GWI_BB(gwi_func_end(gwi, string_findStart, ae_flag_none))

  gwi_func_ini(gwi, "int", "find");
  gwi_func_arg(gwi, "string", "str");
  GWI_BB(gwi_func_end(gwi, string_findStr, ae_flag_none))

  gwi_func_ini(gwi, "int", "find");
  gwi_func_arg(gwi, "int", "pos");
  gwi_func_arg(gwi, "string", "str");
  GWI_BB(gwi_func_end(gwi, string_findStrStart, ae_flag_none))

  gwi_func_ini(gwi, "int", "rfind");
  gwi_func_arg(gwi, "char", "c");
  GWI_BB(gwi_func_end(gwi, string_rfind, ae_flag_none))

  gwi_func_ini(gwi, "int", "rfind");
  gwi_func_arg(gwi, "int", "pos");
  gwi_func_arg(gwi, "char", "c");
  GWI_BB(gwi_func_end(gwi, string_rfindStart, ae_flag_none))

  gwi_func_ini(gwi, "int", "rfind");
  gwi_func_arg(gwi, "string", "str");
  GWI_BB(gwi_func_end(gwi, string_rfindStr, ae_flag_none))

  gwi_func_ini(gwi, "int", "rfind");
  gwi_func_arg(gwi, "int", "pos");
  gwi_func_arg(gwi, "string", "str");
  GWI_BB(gwi_func_end(gwi, string_rfindStrStart, ae_flag_none))

  gwi_func_ini(gwi, "void", "erase");
  gwi_func_arg(gwi, "int", "start");
  gwi_func_arg(gwi, "int", "length");
  GWI_BB(gwi_func_end(gwi, string_erase, ae_flag_none))

  GWI_BB(gwi_class_end(gwi))

  GWI_BB(gwi_oper_ini(gwi, "string",  "nonnull string", "nonnull string"))
  GWI_BB(gwi_oper_add(gwi, opck_const_rhs))
  GWI_BB(gwi_oper_end(gwi, "=>",      String_Assign))

  GWI_BB(gwi_oper_ini(gwi, "string",  "string", "bool"))
  GWI_BB(gwi_oper_end(gwi, "==",       String_eq))
  GWI_BB(gwi_oper_end(gwi, "!=",       String_neq))

  GWI_BB(gwi_oper_ini(gwi, "int", "nonnull string", "nonnull string"))
  GWI_BB(gwi_oper_end(gwi, "@slice", StringSlice))

  struct SpecialId_ spid = { .ck=check_funcpp, .exec=RegPushMe, .is_const=1 };
  gwi_specialid(gwi, "__func__", &spid);
  return GW_OK;
}
