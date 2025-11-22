# x86 Context Switch
#
# void context_switch(cpu_context_t* old_ctx, cpu_context_t* new_ctx)
#
# Saves current CPU state to old_ctx, loads new state from new_ctx.
# Must match the layout of cpu_context_t in include/kernel/task.h
#
# cpu_context_t layout (from task.h):
#   +0:  edi
#   +4:  esi
#   +8:  ebx
#   +12: ebp
#   +16: esp
#   +20: eip

.section .text
.global context_switch
.type context_switch, @function

context_switch:
    # Set up stack frame
    pushl %ebp
    movl %esp, %ebp

    # Get arguments
    movl 8(%ebp), %eax      # eax = old_ctx
    movl 12(%ebp), %edx     # edx = new_ctx

    # ============================================
    # Save current context to old_ctx
    # ============================================
    movl %edi, 0(%eax)      # Save EDI
    movl %esi, 4(%eax)      # Save ESI
    movl %ebx, 8(%eax)      # Save EBX
    movl %ebp, 12(%eax)     # Save EBP
    movl %esp, 16(%eax)     # Save ESP (current stack pointer)

    # Save return address as EIP using call/pop trick
    call 1f
1:  popl %ecx
    movl %ecx, 20(%eax)     # Save EIP

    # ============================================
    # Load new context from new_ctx
    # ============================================
    movl 16(%edx), %esp     # Restore ESP
    movl 12(%edx), %ebp     # Restore EBP
    movl 0(%edx), %edi      # Restore EDI
    movl 4(%edx), %esi      # Restore ESI
    movl 8(%edx), %ebx      # Restore EBX

    # Jump to saved EIP
    jmp *20(%edx)
