ENTRY(start)
SECTIONS {
    . = 0x7C00;
  
    .text :
    {
        *(.boot)
        *(.text)
    }
    .data : 
    {
        *(.data)
    }
    .rodata : 
    {
        *(.rodata)
    }
    .bss : 
    {
        *(.bss)    
    }
}