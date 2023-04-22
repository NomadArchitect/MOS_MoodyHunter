// SPDX-License-Identifier: GPL-3.0-or-later

// #pragma once is not used here because this file is included multiple times in the same compilation unit

// PCI Class Codes
// _X(CLASS, SUBCLASS, PROGIF, NAME)
// do note that the values are in hex, not decimal, although the '0x' prefix is not present

#ifndef _X
#error "pci_classes.h must be included with _X defined"
#endif

_X(00, 00, 00, "Unclassified device")
_X(00, 01, 00, "VGA compatible device")
_X(01, 00, 00, "SCSI bus controller")
_X(01, 01, 00, "IDE Controller, ISA Compatibility mode")
_X(01, 01, 05, "IDE Controller, PCI native mode")
_X(01, 01, 0a, "IDE Controller, ISA Compatibility mode, supports both channels switched to PCI native mode")
_X(01, 01, 0f, "IDE Controller, PCI native mode controller, supports both channels switched to ISA compatibility mode")
_X(01, 01, 80, "IDE Controller, ISA Compatibility mode-only controller, supports bus mastering")
_X(01, 01, 85, "IDE Controller, PCI native mode-only controller, supports bus mastering")
_X(01, 01, 8a, "IDE Controller, ISA Compatibility mode controller, supports both channels switched to PCI native mode, supports bus mastering")
_X(01, 01, 8f, "IDE Controller, PCI native mode controller, supports both channels switched to ISA compatibility mode, supports bus mastering ")
_X(01, 02, 00, "Floppy disk controller")
_X(01, 03, 00, "IPI bus controller")
_X(01, 04, 00, "RAID bus controller")
_X(01, 05, 20, "ATA controller")
_X(01, 06, 00, "SATA controller")
_X(01, 07, 00, "Serial Attached SCSI controller")
_X(01, 08, 00, "NVMHCI controller")
_X(01, 08, 01, "NVM Express controller")
_X(01, 80, 00, "Other mass storage controller")
_X(02, 00, 00, "Ethernet controller")
_X(02, 07, 00, "InfiniBand controller")
_X(02, 80, 00, "Other network controller")
_X(03, 00, 00, "VGA compatible controller")
_X(03, 00, 01, "8514 controller")
_X(03, 01, 00, "XGA controller")
_X(03, 02, 00, "3D controller")
_X(03, 80, 00, "Other display controller")
_X(04, 00, 00, "Multimedia video controller")
_X(04, 01, 00, "Multimedia audio controller")
_X(04, 02, 00, "Computer telephony device")
_X(04, 03, 00, "Audio device")
_X(04, 80, 00, "Other multimedia controller")
_X(05, 00, 00, "RAM memory")
_X(05, 01, 00, "FLASH memory")
_X(05, 80, 00, "Other memory controller")
_X(06, 00, 00, "Host bridge")
_X(06, 01, 00, "ISA bridge")
_X(06, 02, 00, "EISA bridge")
_X(06, 03, 00, "MCA bridge")
_X(06, 04, 00, "PCI-to-PCI bridge")
_X(06, 04, 01, "PCI-to-PCI bridge (subtractive decoding)")
_X(06, 05, 00, "PCMCIA bridge")
_X(06, 06, 00, "NuBus bridge")
_X(06, 07, 00, "CardBus bridge")
_X(06, 08, 00, "RACEway bridge")
_X(06, 09, 40, "Semi-transparent PCI-to-PCI bridge, primary side")
_X(06, 09, 80, "Semi-transparent PCI-to-PCI bridge, secondary side")
_X(06, 0a, 00, "InfiniBand to PCI host bridge")
_X(06, 80, 00, "Other bridge device")
_X(07, 00, 00, "Serial controller")
_X(07, 01, 00, "Parallel controller")
_X(07, 05, 00, "Smart Card Controller")
_X(07, 80, 00, "Other communication controller")
_X(08, 00, 00, "Generic 8259 PIC")
_X(08, 00, 01, "ISA PIC")
_X(08, 00, 02, "EISA PIC")
_X(08, 00, 10, "I/O APIC")
_X(08, 00, 20, "I/O(x) APIC")
_X(08, 01, 00, "Generic 8237 DMA controller")
_X(08, 01, 01, "ISA DMA controller")
_X(08, 01, 02, "EISA DMA controller")
_X(08, 02, 00, "Generic 8254 system timer")
_X(08, 02, 01, "ISA system timer")
_X(08, 02, 02, "EISA system timer")
_X(08, 02, 03, "HPET")
_X(08, 03, 00, "Generic RTC controller")
_X(08, 03, 01, "ISA RTC controller")
_X(08, 04, 00, "Generic PCI Hot-plug controller")
_X(08, 05, 00, "SD Host controller")
_X(08, 06, 00, "IOMMU")
_X(08, 80, 00, "Other system peripheral")
_X(09, 00, 00, "Keyboard controller")
_X(09, 01, 00, "Digitizer Pen")
_X(09, 02, 00, "Mouse controller")
_X(09, 03, 00, "Scanner controller")
_X(09, 04, 00, "Gameport controller")
_X(09, 80, 00, "Other input controller")
_X(0a, 00, 00, "Generic Docking Station")
_X(0a, 80, 00, "Other type of docking station")
_X(0b, 00, 00, "CPU - 386")
_X(0b, 01, 00, "CPU - 486")
_X(0b, 02, 00, "CPU - Pentium")
_X(0b, 10, 00, "Alpha")
_X(0b, 20, 00, "PowerPC")
_X(0b, 30, 00, "MIPS")
_X(0b, 40, 00, "Co-processor")
_X(0b, 80, 00, "Other processor")
_X(0c, 00, 00, "FireWire (IEEE 1394)")
_X(0c, 03, 00, "USB controller (UHCI)")
_X(0c, 03, 10, "USB controller (OHCI)")
_X(0c, 03, 20, "USB controller (EHCI)")
_X(0c, 03, 30, "USB controller (XHCI)")
_X(0c, 03, 80, "USB controller (Unspecified)")
_X(0c, 03, fe, "USB device")
_X(0c, 04, 00, "Fibre controller")
_X(0c, 05, 00, "SMBus")
_X(0c, 06, 00, "InfiniBand")
_X(0c, 07, 00, "IPMI SMIC interface")
_X(0c, 07, 01, "IPMI Kybd controller style interface")
_X(0c, 07, 02, "IPMI Block transfer interface")
_X(0c, 08, 00, "SERCOS interface standard (IEC 61491)")
_X(0c, 09, 40, "CANbus")
_X(0c, 80, 00, "Other system peripheral")
_X(0d, 00, 00, "iRDA compatible controller")
_X(0d, 01, 00, "Consumer IR controller")
_X(0d, 10, 00, "RF controller")
_X(0d, 11, 00, "Bluetooth controller")
_X(0d, 12, 00, "Broadband controller")
_X(0d, 20, 00, "Ethernet (802.11a)")
_X(0d, 21, 00, "Ethernet (802.11b)")
_X(0d, 80, 00, "Other network controller")