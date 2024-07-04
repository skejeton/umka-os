; ---***---
; @authors ske
; @created 7/1/2024
; @note Assembly-implemented functions.
; ---***---

section .text

; NOTE: Doesn't save floating point registers.
global setjmp
setjmp:
  mov ecx, [esp+4]
  mov [ecx], ebp
  mov [ecx+4], ebx
  mov [ecx+8], edi
  mov [ecx+12], esi
  mov [ecx+16], esp
  mov eax, [esp]
  mov [ecx+20], eax
  xor eax, eax
  ret

global longjmp
longjmp:
  mov ecx, [esp+4]
  mov eax, [esp+8]
  mov ebp, [ecx]
  mov ebx, [ecx+4]
  mov edi, [ecx+8]
  mov esi, [ecx+12]
  mov esp, [ecx+16]
  add esp, 4
  jmp [ecx+20]