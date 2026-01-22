# Timex FDD Computer

## Hardware Overview

The Timex FDD Computer is a Z80-based floppy disk controller system designed as an external peripheral for the Sinclair ZX Spectrum family.

## System Specifications

### Processor
- Z80 CPU at 4MHz

### Memory
- 2KB ROM (101 bytes utilized for bootloader)
- 64KB Static RAM (HM628128, 32KB utilized)

### Controllers
- WD1770 Floppy Disk Controller
- WD2123 Dual RS232 Serial Controller

### Interface
- 12-bit parallel interface to host computer
- Glue logic for address decoding and bus arbitration

### Power Requirements
- Single +5V supply for all components
- +-12v if real rs232 is implmented (not planned for now)

## Architecture Notes

The system uses minimal ROM space for bootstrap operations, reading 256 bytes from the first sector on the first track to RAM and then loadign Timex Operating System from first 4 tracks on the floppy.

The WD1770 controller handles physical disk operations including sector read/write, track seek, and drive control signals.

The static RAM provides buffer space for sector data and system variables during disk operations.

## Interface Specifications

Communication with the host occurs through a 12-bit parallel bus, allowing bidirectional data transfer and control signaling. The glue logic manages chip select decoding and bus arbitration between the Z80 and the host interface.

## Emulation Targets

For the WD1770-SD project, the following components require emulation:
- WD1770 FDD controller (primary target)
- 12-bit parallel interface (CPLD implementation)

The WD2123 serial controller is not currently targeted for emulation.
