#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_INFO */
#include <linux/init.h>    /* Needed for the macros */

#include <asm/sbi.h>

#define SBI_EXT_0_1_DEBUG_START 0x11
#define SBI_EXT_0_1_DEBUG_END   0x12
#define SBI_TEST_LOCAL_SBI (0xC200002)
#define SBI_TEST_RECV_PRINT (0xC200004)

extern volatile bool test_vplic;
extern volatile unsigned long test_vplic_num;
extern volatile int test_vplic_irq;
extern int *vplic_sm;

static int __init vplic_init(void)
{
    printk("VPLIC TEST START!\n");
    csr_write(CSR_SIE, 0);
    csr_write(CSR_SIP, 0);
    test_vplic_irq = 34;
    test_vplic = true;
    sbi_ecall(SBI_TEST_LOCAL_SBI, 0, test_vplic_irq,
            csr_read(CSR_SIE), 0, 0, 0, 0);
    csr_write(CSR_SIE, (1UL << RV_IRQ_EXT));
    while (true) {
        smp_wmb();
        vplic_sm[1] = vplic_sm[0];
    }
    printk("UNREACHABLE!\n");
    return 0;
}

static void __exit vplic_exit(void)
{
}

module_init(vplic_init);
module_exit(vplic_exit);

MODULE_LICENSE("GPL");
