; This is reused from my previous OS project, not written during this time.

; ---***---
; @authors ske
; @created 12/25/2022
; @note X86 Bootloader.
; ---***---

; @note Size of the image in sectors * 63
%define KBOOT_SIZE_SEC 15
; @note In protected mode segments become offsets into the GDT.
;       This is different from real mode.
%define CODE_SEG gdt_code-gdt_start
%define DATA_SEG gdt_data-gdt_start
%define VESA_MODE_NUBMER 0x118

; @note Bootloaders start addressing at 0x7C00.
;       The 0x7C00 offset is defined in the linker script not here.
section .boot

; @note Bootloaders start in 16 bit.
bits 16

; @note Export start to the linker
global start

start:
  mov ax, 0x0
  mov ds, ax
  mov es, ax
  mov ss, ax
  mov sp, 0x7C00

  ; @note Enable A20 to access even Mb's of RAM.
  mov     ax, 2403h
  int     15h
 
  .loop:
    mov ah, 2h
    mov al, [sectors]
    mov ch, 0     ; cylinder 
    mov cl, [sector]
    mov dh, [head]
    mov bx, [osegment]
    mov es, bx
    mov bx, 0  
    int 13h

    mov byte[sectors], 63
    mov byte[sector], 1
    mov bx, [toadd]
    add word [osegment], bx
    mov word [toadd], 0x7E0
    add byte[head], 1
    add dword [cnt], 1
    cmp dword [cnt], KBOOT_SIZE_SEC
    jne .loop

  mov ax, 0x0
  mov ds, ax
  mov es, ax

  jmp 0x0000:0x7E00

align 4

toadd:
  dw 0x7C0
osegment:
  dw 0x7E0
sectors:
  db 62
sector:
  db 2
head:
  db 0
sectors_per_track:
  db 0
cnt:
  dd 0
drive_number:
  db 0

; @note {
; Limit = ending memory location
; Base = starting memory location
; }
; The base/limit are spread out,
; but they are all very simple integers.
; @note Limit is 20 bit which doens't cover whole 32 bit space, but is fixed
;       with the 4 kilobyte granularity flag.
;
; Format:
;   Limit (low word): u16
;   Base (low word): u16
;   Base (middle byte): u8
;   Access (present?, privilege ring hi, privilege ring lo,
;     is not task segment (code, data)?, is executable?, DC[0], RW[1],
;     has been accessed?): u8
;   Limit (high nibble): u4
;   Flags (1 byte or 4kb limit multiplier, protected mode segment?, 
;          long mode segment?[2], reserved)
;   Base (high byte): u8
gdt_start:
  ; @note null entry
  dq 0 
gdt_code:
  dw 0xFFFF     ; Limit
  dw 0x0000     ; Base
  db 0x00       ; Base
  db 10011010b  ; Access
  db 11001111b  ; Flags + Limit
  db 0x00       ; Base
gdt_data:
  dw 0xFFFF     ; Limit
  dw 0x0000     ; Base
  db 0x00       ; Base
  db 10010010b  ; Access
  db 11001111b  ; Flags + Limit
  db 0x00       ; Base
gdt_end:

gdt_desc:
  dw gdt_end - gdt_start ; GDT size 
  dd gdt_start ; GDT address 

%assign space_taken $-$$
%if space_taken > 510 
  %warning Space taken space_taken
  %error Bootloader is too big.
%endif

; @note Fill the rest with zeros to align bootloader magic.
times 510-($-$$) db 0 

; @note Bootloader magic.
dw 0xAA55

load2:
  sti
  
  mov ax, 0x4F00         ; Get VESA hardware information
  mov di, vesa_vbe_info  ; Move VESA struct to DI
  int 0x10


  mov ax, 0x4F01         ; Load VESA mode information
  mov di, vesa_mode_info_struct
  mov cx, VESA_MODE_NUBMER
  int 0x10

  mov ax, 0x4F02         ; Enable VESA mode
  mov bx, VESA_MODE_NUBMER
  or bx, 0x4000          ; Set some flag
  mov di, 0
  int 0x10

  cli

  ; @note LGDT identity memory map
  lgdt [gdt_desc]

  ; @note Enable protected mode by setting first bit in control register 0.
  mov eax, cr0
  or eax, 1
  mov cr0, eax

  jmp CODE_SEG:after

bits 32

after:
  mov esp, 0xE00000  ; Set the stack pointer
  ; @note Setup segments.
  mov ax, DATA_SEG
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax

extern kmain
  call kmain
  cli
  hlt

; [0] (https://wiki.osdev.org/Global_Descriptor_Table) 
;   DC: Direction bit/Conforming bit.
;   For data selectors: Direction bit. If clear (0) the segment grows up.
;     If set (1) the segment grows down, ie. the Offset has to be greater than
;     the Limit.
;   For code selectors: Conforming bit.
;     If clear (0) code in this segment can only be executed from the ring set
;     in DPL. If set (1) code in this segment can be executed from an equal or
;     lower privilege level. For example, code in ring 3 can far-jump to
;     conforming code in a ring 2 segment. The DPL field represent the highest
;     privilege level that is allowed to execute the segment. For example, code
;     in ring 0 cannot far-jump to a conforming code segment where DPL is 2,
;     while code in ring 2 and 3 can. Note that thehttps://youtu.be/25zdbdxTgaE privilege level remains the
;     same, ie. a far-jump from ring 3 to a segment with a DPL of 2 remains in
;     ring 3 after the jump.
;
; [1] (https://wiki.osdev.org/Global_Descriptor_Table) 
;   RW: Readable bit/Writable bit.
;   For code segments: Readable bit.
;     If clear (0), read access for this segment is not allowed.
;     If set (1) read access is allowed. Write access is never allowed for code
;     segments.
;   For data segments: Writeable bit.
;     If clear (0), write access for this segment is not allowed.
;     If set (1) write access is allowed. Read access is always allowed for
;     data segments.
;
; [2] Protected mode segment flag (bit 3) must be disabled for long mode segments.

vesa_vbe_info:
    .VbeSignature db 'VESA' ; VBE Signature
    .VbeVersion dw 0200h ; VBE Version
    .OemStringPtr dd 0 ; Pointer to OEM String
    .Capabilities times 4 db 0 ; Capabilities of graphics controller
    .VideoModePtr dd 0 ; Pointer to VideoModeList
    .TotalMemory dw 0 ; Number of 64kb memory blocks
    ; Added for VBE 2.0
    .OemSoftwareRev dw 0 ; VBE implementation Software revision
    .OemVendorNamePtr dd 0 ; Pointer to Vendor Name String
    .OemProductNamePtr dd 0 ; Pointer to Product Name String
    .OemProductRevPtr dd 0 ; Pointer to Product Revision String
    .Reserved times 222 db 0 ; Reserved for VBE implementation scratch
    ; area
    .OemData times 256 db 0 ; Data Area for OEM Strings

global vesa_mode_info_struct
vesa_mode_info_struct:
    .ModeAttributes dw 0 ; 0 mode attributes
    .WinAAttributes db 0 ; 2 window A attributes
    .WinBAttributes db 0 ; 3 window B attributes
    .WinGranularity dw 0 ; 4 window granularity
    .WinSize dw 0 ; 6 window size
    .WinASegment dw 0 ; 8 window A start segment
    .WinBSegment dw 0 ; 10 window B start segment
    .WinFuncPtr dd 0 ; 12 pointer to window function
    .BytesPerScanLine dw 0 ; 16 bytes per scan line
    ; Mandatory information for VBE 1.2 and above
    .XResolution dw 0 ; 18 horizontal resolution in pixels or characters
    .YResolution dw 0 ; 20 vertical resolution in pixels or characters
    .XCharSize db 0 ; 21 character cell width in pixels
    .YCharSize db 0 ; 22 character cell height in pixels
    .NumberOfPlanes db 0 ; number of memory planes
    .BitsPerPixel db 0 ; bits per pixel
    .NumberOfBanks db 0 ; number of banks
    .MemoryModel db 0 ; memory model type
    .BankSize db 0 ; bank size in KB
    .NumberOfImagePages db 0 ; number of images
    .Reserved db 1 ; reserved for page function
    ; Direct Color fields (required for direct/6 and YUV/7 memory models)
    .RedMaskSize db 0 ; size of direct color red mask in bits
    .RedFieldPosition db 0 ; bit position of lsb of red mask
    .GreenMaskSize db 0 ; size of direct color green mask in bits
    .GreenFieldPosition db 0 ; bit position of lsb of green mask
    .BlueMaskSize db 0 ; size of direct color blue mask in bits
    .BlueFieldPosition db 0 ; bit position of lsb of blue mask
    .RsvdMaskSize db 0 ; size of direct color reserved mask in bits
    .RsvdFieldPosition db 0 ; bit position of lsb of reserved mask
    .DirectColorModeInfo db 0 ; direct color mode attributes
    ; Mandatory information for VBE 2.0 and above
    .PhysBasePtr dd 0 ; physical address for flat memory frame buffer
    .OffScreenMemOffset dd 0 ; pointer to start of off screen memory
    .OffScreenMemSize dw 0 ; amount of off screen memory in 1k 
    resb 206

%include "src/sys/lib.s"
%include "src/sys/irq.s"
