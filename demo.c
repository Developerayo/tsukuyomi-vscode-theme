#include "debug_counter.h"
#include "id.h"
#include "internal.h"
#include "internal/array.h"
#include "internal/compar.h"
#include "internal/enum.h"
#include "internal/gc.h"
#include "internal/hash.h"
#include "internal/numeric.h"
#include "internal/object.h"
#include "internal/proc.h"
#include "internal/rational.h"
#include "internal/vm.h"
#include "probes.h"
#include "ruby/encoding.h"
#include "ruby/st.h"
#include "ruby/util.h"
#include "transient_heap.h"
#include "builtin.h"

#if !ARRAY_DEBUG
# undef NDEBUG
# define NDEBUG
#endif
#include "ruby_assert.h"

VALUE rb_cArray;

/* for OPTIMIZED_CMP: */
#define id_cmp idCmp

#define ARY_DEFAULT_SIZE 16
#define ARY_MAX_SIZE (LONG_MAX / (int)sizeof(VALUE))
#define SMALL_ARRAY_LEN 16

RBIMPL_ATTR_MAYBE_UNUSED()
static int
should_be_T_ARRAY(VALUE ary)
{
    return RB_TYPE_P(ary, T_ARRAY);
}

RBIMPL_ATTR_MAYBE_UNUSED()
static int
should_not_be_shared_and_embedded(VALUE ary)
{
    return !FL_TEST((ary), ELTS_SHARED) || !FL_TEST((ary), RARRAY_EMBED_FLAG);
}

#define ARY_SHARED_P(ary) \
  (assert(should_be_T_ARRAY((VALUE)(ary))), \
   assert(should_not_be_shared_and_embedded((VALUE)ary)), \
   FL_TEST_RAW((ary),ELTS_SHARED)!=0)

#define ARY_EMBED_P(ary) \
  (assert(should_be_T_ARRAY((VALUE)(ary))), \
   assert(should_not_be_shared_and_embedded((VALUE)ary)), \
   FL_TEST_RAW((ary), RARRAY_EMBED_FLAG) != 0)

#define ARY_HEAP_PTR(a) (assert(!ARY_EMBED_P(a)), RARRAY(a)->as.heap.ptr)
#define ARY_HEAP_LEN(a) (assert(!ARY_EMBED_P(a)), RARRAY(a)->as.heap.len)
#define ARY_HEAP_CAPA(a) (assert(!ARY_EMBED_P(a)), assert(!ARY_SHARED_ROOT_P(a)), \
                          RARRAY(a)->as.heap.aux.capa)

#define ARY_EMBED_PTR(a) (assert(ARY_EMBED_P(a)), RARRAY(a)->as.ary)
#define ARY_EMBED_LEN(a) \
    (assert(ARY_EMBED_P(a)), \
     (long)((RBASIC(a)->flags >> RARRAY_EMBED_LEN_SHIFT) & \
	 (RARRAY_EMBED_LEN_MASK >> RARRAY_EMBED_LEN_SHIFT)))
#define ARY_HEAP_SIZE(a) (assert(!ARY_EMBED_P(a)), assert(ARY_OWNS_HEAP_P(a)), ARY_CAPA(a) * sizeof(VALUE))

#define ARY_OWNS_HEAP_P(a) (assert(should_be_T_ARRAY((VALUE)(a))), \
                            !FL_TEST_RAW((a), ELTS_SHARED|RARRAY_EMBED_FLAG))

#define FL_SET_EMBED(a) do { \
    assert(!ARY_SHARED_P(a)); \
    FL_SET((a), RARRAY_EMBED_FLAG); \
    RARY_TRANSIENT_UNSET(a); \
    ary_verify(a); \
} while (0)

#define FL_UNSET_EMBED(ary) FL_UNSET((ary), RARRAY_EMBED_FLAG|RARRAY_EMBED_LEN_MASK)
#define FL_SET_SHARED(ary) do { \
    assert(!ARY_EMBED_P(ary)); \
    FL_SET((ary), ELTS_SHARED); \
} while (0)
#define FL_UNSET_SHARED(ary) FL_UNSET((ary), ELTS_SHARED)

#define ARY_SET_PTR(ary, p) do { \
    assert(!ARY_EMBED_P(ary)); \
    assert(!OBJ_FROZEN(ary)); \
    RARRAY(ary)->as.heap.ptr = (p); \
} while (0)
#define ARY_SET_EMBED_LEN(ary, n) do { \
    long tmp_n = (n); \
    assert(ARY_EMBED_P(ary)); \
    assert(!OBJ_FROZEN(ary)); \
    RBASIC(ary)->flags &= ~RARRAY_EMBED_LEN_MASK; \
    RBASIC(ary)->flags |= (tmp_n) << RARRAY_EMBED_LEN_SHIFT; \
} while (0)
#define ARY_SET_HEAP_LEN(ary, n) do { \
    assert(!ARY_EMBED_P(ary)); \
    RARRAY(ary)->as.heap.len = (n); \
} while (0)
#define ARY_SET_LEN(ary, n) do { \
    if (ARY_EMBED_P(ary)) { \
        ARY_SET_EMBED_LEN((ary), (n)); \
    } \
    else { \
        ARY_SET_HEAP_LEN((ary), (n)); \
    } \
    assert(RARRAY_LEN(ary) == (n)); \
} while (0)
#define ARY_INCREASE_PTR(ary, n) do  { \
    assert(!ARY_EMBED_P(ary)); \
    assert(!OBJ_FROZEN(ary)); \
    RARRAY(ary)->as.heap.ptr += (n); \
} while (0)
#define ARY_INCREASE_LEN(ary, n) do  { \
    assert(!OBJ_FROZEN(ary)); \
    if (ARY_EMBED_P(ary)) { \
        ARY_SET_EMBED_LEN((ary), RARRAY_LEN(ary)+(n)); \
    } \
    else { \
        RARRAY(ary)->as.heap.len += (n); \
    } \
} while (0)

#define ARY_CAPA(ary) (ARY_EMBED_P(ary) ? RARRAY_EMBED_LEN_MAX : \
                       ARY_SHARED_ROOT_P(ary) ? RARRAY_LEN(ary) : ARY_HEAP_CAPA(ary))
#define ARY_SET_CAPA(ary, n) do { \
    assert(!ARY_EMBED_P(ary)); \
    assert(!ARY_SHARED_P(ary)); \
    assert(!OBJ_FROZEN(ary)); \
    RARRAY(ary)->as.heap.aux.capa = (n); \
} while (0)

#define ARY_SHARED_ROOT(ary) (assert(ARY_SHARED_P(ary)), RARRAY(ary)->as.heap.aux.shared_root)
#define ARY_SET_SHARED(ary, value) do { \
    const VALUE _ary_ = (ary); \
    const VALUE _value_ = (value); \
    assert(!ARY_EMBED_P(_ary_)); \
    assert(ARY_SHARED_P(_ary_)); \
    assert(ARY_SHARED_ROOT_P(_value_)); \
    RB_OBJ_WRITE(_ary_, &RARRAY(_ary_)->as.heap.aux.shared_root, _value_); \
} while (0)
#define RARRAY_SHARED_ROOT_FLAG FL_USER5
#define ARY_SHARED_ROOT_P(ary) (assert(should_be_T_ARRAY((VALUE)(ary))), \
                                FL_TEST_RAW((ary), RARRAY_SHARED_ROOT_FLAG))
#define ARY_SHARED_ROOT_REFCNT(ary) \
    (assert(ARY_SHARED_ROOT_P(ary)), RARRAY(ary)->as.heap.aux.capa)
#define ARY_SHARED_ROOT_OCCUPIED(ary) (ARY_SHARED_ROOT_REFCNT(ary) == 1)
#define ARY_SET_SHARED_ROOT_REFCNT(ary, value) do { \
    assert(ARY_SHARED_ROOT_P(ary)); \
    RARRAY(ary)->as.heap.aux.capa = (value); \
} while (0)
#define FL_SET_SHARED_ROOT(ary) do { \
    assert(!ARY_EMBED_P(ary)); \
    assert(!RARRAY_TRANSIENT_P(ary)); \
    FL_SET((ary), RARRAY_SHARED_ROOT_FLAG); \
} while (0)

static inline void
ARY_SET(VALUE a, long i, VALUE v)
{
    assert(!ARY_SHARED_P(a));
    assert(!OBJ_FROZEN(a));

    RARRAY_ASET(a, i, v);
}
#undef RARRAY_ASET


#if ARRAY_DEBUG
#define ary_verify(ary) ary_verify_(ary, __FILE__, __LINE__)

static VALUE
ary_verify_(VALUE ary, const char *file, int line)
{
    assert(RB_TYPE_P(ary, T_ARRAY));

    if (FL_TEST(ary, ELTS_SHARED)) {
        VALUE root = RARRAY(ary)->as.heap.aux.shared_root;
        const VALUE *ptr = ARY_HEAP_PTR(ary);
        const VALUE *root_ptr = RARRAY_CONST_PTR_TRANSIENT(root);
        long len = ARY_HEAP_LEN(ary), root_len = RARRAY_LEN(root);
        assert(FL_TEST(root, RARRAY_SHARED_ROOT_FLAG));
        assert(root_ptr <= ptr && ptr + len <= root_ptr + root_len);
        ary_verify(root);
    }
    else if (ARY_EMBED_P(ary)) {
        assert(!RARRAY_TRANSIENT_P(ary));
        assert(!ARY_SHARED_P(ary));
        assert(RARRAY_LEN(ary) <= RARRAY_EMBED_LEN_MAX);
    }
    else {
#if 1
        const VALUE *ptr = RARRAY_CONST_PTR_TRANSIENT(ary);
        long i, len = RARRAY_LEN(ary);
        volatile VALUE v;
        if (len > 1) len = 1; /* check only HEAD */
        for (i=0; i<len; i++) {
            v = ptr[i]; /* access check */
        }
        v = v;
#endif
    }

#if USE_TRANSIENT_HEAP
    if (RARRAY_TRANSIENT_P(ary)) {
        assert(rb_transient_heap_managed_ptr_p(RARRAY_CONST_PTR_TRANSIENT(ary)));
    }
#endif

    rb_transient_heap_verify();

    return ary;
}

void
rb_ary_verify(VALUE ary)
{
    ary_verify(ary);
}
#else
#define ary_verify(ary) ((void)0)
#endif

VALUE *
rb_ary_ptr_use_start(VALUE ary)
{
#if ARRAY_DEBUG
    FL_SET_RAW(ary, RARRAY_PTR_IN_USE_FLAG);
#endif
    return (VALUE *)RARRAY_CONST_PTR_TRANSIENT(ary);
}

void
rb_ary_ptr_use_end(VALUE ary)
{
#if ARRAY_DEBUG
    FL_UNSET_RAW(ary, RARRAY_PTR_IN_USE_FLAG);
#endif
}

void
rb_mem_clear(VALUE *mem, long size)
{
    while (size--) {
	*mem++ = Qnil;
    }
}

static void
ary_mem_clear(VALUE ary, long beg, long size)
{
    RARRAY_PTR_USE_TRANSIENT(ary, ptr, {
	rb_mem_clear(ptr + beg, size);
    });
}

static inline void
memfill(register VALUE *mem, register long size, register VALUE val)
{
    while (size--) {
	*mem++ = val;
    }
}

static void
ary_memfill(VALUE ary, long beg, long size, VALUE val)
{
    RARRAY_PTR_USE_TRANSIENT(ary, ptr, {
	memfill(ptr + beg, size, val);
	RB_OBJ_WRITTEN(ary, Qundef, val);
    });
}

static void
ary_memcpy0(VALUE ary, long beg, long argc, const VALUE *argv, VALUE buff_owner_ary)
{
    assert(!ARY_SHARED_P(buff_owner_ary));

    if (argc > (int)(128/sizeof(VALUE)) /* is magic number (cache line size) */) {
        rb_gc_writebarrier_remember(buff_owner_ary);
        RARRAY_PTR_USE_TRANSIENT(ary, ptr, {
            MEMCPY(ptr+beg, argv, VALUE, argc);
        });
    }
    else {
        int i;
        RARRAY_PTR_USE_TRANSIENT(ary, ptr, {
            for (i=0; i<argc; i++) {
                RB_OBJ_WRITE(buff_owner_ary, &ptr[i+beg], argv[i]);
            }
        });
    }
}

static void
ary_memcpy(VALUE ary, long beg, long argc, const VALUE *argv)
{
    ary_memcpy0(ary, beg, argc, argv, ary);
}

static VALUE *
ary_heap_alloc(VALUE ary, size_t capa)
{
    VALUE *ptr = rb_transient_heap_alloc(ary, sizeof(VALUE) * capa);

    if (ptr != NULL) {
        RARY_TRANSIENT_SET(ary);
    }
    else {
        RARY_TRANSIENT_UNSET(ary);
        ptr = ALLOC_N(VALUE, capa);
    }

    return ptr;
}

static void
ary_heap_free_ptr(VALUE ary, const VALUE *ptr, long size)
{
    if (RARRAY_TRANSIENT_P(ary)) {
        /* ignore it */
    }
    else {
        ruby_sized_xfree((void *)ptr, size);
    }
}

static void
ary_heap_free(VALUE ary)
{
    if (RARRAY_TRANSIENT_P(ary)) {
        RARY_TRANSIENT_UNSET(ary);
    }
    else {
        ary_heap_free_ptr(ary, ARY_HEAP_PTR(ary), ARY_HEAP_SIZE(ary));
    }
}

static void
ary_heap_realloc(VALUE ary, size_t new_capa)
{
    size_t old_capa = ARY_HEAP_CAPA(ary);

    if (RARRAY_TRANSIENT_P(ary)) {
        if (new_capa <= old_capa) {
            /* do nothing */
        }
        else {
            VALUE *new_ptr = rb_transient_heap_alloc(ary, sizeof(VALUE) * new_capa);

            if (new_ptr == NULL) {
                new_ptr = ALLOC_N(VALUE, new_capa);
                RARY_TRANSIENT_UNSET(ary);
            }

            MEMCPY(new_ptr, ARY_HEAP_PTR(ary), VALUE, old_capa);
            ARY_SET_PTR(ary, new_ptr);
        }
    }
    else {
        SIZED_REALLOC_N(RARRAY(ary)->as.heap.ptr, VALUE, new_capa, old_capa);
    }
    ary_verify(ary);
}

#if USE_TRANSIENT_HEAP
static inline void
rb_ary_transient_heap_evacuate_(VALUE ary, int transient, int promote)
{
    if (transient) {
        VALUE *new_ptr;
        const VALUE *old_ptr = ARY_HEAP_PTR(ary);
        long capa = ARY_HEAP_CAPA(ary);
        long len  = ARY_HEAP_LEN(ary);

        if (ARY_SHARED_ROOT_P(ary)) {
            capa = len;
        }

        assert(ARY_OWNS_HEAP_P(ary));
        assert(RARRAY_TRANSIENT_P(ary));
        assert(!ARY_PTR_USING_P(ary));

        if (promote) {
            new_ptr = ALLOC_N(VALUE, capa);
            RARY_TRANSIENT_UNSET(ary);
        }
        else {
            new_ptr = ary_heap_alloc(ary, capa);
        }

        MEMCPY(new_ptr, old_ptr, VALUE, capa);
        /* do not use ARY_SET_PTR() because they assert !frozen */
        RARRAY(ary)->as.heap.ptr = new_ptr;
    }

    ary_verify(ary);
}

void
rb_ary_transient_heap_evacuate(VALUE ary, int promote)
{
    rb_ary_transient_heap_evacuate_(ary, RARRAY_TRANSIENT_P(ary), promote);
}

void
rb_ary_detransient(VALUE ary)
{
    assert(RARRAY_TRANSIENT_P(ary));
    rb_ary_transient_heap_evacuate_(ary, TRUE, TRUE);
}
#else
void
rb_ary_detransient(VALUE ary)
{
    /* do nothing */
}
#endif