# Timex FDD Interface

## Overview

The Timex FDD Interface bridges the Sinclair ZX Spectrum to the Timex FDD Computer, providing transparent disk access through ROM-based BASIC instruction trapping and a parallel communication interface.

## Hardware Components

### Original Implementation

**Memory:**
- 4KB ROM (0x0000-0x1FFF) - BASIC instruction trap handlers
- 2KB RAM (0x2000-0x2FFF) - Interface working memory

**Logic:**
- GAL23 - ROM/RAM page control
- GAL16 - Address decode logic
- LS109 - J-K flip-flop for state management
- LS273 - 8-bit latch (bidirectional, two instances)
- LS244 - 8-bit buffer (bidirectional, two instances)

### CPLD Implementation

The XC9572XL CPLD consolidates the discrete logic into a single chip, eliminating the 12-wire cable and simplifying the design.

**Integrated Functions:**
- GAL23 and GAL16 logic (complete equation translation)
- LS109 flip-flop (page-in/out state control)
- Dual LS273 latches (ZX and FDD sides)
- Dual LS244 buffers (ZX and FDD sides)
- WD1770 DRQ routing (appears on FDD_D7 during reads)

**Package Requirements:**
- XC9572XL in VQ64 package
- 39 I/O pins utilized

## Memory Map

| Address Range | Function | Access |
|---------------|----------|--------|
| 0x0000-0x1FFF | Interface ROM | Read |
| 0x2000-0x2FFF | Interface RAM | Read/Write |

Memory appears with incomplete address decoding, creating echo regions in the address space.

## Paging Mechanism

**Page-In Triggers:**
- Access to 0x0000
- Access to 0x0008

**Page-Out Trigger:**
- Access to 0x0604

The paging logic maintains state through the LS109-equivalent flip-flop, allowing the ROM/RAM to transparently overlay the ZX Spectrum memory when disk operations occur.

## Interface Protocol

### Communication Channel

Six-bit parallel interface provides bidirectional communication between the ZX Spectrum and FDD computer.

### Data Flow

**ZX to FDD:**
Data latched through LS273 equivalent, presented to FDD side through LS244 buffer.

**FDD to ZX:**
Data latched through LS273 equivalent, presented to ZX side through LS244 buffer.

### Special Signals

**WD1770 DRQ:**
The Data Request signal from the WD1770 controller routes to FDD_D7, allowing the ZX Spectrum to poll for data availability during disk operations.

## CPLD Architecture

The Verilog implementation replicates all original discrete logic functions:

**State Management:**
Flip-flop controls ROM/RAM paging based on address access patterns.

**Address Decode:**
Generates chip select signals for ROM and RAM based on address bus and paging state.

**Bus Interface:**
Bidirectional data buses with proper tristate control for both ZX and FDD sides.

**Signal Routing:**
All control and data signals routed internally, eliminating external cable requirements.

## Design Benefits

**Integration:**
Single CPLD replaces six discrete logic chips, reducing board space and component count.

**Reliability:**
Eliminates 12-wire ribbon cable and associated connection issues.

**Compatibility:**
Maintains electrical and timing compatibility with original discrete implementation.

**Flexibility:**
CPLD logic can be updated without hardware changes, allowing protocol refinements.
