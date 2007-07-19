#ifndef _INTEL_BATCHBUFFER_H
#define _INTEL_BATCHBUFFER_H

/* #define VERBOSE 0 */
#ifndef VERBOSE
extern int VERBOSE;
#endif

#define BATCH_LOCALS    char *batch_ptr;

#define BEGIN_BATCH(n)                                                  \
    do {                                                                \
        if (VERBOSE) fprintf(stderr,                                    \
                             "BEGIN_BATCH(%ld) in %s, %d dwords free\n", \
                             ((unsigned long)n), __FUNCTION__,          \
                             pI915XvMC->batch.space/4);                     \
        if (pI915XvMC->batch.space < (n)*4)                                 \
            intelFlushBatch(pI915XvMC, TRUE);                            \
        batch_ptr = pI915XvMC->batch.ptr;                                   \
    } while (0)

#define OUT_BATCH(n)                                                    \
    do {                                                                \
        *(GLuint *)batch_ptr = (n);                                     \
        if (VERBOSE) fprintf(stderr, " -- %08x at %s/%d\n", (n), __FILE__, __LINE__); \
        batch_ptr += 4;                                                 \
    } while (0)

#define ADVANCE_BATCH()                                        \
    do {                                                       \
        if (VERBOSE) fprintf(stderr, "ADVANCE_BATCH()\n");     \
        pI915XvMC->batch.space -= (batch_ptr - pI915XvMC->batch.ptr);  \
        pI915XvMC->batch.ptr = batch_ptr;                          \
        assert(pI915XvMC->batch.space >= 0);                       \
    } while(0)

extern void intelFlushBatch(i915XvMCContext *, Bool);
extern void intelBatchbufferData(i915XvMCContext *, const void *, unsigned, unsigned);
extern void intelInitBatchBuffer(i915XvMCContext *);
extern void intelDestroyBatchBuffer(i915XvMCContext *);
extern void intelCmdIoctl(i915XvMCContext *, char *, unsigned);
#endif /* _INTEL_BATCHBUFFER_H */
