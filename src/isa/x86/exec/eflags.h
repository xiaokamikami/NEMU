static inline def_EHelper(cld) {
  rtl_set_DF(s, rz);
  print_asm("cld");
}

static inline def_EHelper(cli) {
  rtl_set_IF(s, rz);
  print_asm("cli");
}

static inline def_EHelper(pushf) {
  void rtl_compute_eflags(DecodeExecState *s, rtlreg_t *dest);
  rtl_compute_eflags(s, s0);
  rtl_push(s, s0);
  print_asm("popf");
}

static inline def_EHelper(popf) {
  void rtl_set_eflags(DecodeExecState *s, const rtlreg_t *src);
  rtl_pop(s, s0);
  rtl_set_eflags(s, s0);
  print_asm("popf");
}
