; ---***---
; @authors ske
; @created 7/1/2024
; @note IRQ wrappers.
; ---***---

section .text

align 4
extern handleIrq1
global irq1
irq1:
  pushad
  call handleIrq1
  popad
  iret

align 4
extern handleIrq12
global irq12
irq12:
  pushad
  call handleIrq12
  popad
  iret
