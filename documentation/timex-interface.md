# Timex FDD Interface

## Description

This interface allows a Sinclair ZX Spectrum to talk with the Timex FDD Computer

* 4Kb ROM mapped as 0x0000 to 01fff (implements code for trapping BASIC instructions)
* 2Kb RAM mapped as 0x2000 to 02fff (work ram for interface code)
* GAL chips for selecting rom and ram, flip flop for maintaining state
* LS273 and LS244 for interfacing

## Work as started to use a XC9572XL CPLD 

ZX Spectrum side (CPLD replacement):

XC9572XL CPLD replaces GAL23, GAL16, LS109, LS273, LS244
4KB ROM, 2KB RAM (with echoes)
Pages in/out based on address access
6-bit parallel interface to FDD

Summary of what's included:

Complete GAL23 and GAL16 logic - all equations translated to Verilog
LS109 J-K flip-flop replacement - page-in/out state management
Both LS273 latches - ZX side and FDD side
Both LS244 buffers - ZX side and FDD side
WD1770 DRQ routing - appears on FDD_D7 when FDD reads
Comprehensive comments - explaining each section and the original equations
Pin count summary - showing it needs VQ64 package (39 I/O pins required)

Key features:

12-wire cable eliminated - all communication is internal
ROM/RAM with echoes - matches original incomplete address decoding
Paging logic - pages in at 0x0000/0x0008, pages out at 0x604
Bidirectional data buses - properly handled with tristate logic

