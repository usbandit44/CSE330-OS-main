#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arvin Edouard, Kyle Phan, Dean Prach");

static int pid = 0;

module_param(pid, int, 0644);

unsigned long rss_counter = 0; 
unsigned long wss_counter = 0; 
unsigned long swap_counter = 0;

unsigned long timer_interval_ns = 10e9; // 10-second timer
static struct hrtimer hr_timer;

struct task_struct *task;

void walk_pages(void){
    struct vm_area_struct *vma = NULL;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *ptep, pte;
    if(pid > 0){
        task = pid_task(find_vpid(pid), PIDTYPE_PID);
        struct mm_struct *mm = task->mm;

        VMA_ITERATOR(vmi, mm, 0);
        for_each_vma(vmi, vma){
            for(unsigned long i = vma->vm_start; i < vma->vm_end; i += PAGE_SIZE){
                pgd = pgd_offset(mm, i);
                if(pgd_none(*pgd) || pgd_bad(*pgd)){
                    return;
                }
                p4d = p4d_offset(pgd, i);
                if(p4d_none(*p4d) || p4d_bad(*p4d)){
                    return;
                }
                pud = pud_offset(p4d, i);
                if(pud_none(*pud) || pud_bad(*pud)){
                    return;
                }
                pmd = pmd_offset(pud, i);
                if(pmd_none(*pmd) || pmd_bad(*pmd)){
                    return;
                }
                ptep = pte_offset_kernel(pmd, i);
                if(!ptep){
                    return;
                }
                pte = *ptep;
                if(!pte_none(pte)){
                    if(pte_present(pte)){
                        rss_counter++;
                        if(pte_young(pte)){
                            wss_counter++;
                            // test_and_clear_bit(_PAGE_BIT_ACCESSED, (unsigned long *) &ptep->pte);
                            test_and_clear_bit(_PAGE_BIT_ACCESSED, (unsigned long *)ptep);
                        }
                    }else{
                        swap_counter++;
                    }
                    
                }
            }
        }
    }
}

enum hrtimer_restart timer_callback( struct hrtimer *timer_for_restart )
{
	// Resetting the timer, which also means… ?
  	ktime_t currtime , interval;
  	currtime  = ktime_get();
  	interval = ktime_set(0,timer_interval_ns); 
  	hrtimer_forward(timer_for_restart, currtime , interval);

	// Do the measurement, like looking into VMA and walking through memory pages
// And also do the Kernel log printing aka printk per requirements
    rss_counter = 0;
    wss_counter = 0;
    swap_counter = 0;
    walk_pages();
    printk("PID [%i]; RSS=[%lu]0 KB, SWAP=[%lu] KB, WSS=[%lu] KB", pid, rss_counter*4, swap_counter*4, wss_counter*4);

	return HRTIMER_RESTART;
}

static int __init timer_init(void) {
	ktime_t ktime ;
    ktime = ktime_set( 0, timer_interval_ns );
	hrtimer_init( &hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	hr_timer.function = &timer_callback;
 	hrtimer_start( &hr_timer, ktime, HRTIMER_MODE_REL );
	return 0;
}

static void __exit timer_exit(void) {
	int ret;
  	ret = hrtimer_cancel( &hr_timer );
  	if (ret) printk("The timer was still in use...\n");
  	printk("HR Timer module uninstalling\n");
	
}

module_init(timer_init);
module_exit(timer_exit);
