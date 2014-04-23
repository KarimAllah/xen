#include <mini-os/sched.h>
#include <mini-os/xmalloc.h>


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

    thread->sp = (unsigned long)thread->stack + STACK_SIZE;
    /* Save pointer to the thread on the stack, used by current macro */
    *((unsigned long *)thread->stack) = (unsigned long)thread;

    thread->ip = (unsigned long) function;
    /* FIXME thread->r0 = (unsigned long)data; */

    return thread;
}

void run_idle_thread(void)
{
    __asm__ __volatile__ ("mov sp, %0; mov pc, %1"::"r"(idle_thread->sp), "r"(idle_thread->ip));
    /* Never arrive here! */
}
