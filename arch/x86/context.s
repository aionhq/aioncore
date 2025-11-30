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
#   +24: cs
#   +28: ss
#   +32: ds
#   +36: es
#   +40: fs
#   +44: gs
#   +48: eflags

.section .text
.global context_switch
.type context_switch, @function

context_switch:
    # Stack layout on entry (before any pushes):
    #   [esp+0]  = return address (where to resume in caller)
    #   [esp+4]  = old_ctx arg
    #   [esp+8]  = new_ctx arg

    # We'll push callee-saved regs, so adjust offsets accordingly

    # Get arguments (before we modify stack)
    movl 4(%esp), %eax      # eax = old_ctx
    movl 8(%esp), %edx      # edx = new_ctx

    # ============================================
    # Save current context to old_ctx
    # ============================================
    # Save callee-saved registers
    movl %edi, 0(%eax)      # Save EDI
    movl %esi, 4(%eax)      # Save ESI
    movl %ebx, 8(%eax)      # Save EBX
    movl %ebp, 12(%eax)     # Save EBP (register value, not dereferenced)

    # Save caller's stack pointer
    # After context_switch returns, ESP will be current ESP + 12
    # (+4 for return address, +4 for old_ctx arg, +4 for new_ctx arg)
    leal 12(%esp), %ecx     # Compute caller's ESP after return
    movl %ecx, 16(%eax)     # Save caller's ESP

    # Save return address (where we resume in caller)
    movl 0(%esp), %ecx      # Get return address from stack
    movl %ecx, 20(%eax)     # Save as EIP

    # Save segment registers
    movw %cs, %cx           # Read CS (16-bit)
    movl %ecx, 24(%eax)     # Save CS
    movw %ss, %cx           # Read SS (16-bit)
    movl %ecx, 28(%eax)     # Save SS
    movl %ds, %ecx
    movl %ecx, 32(%eax)     # Save DS
    movl %es, %ecx
    movl %ecx, 36(%eax)     # Save ES
    movl %fs, %ecx
    movl %ecx, 40(%eax)     # Save FS
    movl %gs, %ecx
    movl %ecx, 44(%eax)     # Save GS

    # Save EFLAGS
    pushf                   # Push EFLAGS onto stack
    popl %ecx               # Pop into ECX
    movl %ecx, 48(%eax)     # Save EFLAGS

    # ============================================
    # Load new context from new_ctx
    # ============================================
    # Check target CS FIRST to determine kernel→kernel or kernel→user transition
    # CRITICAL: We cannot load ring 3 selectors into SS from ring 0!
    movl 24(%edx), %ecx     # Load target CS
    andl $0x03, %ecx        # Extract RPL (bits 0-1)
    cmpl $0, %ecx           # RPL == 0? (kernel code)
    je .context_switch_kernel

    # ========== Userspace Path (RPL == 3) ==========
    # For ring 0→ring 3 transition, use iret
    # Do NOT restore segment registers manually - iret will load them
.context_switch_user:
    # Restore callee-saved registers first
    movl 0(%edx), %edi      # Restore EDI
    movl 4(%edx), %esi      # Restore ESI
    movl 8(%edx), %ebx      # Restore EBX
    movl 12(%edx), %ebp     # Restore EBP

    # Build iret stack frame (iret requires specific order)
    # Stack layout after these pushes:
    # [esp+0]  = EIP
    # [esp+4]  = CS
    # [esp+8]  = EFLAGS
    # [esp+12] = ESP (user stack)
    # [esp+16] = SS
    pushl 28(%edx)          # Push SS (user data selector)
    pushl 16(%edx)          # Push ESP (user stack pointer)
    pushl 48(%edx)          # Push EFLAGS
    pushl 24(%edx)          # Push CS (user code selector)
    pushl 20(%edx)          # Push EIP (user entry point)

    # Load user data segments into DS/ES/FS/GS
    # We CAN load user selectors into these from ring 0
    movl 32(%edx), %ecx     # Load target DS
    movl %ecx, %ds
    movl 36(%edx), %ecx     # Load target ES
    movl %ecx, %es
    movl 40(%edx), %ecx     # Load target FS
    movl %ecx, %fs
    movl 44(%edx), %ecx     # Load target GS
    movl %ecx, %gs

    iret                    # Atomically switch to ring 3
                            # CPU will load CS:EIP, SS:ESP, and EFLAGS

    # ========== Kernel Path (RPL == 0) ==========
    # For ring 0→ring 0 transition, use fast path
.context_switch_kernel:
    # Restore EFLAGS
    movl 48(%edx), %ecx     # Load EFLAGS
    pushl %ecx              # Push onto stack
    popf                    # Pop into EFLAGS

    # Restore segment registers (all kernel selectors)
    movl 28(%edx), %ecx     # Load SS
    movl %ecx, %ss
    movl 32(%edx), %ecx     # Load DS
    movl %ecx, %ds
    movl 36(%edx), %ecx     # Load ES
    movl %ecx, %es
    movl 40(%edx), %ecx     # Load FS
    movl %ecx, %fs
    movl 44(%edx), %ecx     # Load GS
    movl %ecx, %gs

    # Restore callee-saved registers
    movl 0(%edx), %edi      # Restore EDI
    movl 4(%edx), %esi      # Restore ESI
    movl 8(%edx), %ebx      # Restore EBX
    movl 12(%edx), %ebp     # Restore EBP

    # Restore kernel ESP and jump to EIP
    movl 16(%edx), %esp     # Restore kernel ESP
    jmp *20(%edx)           # Jump to target EIP
