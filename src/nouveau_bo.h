#ifndef __NOUVEAU_BO_H__
#define __NOUVEAU_BO_H__

/* Relocation/Buffer type flags */
#define NOUVEAU_BO_VRAM (1 << 0)
#define NOUVEAU_BO_GART (1 << 1)
#define NOUVEAU_BO_RD   (1 << 2)
#define NOUVEAU_BO_WR   (1 << 3)
#define NOUVEAU_BO_RDWR (NOUVEAU_BO_RD | NOUVEAU_BO_WR)
#define NOUVEAU_BO_MAP  (1 << 4)
#define NOUVEAU_BO_PIN  (1 << 5)
#define NOUVEAU_BO_LOW  (1 << 6)
#define NOUVEAU_BO_HIGH (1 << 7)
#define NOUVEAU_BO_OR   (1 << 8)

struct nouveau_bo {
	struct nouveau_device *device;

	uint64_t size;
	void *map;

	/*XXX: temporary! */
	uint64_t offset;
};

#endif
