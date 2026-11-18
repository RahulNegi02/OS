
.section .text
.code64

.global test_write_readonly
.global test_write_readonly_recovery
.global test_execute_nx
.global test_execute_nx_recovery

/*
 * Read-only protection test
 */

test_write_readonly:
    movb $0xAA, (%rdi)
    movl $0, %eax
    ret

test_write_readonly_recovery:
    movl $1, %eax
    ret

/*
 * NX execution protection test
 */

test_execute_nx:
    call *%rdi
    movl $0, %eax
    ret

test_execute_nx_recovery:
    /*
     * Remove the return address pushed by CALL.
     * Execution faulted before the target function ran.
     */
    addq $8, %rsp

    movl $1, %eax
    ret

.section .note.GNU-stack,"",@progbits

