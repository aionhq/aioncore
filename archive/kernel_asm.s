.section .text
.global kmain
.type kmain, @function

kmain:
    # VGA text buffer address
    movl $0xB8000, %edi

    # Clear screen (fill with spaces)
    movl $0x0F200F20, %eax    # Two space characters with white on black
    movl $1000, %ecx          # 80*25/2 = 1000 dwords
    rep stosl

    # Reset pointer to start of VGA buffer
    movl $0xB8000, %edi

    # Write "Hello from Assembly Kernel!"
    # Each character is: ASCII byte + color byte (0x0A = light green)
    movw $0x0A48, (%edi)      # 'H'
    movw $0x0A65, 2(%edi)     # 'e'
    movw $0x0A6C, 4(%edi)     # 'l'
    movw $0x0A6C, 6(%edi)     # 'l'
    movw $0x0A6F, 8(%edi)     # 'o'
    movw $0x0A20, 10(%edi)    # ' '
    movw $0x0A66, 12(%edi)    # 'f'
    movw $0x0A72, 14(%edi)    # 'r'
    movw $0x0A6F, 16(%edi)    # 'o'
    movw $0x0A6D, 18(%edi)    # 'm'
    movw $0x0A20, 20(%edi)    # ' '
    movw $0x0A41, 22(%edi)    # 'A'
    movw $0x0A73, 24(%edi)    # 's'
    movw $0x0A73, 26(%edi)    # 's'
    movw $0x0A65, 28(%edi)    # 'e'
    movw $0x0A6D, 30(%edi)    # 'm'
    movw $0x0A62, 32(%edi)    # 'b'
    movw $0x0A6C, 34(%edi)    # 'l'
    movw $0x0A79, 36(%edi)    # 'y'
    movw $0x0A20, 38(%edi)    # ' '
    movw $0x0A4B, 40(%edi)    # 'K'
    movw $0x0A65, 42(%edi)    # 'e'
    movw $0x0A72, 44(%edi)    # 'r'
    movw $0x0A6E, 46(%edi)    # 'n'
    movw $0x0A65, 48(%edi)    # 'e'
    movw $0x0A6C, 50(%edi)    # 'l'
    movw $0x0A21, 52(%edi)    # '!'

    # Second line - "Successfully booted with QEMU" (cyan - 0x0B)
    movl $0xB80A0, %edi       # Line 2 (160 bytes = 80 chars * 2)
    movw $0x0B53, (%edi)      # 'S'
    movw $0x0B75, 2(%edi)     # 'u'
    movw $0x0B63, 4(%edi)     # 'c'
    movw $0x0B63, 6(%edi)     # 'c'
    movw $0x0B65, 8(%edi)     # 'e'
    movw $0x0B73, 10(%edi)    # 's'
    movw $0x0B73, 12(%edi)    # 's'
    movw $0x0B66, 14(%edi)    # 'f'
    movw $0x0B75, 16(%edi)    # 'u'
    movw $0x0B6C, 18(%edi)    # 'l'
    movw $0x0B6C, 20(%edi)    # 'l'
    movw $0x0B79, 22(%edi)    # 'y'
    movw $0x0B20, 24(%edi)    # ' '
    movw $0x0B62, 26(%edi)    # 'b'
    movw $0x0B6F, 28(%edi)    # 'o'
    movw $0x0B6F, 30(%edi)    # 'o'
    movw $0x0B74, 32(%edi)    # 't'
    movw $0x0B65, 34(%edi)    # 'e'
    movw $0x0B64, 36(%edi)    # 'd'
    movw $0x0B20, 38(%edi)    # ' '
    movw $0x0B77, 40(%edi)    # 'w'
    movw $0x0B69, 42(%edi)    # 'i'
    movw $0x0B74, 44(%edi)    # 't'
    movw $0x0B68, 46(%edi)    # 'h'
    movw $0x0B20, 48(%edi)    # ' '
    movw $0x0B51, 50(%edi)    # 'Q'
    movw $0x0B45, 52(%edi)    # 'E'
    movw $0x0B4D, 54(%edi)    # 'M'
    movw $0x0B55, 56(%edi)    # 'U'

    # Third line - "Kernel running in 32-bit protected mode" (yellow - 0x0E)
    movl $0xB8140, %edi       # Line 3
    movw $0x0E4B, (%edi)      # 'K'
    movw $0x0E65, 2(%edi)     # 'e'
    movw $0x0E72, 4(%edi)     # 'r'
    movw $0x0E6E, 6(%edi)     # 'n'
    movw $0x0E65, 8(%edi)     # 'e'
    movw $0x0E6C, 10(%edi)    # 'l'
    movw $0x0E20, 12(%edi)    # ' '
    movw $0x0E72, 14(%edi)    # 'r'
    movw $0x0E75, 16(%edi)    # 'u'
    movw $0x0E6E, 18(%edi)    # 'n'
    movw $0x0E6E, 20(%edi)    # 'n'
    movw $0x0E69, 22(%edi)    # 'i'
    movw $0x0E6E, 24(%edi)    # 'n'
    movw $0x0E67, 26(%edi)    # 'g'
    movw $0x0E20, 28(%edi)    # ' '
    movw $0x0E69, 30(%edi)    # 'i'
    movw $0x0E6E, 32(%edi)    # 'n'
    movw $0x0E20, 34(%edi)    # ' '
    movw $0x0E33, 36(%edi)    # '3'
    movw $0x0E32, 38(%edi)    # '2'
    movw $0x0E2D, 40(%edi)    # '-'
    movw $0x0E62, 42(%edi)    # 'b'
    movw $0x0E69, 44(%edi)    # 'i'
    movw $0x0E74, 46(%edi)    # 't'
    movw $0x0E20, 48(%edi)    # ' '
    movw $0x0E70, 50(%edi)    # 'p'
    movw $0x0E72, 52(%edi)    # 'r'
    movw $0x0E6F, 54(%edi)    # 'o'
    movw $0x0E74, 56(%edi)    # 't'
    movw $0x0E65, 58(%edi)    # 'e'
    movw $0x0E63, 60(%edi)    # 'c'
    movw $0x0E74, 62(%edi)    # 't'
    movw $0x0E65, 64(%edi)    # 'e'
    movw $0x0E64, 66(%edi)    # 'd'
    movw $0x0E20, 68(%edi)    # ' '
    movw $0x0E6D, 70(%edi)    # 'm'
    movw $0x0E6F, 72(%edi)    # 'o'
    movw $0x0E64, 74(%edi)    # 'd'
    movw $0x0E65, 76(%edi)    # 'e'

    ret

.size kmain, . - kmain
