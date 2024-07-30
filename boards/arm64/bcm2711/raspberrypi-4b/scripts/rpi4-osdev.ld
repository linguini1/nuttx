OUTPUT_ARCH(aarch64) /* Architecture to use */

ENTRY(__start) /* Entry point for the program */

SECTIONS
{
    . = 0x80000;     /* Kernel load address for AArch64 */
    _start = .;
    .text : { 
        _stext = .;
        *(.start .start.*)      /* Place __start here */
        KEEP(*(.text.boot)) *(.text .text.* .gnu.linkonce.t*)
    }
    _etext = .;
    _sztext = _etext - _stext;

    .init_section : {
          _sinit = ABSOLUTE(.);
          KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
          KEEP(*(.init_array .ctors))
          _einit = ABSOLUTE(.);
    }

    .rodata : { 
        _srodata = .;
        *(.rodata .rodata.* .gnu.linkonce.r*) 
    }
    _erodata = .;
    _szrodata = _erodata - _srodata;
    PROVIDE(_data = .);
    .data : { 
        _sdata = .;
        *(.data .data.* .gnu.linkonce.d*) 
    }
    _edata = .;

    .initstack : {             /* INIT STACK */
         _s_initstack = .;
         *(.initstack)
         . = ALIGN(16);
    }
    . = ALIGN(4096);
    _e_initstack = . ;
    g_idle_topstack = . ;

    _szdata = _e_initstack - _sdata;

    .bss (NOLOAD) : {
        . = ALIGN(16);
        _sbss = .;
        *(.bss .bss.*)
        *(COMMON)
    }
    _ebss = .;
    _szbss = _ebss - _sbss;

   /DISCARD/ : { *(.comment) *(.gnu*) *(.note*) *(.eh_frame*) }
}
