#ifndef _LIB_DYNLIST_H
#define _LIB_DYNLIST_H

#include "macros.h"
#include "math.h"
#include "misc.h"
#include "types.h"

/*
usage:
#include "c-lib/dynlist.h"

int cmp(const void* a, const void* b) {
  return *(const int*)a - *(const int*)b;
}

int main() {
  int* letters = NULL; dynlist_init(letters);
  DYNLIST(int) numbers = dynlist_create(int);

  *dynlist_append(numbers) = 10;
  *dynlist_push(numbers) = 20; // same as append
  *dynlist_prepend(numbers) = 5;

  *dynlist_insert(numbers, 3) = 7; // insert at end

  // output: 5 10 20 7
  dynlist_each(numbers, num) {
    printf("%d ", *num);
  }

  int removed = dynlist_remove(numbers, 1);
  printf("\nremoved: %d then sorted\n", removed); // 10

  dynlist_sort(numbers, cmp);

  // output: 5 7 20
  dynlist_each(numbers, num) {
    printf("%d ", *num);
  }
  printf("\n");

  dynlist_destroy(numbers);
  return 0;
}
*/

#define DYNLIST_MIN_CAP 4
#define DYNLIST(_T) typeof(_T*)

typedef struct {
  size_t size;
  size_t capacity;
  size_t t_size;
} dynlist_header_t;

// get header from list pointer
M_INLINE dynlist_header_t* dynlist_header(const void* l) {
  ASSERT(l, "dynlist is NULL");
  return (dynlist_header_t*)((u8*)l - sizeof(dynlist_header_t));
}

// initialize a list with type and capacity
#define _dynlist_init2(_d, _cap) _dynlist_init_impl((void**)&(_d), sizeof(*(_d)), (_cap));
#define _dynlist_init1(_d) _dynlist_init2(_d, DYNLIST_MIN_CAP)

// create a new list with type and capacity
#define _dynlist_create2(_T, _cap) ({ \
    DYNLIST(_T) _list = NULL; \
    _dynlist_init_impl((void**)&_list, sizeof(_T), (_cap)); \
    _list; \
})
#define _dynlist_create1(_T) _dynlist_create2(_T, DYNLIST_MIN_CAP)

// init dynlist (list, <cap?>)
#define dynlist_init(...) DISPATCH_N(_dynlist_init, __VA_ARGS__)
// create dynlist (_T, <cap?>)
#define dynlist_create(...) DISPATCH_N(_dynlist_create, __VA_ARGS__)

// destroy a list and free memory
#define dynlist_destroy(_d) ({ \
        typeof(_d) *_pd = &(_d); \
        _dynlist_destroy_impl((void**) _pd); \
        *_pd = NULL; \
    })

// get current list size
M_INLINE size_t dynlist_size(const void* l) {
  return l ? dynlist_header(l)->size : 0;
}

// get current list capacity
M_INLINE size_t dynlist_capacity(const void* l) {
  return l ? dynlist_header(l)->capacity : 0;
}

// ensure list has at least specified capacity
#define dynlist_ensure(_d, _n) \
  _dynlist_realloc_impl((void**)&(_d), (_n), true)

// append, returns pointer to new space
#define dynlist_push dynlist_append
#define dynlist_append(_d) \
    (typeof(&(_d)[0]))_dynlist_insert_impl((void**)&(_d), dynlist_size(_d))

// prepend, returns pointer to new space
#define dynlist_prepend(_d) \
    (typeof(&(_d)[0]))_dynlist_insert_impl((void**)&(_d), 0)

// inserts, returns pointer to new space
#define dynlist_insert(_d, _i) \
    (typeof(&(_d)[0]))_dynlist_insert_impl((void**)&(_d), (_i))

// remove element at index and return it
#define dynlist_remove(_d, _i) ({ \
    typeof(_d)* _pd = &(_d); \
    typeof(_i) _idx = (_i); \
    typeof((_d)[0]) _tmp = (*_pd)[_idx]; \
    _dynlist_remove_impl((void**)_pd, _idx, true); \
    _tmp; \
})
#define dynlist_remove_no_realloc(_d, _i) ({ \
    typeof(_d)* _pd = &(_d); \
    typeof(_i) _idx = (_i); \
    typeof((_d)[0]) _tmp = (*_pd)[_idx]; \
    _dynlist_remove_impl((void**)_pd, _idx, false); \
    _tmp; \
})

// remove last element and return it
#define dynlist_pop(_d) dynlist_remove(_d, dynlist_size(_d) - 1)

// resize list to contain exactly n elements
#define dynlist_resize(_d, _n) _dynlist_resize_impl((void**)&(_d), (_n), true)
#define dynlist_resize_no_contract(_d, _n) _dynlist_resize_impl((void**)&(_d), (_n), false)

// clear all elements in list
#define dynlist_clear(_d) dynlist_resize((_d), 0)

// create a copy of the list
#define dynlist_copy(_d) ((typeof(_d))_dynlist_copy_impl((void**)&(_d)))

// append all of one dynlist to another
#define dynlist_push_all(_d, _e) ({ \
        typeof(_d) *_pd = &(_d); \
        typeof(_e) *_pe = &(_e); \
        ASSERT(dynlist_header(*_pd)->t_size == dynlist_header(*_pe)->t_size); \
        const size_t _offset = dynlist_size(*_pd), _n = dynlist_size(*_pe); \
        dynlist_resize(*_pd, _offset + dynlist_size(*_pe)); \
        for (size_t _i = 0; _i < _n; _i++) { (*_pd)[_offset + _i] = (*_pe)[_i]; } \
    })

// iteration macro
#define dynlist_each(_d, _it) \
  for (size_t _i = 0, _size = dynlist_size(_d); _i < _size; _i++) \
      for (typeof(&(_d)[0]) _it = &(_d)[_i]; _it; _it = NULL)

// remove current element during iteration
#define dynlist_remove_it(_d, _it, _i) do { \
    dynlist_remove(_d, _i); \
    (_i)--; \
    _size--; \
} while (0)

// comparison function type for sorting
typedef size_t (*dynlist_cmp_func)(const void*, const void*);

// sort list using comparison function
#define dynlist_sort(_d, _cmp) _dynlist_sort_impl((void**)&(_d), (_cmp))

// insert element in sorted position
#define dynlist_insert_sorted(_d, _cmp, _val) ({ \
    typeof(_val) _v = (_val); \
    (typeof(&(_d)[0]))_dynlist_insert_sorted_impl((void**)&(_d), (_cmp), &_v); \
})

// ============================================
// implementations, not intended for direct use

static M_UNUSED void _dynlist_init_impl(void** plist, size_t t_size,
                                        size_t init_cap) {
  ASSERT(!*plist);

  init_cap = max(init_cap, DYNLIST_MIN_CAP);

  dynlist_header_t* h =
      (dynlist_header_t*)malloc(sizeof(dynlist_header_t) + t_size * init_cap);
  ASSERT(h);

  *h = (dynlist_header_t){.size = 0, .capacity = init_cap, .t_size = t_size};
  *plist = h + 1;
}

static M_UNUSED void _dynlist_destroy_impl(void** plist) {
  if (*plist) {
    dynlist_header_t* h = dynlist_header(*plist);
    free(h);
    *plist = NULL;
  }
}

static M_UNUSED void _dynlist_realloc_impl(void** plist, size_t new_cap,
                                           bool allow_contract) {
  ASSERT(new_cap >= 0);
  ASSERT(*plist);

  dynlist_header_t* h = dynlist_header(*plist);

  if (new_cap <= h->size) new_cap = h->size;
  if (new_cap < DYNLIST_MIN_CAP) new_cap = DYNLIST_MIN_CAP;
  if (new_cap == h->capacity) return;

  // new capacity, power of 2
  size_t capacity = DYNLIST_MIN_CAP;
  while (capacity < new_cap) {
    capacity *= 2;
  }

  if (allow_contract) {
    while (capacity > DYNLIST_MIN_CAP && capacity / 2 >= new_cap) {
      capacity /= 2;
    }
  }

  dynlist_header_t* new_h = (dynlist_header_t*)malloc(sizeof(dynlist_header_t) +
                                                      h->t_size * capacity);
  ASSERT(new_h);

  *new_h = (dynlist_header_t){
      .size = h->size, .capacity = capacity, .t_size = h->t_size};

  void* new_data = new_h + 1;
  memcpy(new_data, *plist, h->t_size * h->size);
  free(h);
  *plist = new_data;
}

static M_UNUSED void* _dynlist_insert_impl(void** plist, size_t index) {
  ASSERT(plist);
  ASSERT(*plist);

  dynlist_header_t* h = dynlist_header(*plist);
  ASSERT(index >= 0 && index <= h->size);

  // ensure capacity
  if (h->size >= h->capacity) {
    _dynlist_realloc_impl(plist, h->size + 1, false);
    h = dynlist_header(*plist);
  }

  u8* data = (u8*)(*plist);
  if (index < h->size) {
    memmove(data + (index + 1) * h->t_size, data + index * h->t_size,
            (h->size - index) * h->t_size);
  }

  h->size++;
  return data + index * h->t_size;
}

static M_UNUSED void _dynlist_remove_impl(void** plist, size_t index,
                                          bool allow_contract) {
  ASSERT(plist);
  ASSERT(*plist);

  dynlist_header_t* h = dynlist_header(*plist);

  ASSERT(h->size != 0);
  ASSERT(index < h->size);

  u8* data = (u8*)h + 1;
  memmove(data + (index * h->t_size), data + ((index + 1) * h->t_size),
          (h->size - index - 1) * h->t_size);

  _dynlist_realloc_impl(plist, h->size - 1, allow_contract);

  dynlist_header(*plist)->size--;
}

static M_UNUSED void _dynlist_resize_impl(void** plist, size_t n,
                                          bool allow_contract) {
  ASSERT(plist);
  ASSERT(*plist);

  if (dynlist_header(*plist)->capacity < n) {
    _dynlist_realloc_impl(plist, max(n, DYNLIST_MIN_CAP), allow_contract);
  }

  dynlist_header(*plist)->size = n;
}

static M_UNUSED void* _dynlist_copy_impl(void** plist) {
  ASSERT(plist);
  ASSERT(*plist);

  dynlist_header_t* h = dynlist_header(*plist);

  DYNLIST(void) new_list = NULL;
  _dynlist_init_impl(&new_list, h->t_size, h->size);

  // update the used size in the new list
  dynlist_header_t* new_h = dynlist_header(new_list);
  new_h->size = h->size;

  // copy data
  memcpy(new_list, *plist, h->t_size * h->size);
  return new_list;
}

static M_UNUSED void _dynlist_sort_impl(void** plist, dynlist_cmp_func cmp) {
  if (!*plist) return;
  dynlist_header_t* h = dynlist_header(*plist);

  // bubble sort for small lists
  for (size_t i = 0; i < h->size - 1; i++) {
    for (size_t j = 0; j < h->size - i - 1; j++) {
      u8* a = (u8*)(*plist) + j * h->t_size;
      u8* b = a + h->t_size;
      if (cmp(a, b) > 0) {
        u8 tmp[h->t_size];
        memcpy(tmp, a, h->t_size);
        memcpy(a, b, h->t_size);
        memcpy(b, tmp, h->t_size);
      }
    }
  }
}

static M_UNUSED void* _dynlist_insert_sorted_impl(void** plist,
                                                  dynlist_cmp_func cmp,
                                                  const void* val) {
  if (!*plist) return NULL;

  dynlist_header_t* h = dynlist_header(*plist);
  int low = 0, high = h->size;
  u8* data = (u8*)(*plist);

  // binary search for insertion point
  while (low < high) {
    int mid = low + (high - low) / 2;
    if (cmp(data + mid * h->t_size, val) < 0) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }

  void* new_item = _dynlist_insert_impl(plist, low);
  memcpy(new_item, val, h->t_size);
  return new_item;
}

#endif
