#include <isa.h>
#include <memory/paddr.h>
#include <memory/vaddr.h>
#include <device/map.h>
#include <stdlib.h>
#include <time.h>

static uint8_t pmem[PMEM_SIZE] PG_ALIGN = {};
nemu_bool pmem_dirty[PMEM_SIZE] PG_ALIGN = {0};

void* guest_to_host(paddr_t addr) { return &pmem[addr]; }
paddr_t host_to_guest(void *addr) { return (uint8_t *)pmem - (uint8_t *)addr; }

IOMap* fetch_mmio_map(paddr_t addr);

void init_mem() {
#if !defined(DIFF_TEST) && !defined(__SIMPOINT)
  srand(time(0));
  uint32_t *p = (uint32_t *)pmem;
  int i;
  for (i = 0; i < PMEM_SIZE / sizeof(p[0]); i ++) {
    p[i] = rand();
  }
#endif
}

uint8_t *getPmem() {
  return pmem;
}

static inline nemu_bool in_pmem(paddr_t addr) {
  return (PMEM_BASE <= addr) && (addr <= PMEM_BASE + PMEM_SIZE - 1);
}

static inline word_t pmem_read(paddr_t addr, int len) {
  void *p = &pmem[addr - PMEM_BASE];
  switch (len) {
    case 1: return *(uint8_t  *)p;
    case 2: return *(uint16_t *)p;
    case 4: return *(uint32_t *)p;
#ifdef ISA64
    case 8: return *(uint64_t *)p;
#endif
    default: assert(0);
  }
}

static inline void pmem_write(paddr_t addr, word_t data, int len) {
  // write to pmem, mark pmem addr as dirty
  void *p = &pmem[addr - PMEM_BASE];
  switch (len) {
    case 1: 
      *(uint8_t  *)p = data;
      pmem_dirty[addr - PMEM_BASE] = true;
      return;
    case 2: 
      *(uint16_t *)p = data;
      for(int i = 0; i < 2; i++)
        pmem_dirty[addr - PMEM_BASE + i] = true;
      return;
    case 4: 
      *(uint32_t *)p = data;
      for(int i = 0; i < 4; i++)
        pmem_dirty[addr - PMEM_BASE + i] = true;
      return;
#ifdef ISA64
    case 8: 
      *(uint64_t *)p = data;
      for(int i = 0; i < 8; i++)
        pmem_dirty[addr - PMEM_BASE + i] = true;
      return;
#endif
    default: assert(0);
  }
}

void rtl_sfence() {
#ifdef ENABLE_DISAMBIGUATE
  memset(pmem_dirty, 0, sizeof(pmem_dirty));
#endif
}

/* Memory accessing interfaces */

word_t paddr_read(paddr_t addr, int len) {
  if (in_pmem(addr)) return pmem_read(addr, len);
  else return map_read(addr, len, fetch_mmio_map(addr));
}

void paddr_write(paddr_t addr, word_t data, int len) {
  if (in_pmem(addr)) pmem_write(addr, data, len);
  else map_write(addr, data, len, fetch_mmio_map(addr));
}

nemu_bool is_sfence_safe(paddr_t addr, int len) {
  if (in_pmem(addr)){
    nemu_bool dirty = false;
    switch (len) {
      case 1: return !pmem_dirty[addr - PMEM_BASE];
      case 2: 
        for(int i =0; i < 2; i++){
          dirty |= pmem_dirty[addr - PMEM_BASE + i];
        }
        return !dirty;
      case 4:
        for(int i =0; i < 4; i++){
          dirty |= pmem_dirty[addr - PMEM_BASE + i];
        }
        return !dirty;
  #ifdef ISA64
      case 8:
        for(int i =0; i < 8; i++){
          dirty |= pmem_dirty[addr - PMEM_BASE + i];
        }
        return !dirty;
  #endif
      default: assert(0);
    }
  } else return true;
}

word_t vaddr_mmu_read(vaddr_t addr, int len, int type);
void vaddr_mmu_write(vaddr_t addr, word_t data, int len);

#define def_vaddr_template(bytes) \
word_t concat(vaddr_ifetch, bytes) (vaddr_t addr) { \
  int ret = isa_vaddr_check(addr, MEM_TYPE_IFETCH, bytes); \
  if (ret == MEM_RET_OK) return paddr_read(addr, bytes); \
  else if (ret == MEM_RET_NEED_TRANSLATE) return vaddr_mmu_read(addr, bytes, MEM_TYPE_IFETCH); \
  return 0; \
} \
word_t concat(vaddr_read, bytes) (vaddr_t addr) { \
  int ret = isa_vaddr_check(addr, MEM_TYPE_READ, bytes); \
  if (ret == MEM_RET_OK) return paddr_read(addr, bytes); \
  else if (ret == MEM_RET_NEED_TRANSLATE) return vaddr_mmu_read(addr, bytes, MEM_TYPE_READ); \
  return 0; \
} \
void concat(vaddr_write, bytes) (vaddr_t addr, word_t data) { \
  int ret = isa_vaddr_check(addr, MEM_TYPE_WRITE, bytes); \
  if (ret == MEM_RET_OK) paddr_write(addr, data, bytes); \
  else if (ret == MEM_RET_NEED_TRANSLATE) vaddr_mmu_write(addr, data, bytes); \
}

def_vaddr_template(1)
def_vaddr_template(2)
def_vaddr_template(4)
#ifdef ISA64
def_vaddr_template(8)
#endif