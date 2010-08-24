/* The old i810 (only) driver. */
extern const OptionInfoRec *lg_i810_available_options(int chipid, int busid);
extern void lg_i810_init(ScrnInfoPtr scrn);

/* The old User-Mode Setting + EXA driver */
extern const OptionInfoRec *lg_ums_available_options(int chipid, int busid);
extern void lg_ums_init(ScrnInfoPtr scrn);
