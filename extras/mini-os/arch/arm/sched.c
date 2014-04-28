#include <mini-os/sched.h>
#include <mini-os/xmalloc.h>

void arm_start_thread(void);

/* Architecture specific setup of thread creation */
struct thread* arch_create_thread(char *name, void (*function)(void *),
                                  void *data)
{
    struct thread *thread;

    thread = xmalloc(struct thread);
    /* We can't use lazy allocation here since the trap handler runs on the stack */
    thread->stack = (char *)alloc_pages(STACK_SIZE_PAGE_ORDER);
    thread->name = name;
    printk("Thread \"%s\": pointer: 0x%lx, stack: 0x%lx\n", name, thread,
            thread->stack);

    /* Save pointer to the thread on the stack, used by current macro */
    *((unsigned long *)thread->stack) = (unsigned long)thread;

    /* Push the details to pass to arm_start_thread onto the stack */
    int *sp = (int *) (thread->stack + STACK_SIZE);
    *(--sp) = (int) function;
    *(--sp) = (int) data;
    thread->sp = (unsigned long) sp;

    thread->ip = (unsigned long) arm_start_thread;

    return thread;
}

void run_idle_thread(void)
{
    __asm__ __volatile__ ("mov sp, %0; mov pc, %1"::"r"(idle_thread->sp), "r"(idle_thread->ip));
    /* Never arrive here! */
}
