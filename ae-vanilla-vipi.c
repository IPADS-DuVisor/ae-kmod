#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_INFO */
#include <linux/init.h>    /* Needed for the macros */
#include <linux/kthread.h>

#include <asm/sbi.h>
#include <asm/smp.h>

#define SBI_TEST_TIMING_START (0xC200000)
#define SBI_TEST_TIMING_END (0xC200001)
#define SBI_TEST_LOCAL_SBI (0xC200002)
#define SBI_TEST_SEND_PRINT (0xC200003)
#define SBI_TEST_RECV_PRINT (0xC200004)

static int send_vcpuid = -1;
static int recv_vcpuid = -1;
extern bool test_vplic;
extern int *vplic_sm;
extern void msleep(unsigned int msecs);

static int send_thread(void *unused)
{
    int cpu = get_cpu();
    unsigned long total_cnt = 0;
    unsigned long total_cycle = 0;
    //send_vcpuid = rdvcpuid();
    send_vcpuid = cpu;
    pr_err("[%d] send_thread wake up on vcpuid %d...\n", cpu, send_vcpuid);
    //sbi_ecall(SBI_TEST_SEND_PRINT, 0, __LINE__, 0x4444, rdvcpuid(), cpu, 0, 0);
    sbi_ecall(SBI_TEST_SEND_PRINT, 0, __LINE__, 0x4444, send_vcpuid, cpu, 0, 0);

    csr_write(CSR_SIE, 0);
    csr_write(CSR_SIP, 0);

    smp_rmb();
    while (!test_vplic) {
        sbi_ecall(SBI_TEST_SEND_PRINT, 0, __LINE__, 0x2233,
                test_vplic, vplic_sm[0], 0, 0);
        smp_rmb();
    }
    vplic_sm[0] = 0;

    while (true) {
        unsigned long cycle_start;
        cycle_start = csr_read(CSR_CYCLE);
        //setvipi0(1UL << recv_vcpuid);
        sbi_ecall(SBI_TEST_LOCAL_SBI, 0,
                recv_vcpuid, cycle_start, 0, 0, 0, 0);
                //recv_vcpuid, 0, 0, 0, 0, 0);
        smp_rmb();
        while (vplic_sm[0] != (total_cnt + 1)) {
            //sbi_ecall(SBI_TEST_SEND_PRINT, 0, __LINE__, 0x1234,
            //        vplic_sm[0], total_cnt, 0, 0);
            smp_rmb();
        }
        total_cycle += csr_read(CSR_CYCLE) - cycle_start;
        if (++total_cnt == 10000) {
            sbi_ecall(SBI_TEST_SEND_PRINT, 0, __LINE__, 0x1234,
                    total_cnt, total_cycle, 0, 0);
            pr_err("----- total cycle %lu cnt %lu avg %lu\n",
                    total_cycle, total_cnt, total_cycle / total_cnt);
            break;
        }
        if (total_cnt % 100 == 0) {
            //sbi_ecall(SBI_TEST_SEND_PRINT, 0, __LINE__, 0x1234,
            //        total_cnt, total_cycle, 0, 0);
        }
        smp_rmb();
        while (vplic_sm[1] != total_cnt) {
            smp_rmb();
        }
    }

    return 0;
}

static int receive_thread(void *unused)
{
    int cpu = get_cpu();
    //recv_vcpuid = rdvcpuid();
    recv_vcpuid = cpu;
    pr_err("[%d] receive_thread wake up on vcpuid %d...\n", cpu, recv_vcpuid);
    csr_write(CSR_SIE, 0);
    csr_write(CSR_SIP, 0);

    //sbi_ecall(SBI_TEST_RECV_PRINT, 0, __LINE__, 0xbeef, rdvcpuid(), cpu, 0, 0);
    sbi_ecall(SBI_TEST_RECV_PRINT, 0, __LINE__, 0xbeef, recv_vcpuid, cpu, 0, 0);
    sbi_ecall(SBI_EXT_0_1_CLEAR_IPI, 0, 0, 0, 0, 0, 0, 0);
    smp_wmb();
    test_vplic = true;
    csr_write(CSR_SIE, 1UL << RV_IRQ_SOFT);
    while (true) {
        smp_wmb();
        vplic_sm[1] = vplic_sm[0];
        csr_write(CSR_SIE, 1UL << RV_IRQ_SOFT);
        if (csr_read(CSR_SIE) == 0)
            sbi_ecall(SBI_TEST_RECV_PRINT, 0, __LINE__, 0x1234,
                    csr_read(CSR_SIP), csr_read(CSR_SIE), 0, 0);
    }

    return 0;
}

static struct task_struct *skt, *rkt;
static int __init vipi_init(void)
{
    int send_cpu = smp_processor_id();
    int receive_cpu = 1 - send_cpu;

    pr_err("SIGN ae-vanilla-vipi");
    pr_err("VIPI MICROBENCHMARK START! cpu send %d recv %d, cur_vcpuid %ld\n",
            //send_cpu, receive_cpu, rdvcpuid());
            send_cpu, receive_cpu, -1);
    skt = kthread_create_on_cpu(send_thread, NULL,
            send_cpu, "vipi_bench_sender");
    rkt = kthread_create_on_cpu(receive_thread, NULL,
            receive_cpu, "vipi_bench_receiver");

    wake_up_process(rkt);
    wake_up_process(skt);
    while (true)
        msleep(100);
    return 0;
}

static void __exit vipi_exit(void)
{
    kthread_stop(skt);
    kthread_stop(rkt);
    printk("VIPI MICROBENCHMARK END!\n");
}

module_init(vipi_init);
module_exit(vipi_exit);

MODULE_LICENSE("GPL");
