/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "internal.h"

#include <pthread.h>
#include <assert.h>
#include <errno.h>


int uv_mutex_init(uv_mutex_t* mutex) {
  if (pthread_mutex_init(mutex, NULL))
    return -1;
  else
    return 0;
}


void uv_mutex_destroy(uv_mutex_t* mutex) {
  pthread_mutex_destroy(mutex);
}


void uv_mutex_lock(uv_mutex_t* mutex) {
  pthread_mutex_lock(mutex);
}


int uv_mutex_trylock(uv_mutex_t* mutex) {
  int r;

  r = pthread_mutex_trylock(mutex);
  assert(r == 0 || r == EAGAIN);

  if (r)
    return -1;
  else
    return 0;
}


void uv_mutex_unlock(uv_mutex_t* mutex) {
  pthread_mutex_unlock(mutex);
}


int uv_rwlock_init(uv_rwlock_t* rwlock) {
  if (pthread_rwlock_init(rwlock, NULL))
    return -1;
  else
    return 0;
}


void uv_rwlock_destroy(uv_rwlock_t* rwlock) {
  pthread_rwlock_destroy(rwlock);
}


void uv_rwlock_rdlock(uv_rwlock_t* rwlock) {
  pthread_rwlock_rdlock(rwlock);
}


int uv_rwlock_tryrdlock(uv_rwlock_t* rwlock) {
  int r;

  r = pthread_rwlock_tryrdlock(rwlock);
  assert(r == 0 || r == EAGAIN);

  if (r)
    return -1;
  else
    return 0;
}


void uv_rwlock_rdunlock(uv_rwlock_t* rwlock) {
  pthread_rwlock_unlock(rwlock);
}


void uv_rwlock_wrlock(uv_rwlock_t* rwlock) {
  pthread_rwlock_wrlock(rwlock);
}


int uv_rwlock_trywrlock(uv_rwlock_t* rwlock) {
  int r;

  r = pthread_rwlock_trywrlock(rwlock);
  assert(r == 0 || r == EAGAIN);

  if (r)
    return -1;
  else
    return 0;
}


void uv_rwlock_wrunlock(uv_rwlock_t* rwlock) {
  pthread_rwlock_unlock(rwlock);
}
