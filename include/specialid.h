#ifndef __SPECIALID
#define __SPECIALID

//typedef struct SpecialId_* SpecialId;
struct SpecialId_;
typedef Type (*idck)(const Env, Exp_Primary*);
typedef Instr (*idem)(const Emitter, Exp_Primary*);

struct SpecialId_ {
  Type type;
  idck ck;
  f_instr exec;
  idem em;
  m_bool is_const;
};

#define ID_CHECK(a)  ANN Type a(const Env env NUSED, Exp_Primary* prim NUSED)
#define ID_EMIT(a)  ANN Instr a(const Emitter emit NUSED, Exp_Primary* prim NUSED)

ANN static inline Type specialid_type(const Env env,
    struct SpecialId_ *spid, const Exp_Primary* prim) {
  if(spid->is_const)
    exp_self(prim)->meta = ae_meta_value;
  return spid->type ?: spid->ck(env, prim);
}

ANN static inline Instr specialid_instr(const Emitter emit,
    struct SpecialId_ *spid, const Exp_Primary* prim) {
  return spid->exec ? emit_add_instr(emit, spid->exec) : spid->em(emit, prim);
}

ANN struct SpecialId_* specialid_get(const Gwion, const Symbol);
#endif
