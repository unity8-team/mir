#ifndef __NV_PCICOMPAT_H__
#define __NV_PCICOMPAT_H__

#include <pciaccess.h>

#define PCI_DEV_VENDOR_ID(_device) ((_device)->vendor_id)
#define PCI_DEV_DEVICE_ID(_device) ((_device)->device_id)
#define PCI_DEV_REVISION(_device) ((_device)->revision)

#define PCI_DEV_FUNC(_device) ((_device)->func)
#define PCI_DEV_BUS(_device) ((_device)->bus)
#define PCI_DEV_DEV(_device) ((_device)->dev)

#define PCI_DEV_TAG(_device) (*(_device))

#define PCI_DEV_MEM_BASE(_device, _area) ((_device)->regions[(_area)].base_addr)
#define PCI_DEV_IO_BASE(_device, _area) (PCI_DEV_MEM_BASE(_device, _area))

#define PCI_DEV_READ_LONG(_device, _offset, _dest_ptr) (pci_device_cfg_read_u32(_device, _dest_ptr, _offset))
#define PCI_DEV_WRITE_LONG(_device, _offset, _src) (pci_device_cfg_write_u32(_device, _src, _offset))

#define PCI_SLOT_READ_LONG(_slot, _offset) __extension__ ({ uint32_t _pci_slot_read_ret; pci_device_cfg_read_u32(pci_device_find_by_slot(0, 0, 0, _slot), &_pci_slot_read_ret, _offset); _pci_slot_read_ret; })

#define PCI_DEV_PCI_ID(_device) ((PCI_DEV_VENDOR_ID(_device) << 16) | PCI_DEV_DEVICE_ID(_device))

#endif /* __NV_PCICOMPAT_H__ */
