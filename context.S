    .section .text
    .global context_switch
/* context_switch(unsigned long *old_regs, unsigned long *new_regs)
   old_regs/new_regs point to arrays of 14 unsigned long:
   [0]=ra, [1]=sp, [2]=s0, [3]=s1, ... [13]=s11
*/
context_switch:
    /* a0 = old_regs, a1 = new_regs */

    /* store caller-saved registers: ra, sp, s0-s11 into *a0 */
    sd ra, 0(a0)        /* regs[0] = ra */
    sd sp, 8(a0)        /* regs[1] = sp */
    sd s0, 16(a0)
    sd s1, 24(a0)
    sd s2, 32(a0)
    sd s3, 40(a0)
    sd s4, 48(a0)
    sd s5, 56(a0)
    sd s6, 64(a0)
    sd s7, 72(a0)
    sd s8, 80(a0)
    sd s9, 88(a0)
    sd s10, 96(a0)
    sd s11, 104(a0)

    /* now load registers from new_regs (a1) */
    ld ra, 0(a1)
    ld sp, 8(a1)
    ld s0, 16(a1)
    ld s1, 24(a1)
    ld s2, 32(a1)
    ld s3, 40(a1)
    ld s4, 48(a1)
    ld s5, 56(a1)
    ld s6, 64(a1)
    ld s7, 72(a1)
    ld s8, 80(a1)
    ld s9, 88(a1)
    ld s10, 96(a1)
    ld s11, 104(a1)

    ret
