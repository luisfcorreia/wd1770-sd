// ============================================================================
// ZX Spectrum + Timex FDD Interface CPLD - XC9572XL
// ============================================================================
// 
// This CPLD replaces all interface logic on both sides:
//
// ZX Spectrum side:
//   - GAL23 (I/O port decoding)
//   - GAL16 (address decoding, chip selects)
//   - LS109 J-K flip-flop (page-in/out state)
//   - LS273 octal latch (ZX to FDD data)
//   - LS244 octal buffer (FDD to ZX data)
//
// Timex FDD side:
//   - LS273 octal latch (FDD to ZX data)
//   - LS244 octal buffer (ZX to FDD data, + WD1770 DRQ on D7)
//   - Clock generation (16MHz input -> 8MHz STM32, 4MHz Z80)
//
// The 12-wire interface cable is eliminated - communication is internal.
//
// Memory Map (when paged in):
//   0x0000-0x0FFF: 4KB ROM
//   0x1000-0x1FFF: ROM echo (A12 not decoded)
//   0x2000-0x27FF: 2KB RAM
//   0x2800-0x3FFF: RAM echoes (A11-A12 not decoded)
//
// Paging:
//   Pages IN:  Read from 0x0000 or 0x0008
//   Pages OUT: Access to 0x604 region
//
// Clock Generation:
//   Input:  16MHz master clock
//   Output: 8MHz for STM32 (divide by 2)
//   Output: 4MHz for Z80 (divide by 4)
//
// ============================================================================

module zx_fdd_interface (
    // ========================================================================
    // Clock Input
    // ========================================================================
    
    input wire CLK_16MHZ,    // 16MHz master clock input
    
    // ========================================================================
    // ZX Spectrum Side Signals
    // ========================================================================
    
    // ZX Spectrum Address Bus
    input wire [15:0] A,
    
    // ZX Spectrum Control Signals
    input wire nIORQ,    // Active low I/O request
    input wire nMREQ,    // Active low memory request
    input wire nRD,      // Active low read
    input wire nWR,      // Active low write
    input wire nM1,      // Active low M1 (opcode fetch)
    
    // ZX Spectrum Data Bus (only 6 bits used: D0, D1, D4, D5, D6, D7)
    // D2 and D3 are not connected to CPLD
    inout wire D0,
    inout wire D1,
    inout wire D4,
    inout wire D5,
    inout wire D6,
    inout wire D7,
    
    // ZX Spectrum Chip Select Outputs
    output wire nZX_ROMCS,   // Disables ZX Spectrum ROM (active low)
    output wire nROM_CS,     // Interface 4KB ROM chip select (active low)
    output wire nRAM_CS,     // Interface 2KB RAM chip select (active low)
    
    // ========================================================================
    // Timex FDD Side Signals
    // ========================================================================
    
    // FDD Z80 Data Bus (only 6 bits used: D0, D1, D4, D5, D6, D7)
    // D2 and D3 are not connected to CPLD
    inout wire FDD_D0,
    inout wire FDD_D1,
    inout wire FDD_D4,
    inout wire FDD_D5,
    inout wire FDD_D6,
    inout wire FDD_D7,
    
    // FDD Control Signals
    input wire nTIIN,        // FDD Z80 read strobe (active low) - enables LS244
    input wire nTIOUT,       // FDD Z80 write strobe (active low) - latches LS273
    
    // WD1770 FDC Signal
    input wire WD1770_DRQ,   // DRQ from WD1770, routed to FDD_D7 when reading
    
    // ========================================================================
    // Clock Outputs
    // ========================================================================
    
    output wire CLK_8MHZ,    // 8MHz clock for STM32
    output wire CLK_4MHZ     // 4MHz clock for Z80
);

// ============================================================================
// Internal Signals
// ============================================================================

// Page-in/out flip-flop state (replaces LS109 J-K flip-flop)
reg paged_in;

// ZX side: Interface data latch (replaces ZX side LS273)
// Data flow: ZX writes → latched here → FDD reads
reg [5:0] zx_to_fdd;

// FDD side: Interface data latch (replaces FDD side LS273)
// Data flow: FDD writes → latched here → ZX reads
reg [5:0] fdd_to_zx;

// Clock divider counter
reg clk_div;  // Single bit: toggles at 16MHz to create 8MHz

// Internal GAL23 signals
wire gal23_o13;  // Detects 0x0000/0x0008 trigger addresses
wire gal23_o12;  // I/O write strobe (ZX side LS273 latch enable)
wire gal23_o19;  // I/O read strobe (ZX side LS244 output enable)

// Internal GAL16 signals  
wire gal16_o19;  // Address in 0x0000-0x1FFF range
wire gal16_o14;  // A3 and A2 both high (for I/O port decode)
wire gal16_f17;  // Page-in trigger (J input to flip-flop)
wire gal16_o12;  // Page-out trigger (K input to flip-flop)
wire gal16_f18;  // ZX ROM disable signal

// ============================================================================
// Clock Generation
// ============================================================================
// 16MHz input -> 8MHz output (divide by 2) -> 4MHz output (divide by 4)

// 8MHz clock: toggle at 16MHz rate
always @(posedge CLK_16MHZ) begin
    clk_div <= ~clk_div;
end

assign CLK_8MHZ = clk_div;

// 4MHz clock: use clk_div to create divide-by-4
// When clk_div toggles from 0->1, that's a rising edge of 8MHz
// We want to toggle 4MHz output at that point
reg clk_4mhz_internal;

always @(posedge CLK_16MHZ) begin
    if (clk_div & ~clk_4mhz_internal)
        clk_4mhz_internal <= 1'b1;
    else if (~clk_div & clk_4mhz_internal)
        clk_4mhz_internal <= 1'b0;
end

// Alternative simpler implementation for 4MHz
reg clk_4mhz_counter;
always @(posedge CLK_8MHZ) begin
    clk_4mhz_counter <= ~clk_4mhz_counter;
end

assign CLK_4MHZ = clk_4mhz_counter;

// ============================================================================
// GAL16 Logic - Address Decoding and Control
// ============================================================================

// o19: Detects address in 0x0000-0x1FFF range (first 8KB)
// Original equation: /o19 = /A14 * /A15 * /A13
assign gal16_o19 = ~(~A[15] & ~A[14] & ~A[13]);

// o14: Detects A3=1 AND A2=1 (used for I/O port address decoding)
// Original equation: /o14 = /A3 + /A2
// This goes LOW when both A3 and A2 are HIGH
assign gal16_o14 = ~(~A[3] | ~A[2]);

// f17: Page-in trigger - detects reads from 0x0000 or 0x0008
// Original equation: /f17 = A10 + A9 + A2 + o13 + /M1
// Goes LOW when accessing trigger addresses during normal memory access
assign gal16_f17 = ~(A[10] | A[9] | A[2] | gal23_o13 | ~nM1);

// o12: Page-out trigger - detects access to 0x604 region
// Original equation: /o12 = A10 * A9 * /A3 * A2 * /o13 * M1
assign gal16_o12 = ~(A[10] & A[9] & ~A[3] & A[2] & ~gal23_o13 & nM1);

// f18: ZX ROM disable signal
// Original equation: /f18 = /f17 * /paged_in
// ZX Spectrum ROM is disabled when the interface is paged in
assign gal16_f18 = ~(~gal16_f17 & ~paged_in);

// nROM_CS: Interface ROM chip select (active for 0x0000-0x1FFF when paged in)
// Original equation: /o16 = /A14 * /A15 * /A13 * f18 * MREQ
// Only A15, A14, A13 are decoded, so 4KB ROM appears at 0x0000 and mirrors at 0x1000
assign nROM_CS = ~(~A[15] & ~A[14] & ~A[13] & gal16_f18 & ~nMREQ);

// nRAM_CS: Interface RAM chip select (active for 0x2000-0x3FFF when paged in)
// Original equation: /o15 = /A14 * /A15 * A13 * f18 * MREQ
// Only A15, A14, A13 are decoded, so 2KB RAM appears at 0x2000 and mirrors 3 times
assign nRAM_CS = ~(~A[15] & ~A[14] & A[13] & gal16_f18 & ~nMREQ);

// nZX_ROMCS: Disable ZX Spectrum ROM when interface is paged in
assign nZX_ROMCS = gal16_f18;

// ============================================================================
// GAL23 Logic - I/O Port Decoding
// ============================================================================

// o13: Detects address 0x0000 or 0x0008 (trigger addresses for paging in)
// Original equation: /o13 = /A12 * /A8 * /A11 * /A7 * /A6 * /A5 * /A4 * /p19 * /A1 * /A0
// All specified address bits must be 0, and address must be in 0x0000-0x1FFF range
assign gal23_o13 = ~(~A[12] & ~A[8] & ~A[11] & ~A[7] & ~A[6] & 
                     ~A[5] & ~A[4] & ~gal16_o19 & ~A[1] & ~A[0]);

// o12: I/O write strobe - latches ZX side LS273
// Original equation: o12 = /IORQ * /WR * A5 * /A4 * p14 * A1 * A0
// Detects I/O write to control port (with A5=1, A4=0, A3=1, A2=1, A1=1, A0=1)
assign gal23_o12 = ~nIORQ & ~nWR & A[5] & ~A[4] & gal16_o14 & A[1] & A[0];

// o19: I/O read strobe - enables ZX side LS244 output
// Original equation: /o19 = /IORQ * /RD * A5 * /A4 * p14 * A1 * A0
// Detects I/O read from control port (same address as write)
assign gal23_o19 = ~(~nIORQ & ~nRD & A[5] & ~A[4] & gal16_o14 & A[1] & A[0]);

// ============================================================================
// Page-in/out Flip-Flop (replaces LS109 J-K flip-flop)
// ============================================================================
// Clocked by /MREQ falling edge
// J input = gal16_f17 (page-in trigger, active low)
// K input = gal16_o12 (page-out trigger, active low)

always @(negedge nMREQ) begin
    if (~gal16_f17 & ~gal16_o12)      // J=1, K=1: toggle (shouldn't happen normally)
        paged_in <= ~paged_in;
    else if (~gal16_f17 & gal16_o12)  // J=1, K=0: set (page in)
        paged_in <= 1'b1;
    else if (gal16_f17 & ~gal16_o12)  // J=0, K=1: reset (page out)
        paged_in <= 1'b0;
    // else J=0, K=0: hold current state
end

// ============================================================================
// ZX Side: Data Latch (replaces ZX side LS273)
// ============================================================================
// Latches data from ZX data bus when ZX writes to I/O port
// This data is then available for FDD to read

always @(posedge gal23_o12) begin
    zx_to_fdd[0] <= D0;
    zx_to_fdd[1] <= D1;
    zx_to_fdd[2] <= D4;
    zx_to_fdd[3] <= D5;
    zx_to_fdd[4] <= D6;
    zx_to_fdd[5] <= D7;
end

// ============================================================================
// FDD Side: Data Latch (replaces FDD side LS273)
// ============================================================================
// Latches data from FDD data bus when FDD writes
// This data is then available for ZX to read

always @(negedge nTIOUT) begin
    fdd_to_zx[0] <= FDD_D0;
    fdd_to_zx[1] <= FDD_D1;
    fdd_to_zx[2] <= FDD_D4;
    fdd_to_zx[3] <= FDD_D5;
    fdd_to_zx[4] <= FDD_D6;
    fdd_to_zx[5] <= FDD_D7;
end

// ============================================================================
// ZX Side: Bidirectional Data Bus Control (replaces ZX side LS244)
// ============================================================================
// Drive ZX data bus with data from FDD when ZX reads from I/O port
// D2 and D3 are never driven by the CPLD

assign D0 = (~gal23_o19) ? fdd_to_zx[0] : 1'bz;
assign D1 = (~gal23_o19) ? fdd_to_zx[1] : 1'bz;
assign D4 = (~gal23_o19) ? fdd_to_zx[2] : 1'bz;
assign D5 = (~gal23_o19) ? fdd_to_zx[3] : 1'bz;
assign D6 = (~gal23_o19) ? fdd_to_zx[4] : 1'bz;
assign D7 = (~gal23_o19) ? fdd_to_zx[5] : 1'bz;

// ============================================================================
// FDD Side: Bidirectional Data Bus Control (replaces FDD side LS244)
// ============================================================================
// Drive FDD data bus when FDD reads from interface
// Special case: FDD_D7 gets WD1770_DRQ signal instead of data from ZX
// This matches the original hardware where DRQ was routed through the LS244

assign FDD_D0 = (~nTIIN) ? zx_to_fdd[0] : 1'bz;
assign FDD_D1 = (~nTIIN) ? zx_to_fdd[1] : 1'bz;
assign FDD_D4 = (~nTIIN) ? zx_to_fdd[2] : 1'bz;
assign FDD_D5 = (~nTIIN) ? zx_to_fdd[3] : 1'bz;
assign FDD_D6 = (~nTIIN) ? zx_to_fdd[4] : 1'bz;
assign FDD_D7 = (~nTIIN) ? WD1770_DRQ : 1'bz;  // DRQ from WD1770, not from ZX
// FDD_D2 and FDD_D3 are never driven by CPLD

endmodule

// ============================================================================
// Pin Count Summary for XC9572XL
// ============================================================================
//
// Inputs (25 pins):
//   - CLK_16MHZ      : 1 pin (clock input)
//   - A[15:0]        : 16 pins (ZX address bus)
//   - nIORQ          : 1 pin
//   - nMREQ          : 1 pin
//   - nRD            : 1 pin
//   - nWR            : 1 pin
//   - nM1            : 1 pin
//   - nTIIN          : 1 pin
//   - nTIOUT         : 1 pin
//   - WD1770_DRQ     : 1 pin
//
// Outputs (5 pins):
//   - nZX_ROMCS      : 1 pin
//   - nROM_CS        : 1 pin
//   - nRAM_CS        : 1 pin
//   - CLK_8MHZ       : 1 pin (generated clock)
//   - CLK_4MHZ       : 1 pin (generated clock)
//
// Bidirectional (12 pins):
//   - D0, D1, D4-D7  : 6 pins (ZX data bus)
//   - FDD_D0, D1, D4-D7 : 6 pins (FDD data bus)
//
// Total I/O: 42 pins
//
// Package requirements:
//   - XC9572XL-VQ64: 64 pins total, 52 I/O available → FITS with 10 spare pins
//   - XC9572XL-TQ44: 44 pins total, 36 I/O available → DOES NOT FIT (need 42)
//
// JTAG pins (reserved on all packages):
//   - TCK, TMS, TDI, TDO
//
// ============================================================================
