// ============================================================================
// ZX Spectrum + Timex FDD Interface CPLD - XC9572XL (Validation Build)
// ============================================================================
// 
// ZX Spectrum side:
//   - GAL23 (I/O port decoding)
//   - GAL16 (address decoding, chip selects)
//   - LS109 J-K flip-flop (page-in/out state)
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
// ============================================================================

module zx_fdd_interface (
    
    // ========================================================================
    // ZX Spectrum Side Signals
    // ========================================================================
    
    // ZX Spectrum Address Bus (actual minimal set from GAL chips - 14 lines)
    // A0, A1 not connected to GALs - saves 2 pins
    input wire A2,
    input wire A3,
    input wire A4,
    input wire A5,
    input wire A6,
    input wire A7,
    input wire A8,
    input wire A9,
    input wire A10,
    input wire A11,
    input wire A12,
    input wire A13,
    input wire A14,
    input wire A15,
    
    // ZX Spectrum Control Signals
    input wire nIORQ,
    input wire nMREQ,
    input wire nRD,
    input wire nWR,
    input wire nM1,
    
    // ZX Spectrum Chip Select Outputs
    output wire nZX_ROMCS,
    output wire nROM_CS,
    output wire nRAM_CS,
    
    // ========================================================================
    // Validation Output Pins
    // ========================================================================
    
    // GAL23 validation outputs
    output wire gal23_o12,
    output wire gal23_o19,
    
    // GAL16 validation outputs
    output wire gal16_o19,
    output wire gal16_o14,
    output wire gal16_f18
);

// ============================================================================
// Internal Signals
// ============================================================================

reg paged_in;

wire gal23_o13;
wire gal16_f17;
wire gal16_o12;

// ============================================================================
// GAL16 Logic - Address Decoding and Control
// ============================================================================

assign gal16_o19 = ~(~A15 & ~A14 & ~A13);

assign gal16_o14 = ~(~A3 | ~A2);

assign gal16_f17 = ~(A10 | A9 | A2 | gal23_o13 | ~nM1);

assign gal16_o12 = ~(A10 & A9 & ~A3 & A2 & ~gal23_o13 & nM1);

assign gal16_f18 = ~(~gal16_f17 & ~paged_in);

assign nROM_CS = ~(~A15 & ~A14 & ~A13 & gal16_f18 & ~nMREQ);

assign nRAM_CS = ~(~A15 & ~A14 & A13 & gal16_f18 & ~nMREQ);

assign nZX_ROMCS = gal16_f18;

// ============================================================================
// GAL23 Logic - I/O Port Decoding
// ============================================================================

assign gal23_o13 = ~(~A12 & ~A8 & ~A11 & ~A7 & ~A6 & ~A5 & ~A4 & ~gal16_o19);

assign gal23_o12 = ~nIORQ & ~nWR & A5 & ~A4 & gal16_o14;

assign gal23_o19 = ~(~nIORQ & ~nRD & A5 & ~A4 & gal16_o14);

// ============================================================================
// Page-in/out Flip-Flop (replaces LS109 J-K flip-flop)
// ============================================================================

always @(negedge nMREQ) begin
    if (~gal16_f17 & ~gal16_o12)
        paged_in <= ~paged_in;
    else if (~gal16_f17 & gal16_o12)
        paged_in <= 1'b1;
    else if (gal16_f17 & ~gal16_o12)
        paged_in <= 1'b0;
end

endmodule
