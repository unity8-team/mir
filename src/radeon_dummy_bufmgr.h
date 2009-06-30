
#ifndef RADEON_DUMMY_BUFMGR_H
#define RADEON_DUMMY_BUFMGR_H
/* when we don't have modesetting but we still need these functions */

struct radeon_bo {
  int dummy;
  void *ptr;
};

static inline int radeon_cs_begin(Bool dummy, int d2, const char *file,
				   const char *func, int line)
{
  return 0;
}

static inline int radeon_cs_end(Bool dummy, const char *file,
				const char *func, int line)
{
  return 0;
}

static inline void radeon_cs_write_dword(Bool cs, uint32_t dword)
{
}

static inline int radeon_cs_write_reloc(Bool cs,
                                        struct radeon_bo *bo,
                                        uint32_t read_domain,
                                        uint32_t write_domain,
                                        uint32_t flags)
{
  return 0;
}

static inline int radeon_bo_map(struct radeon_bo *bo, int write) {return 0;}
static inline void radeon_bo_ref(struct radeon_bo *bo) {return;}
static inline struct radeon_bo *radeon_bo_unref(struct radeon_bo *bo) {return NULL;}
static inline void radeon_bo_unmap(struct radeon_bo *bo) {return;}
static inline int radeon_bo_wait(struct radeon_bo *bo) {return 0;}


struct radeon_cs_space_check {
  struct radeon_bo *bo;
  int read_domains;
  int write_domain;
  int new_accounted;
};

static inline int radeon_cs_space_check(Bool cs, struct radeon_cs_space_check *bos, int num)
{
  return 0;
}
#define RADEON_CS_SPACE_OP_TO_BIG 0
#define RADEON_CS_SPACE_FLUSH 1

#endif
