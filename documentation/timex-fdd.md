# Timex FDD computer

* Z80 CPU clocked at 4Mhz
* 2Kb ROM (only using 101 bytes for bootloader)
* 64Kb Static RAM (HM628128 chip using only half)
* WD1770 FDD controller (which will be emulated with a twist)
* WD2123 Dual RS232 serial controller (won't be emulatedfor now)
* assorted glue logic, including a 12 bit interface (interface which be replaced by CPLD)
* +5V power for all
