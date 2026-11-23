/* nommu-cxx.c - linked into every C++ program for this board (board.mk
 * does it).  Static uClibc-ng + LinuxThreads quirks for libstdc++ threads:
 *
 * 1. libstdc++ decides whether threads exist by checking that the weak
 *    symbol __pthread_key_create resolves (gthr-posix.h).  LinuxThreads only
 *    has pthread_key_create, so std::thread threw "Enable multithreading to
 *    use std::thread".  Provide the alias.
 * 2. libstdc++ calls pthreads through weak references, and a weak reference
 *    does not pull a member out of a static libc.a: pthread_create stayed 0
 *    and std::thread jumped to address 0.  Reference them all strongly.
 */
#include <pthread.h>
#include <sched.h>

int __pthread_key_create(pthread_key_t *k, void (*d)(void *))
{
	return pthread_key_create(k, d);
}

__attribute__((used)) static void *const nommu_cxx_pthread_refs[] = {
	(void *)pthread_attr_destroy,
	(void *)pthread_attr_init,
	(void *)pthread_attr_setdetachstate,
	(void *)pthread_cancel,
	(void *)pthread_cond_broadcast,
	(void *)pthread_cond_destroy,
	(void *)pthread_cond_init,
	(void *)pthread_cond_signal,
	(void *)pthread_cond_timedwait,
	(void *)pthread_cond_wait,
	(void *)pthread_create,
	(void *)pthread_detach,
	(void *)pthread_equal,
	(void *)pthread_exit,
	(void *)pthread_getschedparam,
	(void *)pthread_getspecific,
	(void *)pthread_join,
	(void *)pthread_key_create,
	(void *)pthread_key_delete,
	(void *)pthread_mutexattr_destroy,
	(void *)pthread_mutexattr_init,
	(void *)pthread_mutexattr_settype,
	(void *)pthread_mutex_destroy,
	(void *)pthread_mutex_init,
	(void *)pthread_mutex_lock,
	(void *)pthread_mutex_timedlock,
	(void *)pthread_mutex_trylock,
	(void *)pthread_mutex_unlock,
	(void *)pthread_once,
	(void *)pthread_self,
	(void *)pthread_setschedparam,
	(void *)pthread_setspecific,
	(void *)sched_get_priority_max,
	(void *)sched_get_priority_min,
	(void *)sched_yield,
};
