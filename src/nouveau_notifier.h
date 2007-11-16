#ifndef __NOUVEAU_NOTIFIER_H__
#define __NOUVEAU_NOTIFIER_H__

#define NV_NOTIFIER_SIZE                                                      32
#define NV_NOTIFY_TIME_0                                              0x00000000
#define NV_NOTIFY_TIME_1                                              0x00000004
#define NV_NOTIFY_RETURN_VALUE                                        0x00000008
#define NV_NOTIFY_STATE                                               0x0000000C
#define NV_NOTIFY_STATE_STATUS_MASK                                   0xFF000000
#define NV_NOTIFY_STATE_STATUS_SHIFT                                          24
#define NV_NOTIFY_STATE_STATUS_COMPLETED                                    0x00
#define NV_NOTIFY_STATE_STATUS_IN_PROCESS                                   0x01
#define NV_NOTIFY_STATE_ERROR_CODE_MASK                               0x0000FFFF
#define NV_NOTIFY_STATE_ERROR_CODE_SHIFT                                       0

struct nouveau_notifier {
	struct nouveau_channel *channel;
	uint32_t handle;
};

#endif
