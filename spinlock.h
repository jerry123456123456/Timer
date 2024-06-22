#ifndef SPINLOCK_H
#define SPINLOCK_H

typedef struct spinlock {
	int lock;
}spinlock_t;

void spinlock_init(spinlock_t *lock) {
	lock->lock = 0;
}

void spinlock_lock(spinlock_t *lock) {
	while (__sync_lock_test_and_set(&lock->lock, 1)) {}
}

int spinlock_trylock(spinlock_t *lock) {
	return __sync_lock_test_and_set(&lock->lock, 1) == 0;
}

void spinlock_unlock(spinlock_t *lock) {
	__sync_lock_release(&lock->lock);
}

void spinlock_destroy(spinlock_t *lock) {
	(void) lock;
}

#endif
