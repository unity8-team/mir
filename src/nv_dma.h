#ifndef NV_DMA_H
#define NV_DMA_H

#define NVDEBUG if (NV_DMA_DEBUG) ErrorF

enum DMAObjects {
	NvNullObject		= 0x00000000,
	NvContextSurfaces	= 0x80000010, 
	NvRop			= 0x80000011, 
	NvImagePattern		= 0x80000012, 
	NvClipRectangle		= 0x80000013, 
	NvSolidLine		= 0x80000014, 
	NvImageBlit		= 0x80000015, 
	NvRectangle		= 0x80000016, 
	NvScaledImage		= 0x80000017, 
	NvMemFormat		= 0x80000018,
	Nv3D			= 0x80000019,
	NvImageFromCpu		= 0x8000001A,
	NvContextBeta1		= 0x8000001B,
	NvContextBeta4		= 0x8000001C,
	Nv2D			= 0x80000020,
	NvDmaFB			= 0xD8000001,
	NvDmaTT			= 0xD8000002,
	NvDmaNotifier0		= 0xD8000003,

	/*XVideo notifiers need to have consecutive handles, be careful when remapping*/
	NvDmaXvNotifier0	= 0xE8000000,
	NvDmaXvNotifier1	= 0xE8000001,
	NvDmaXvNotifier2	= 0xE8000002,
	NvDmaXvNotifier3	= 0xE8000003,
	NvDmaXvNotifier4	= 0xE8000004,
	NvDmaXvNotifier5	= 0xE8000005,
};

#endif /* NV_DMA_H */
