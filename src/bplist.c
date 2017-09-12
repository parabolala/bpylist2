#include <Python.h>
#include <pytime.h>
#include <stdint.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

    #define SwapBigToHost16(x) ((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8)
    #define SwapBigToHost32(x) __builtin_bswap32(x)
    #define SwapBigToHost64(x) __builtin_bswap64(x)

    #define SwapHostToBig16(x) SwapBigToHost16(x)
    #define SwapHostToBig32(x) SwapBigToHost32(x)
    #define SwapHostToBig64(x) SwapBigToHost64(x)

#else

    #define SwapBigToHost16(x) x
    #define SwapBigToHost32(x) x
    #define SwapBigToHost64(x) x

    #define SwapHostToBig16(x) x
    #define SwapHostToBig32(x) x
    #define SwapHostToBig64(x) x

#endif

static const char* const bplist_header = "bplist00";
static const size_t bplist_header_length = sizeof("bplist00") - 1;


/**
 * The definition of this struct comes from the plist spec.
 * See https://opensource.apple.com/source/CF/CF-744.18/CFBinaryPList.c
 */
typedef struct _bplist_trailer {
    /* padding must remain in order to keep struct members aligned */
    uint8_t padding[6];
    uint8_t offset_size;
    uint8_t ref_size;
    uint64_t num_objects;
    uint64_t top_object;
    uint64_t offset_table_offset;
} bplist_trailer;

/**
 * Parsing requires passing around several constant values. In order
 * to avoid data clump, we pass around this struct instead.
 */
typedef struct _bplist_parse_state {
    /* first address that is valid for objects or objects offsets */
    const uint8_t* data;
    /* first address past end of valid memory region for objects or
     * object offsets
     */
    const uint8_t* data_end;
    /* width, in bytes, of object references (indexes into offset table) */
    size_t ref_size;
    /* width, in bytes, of data offsets (data in offset table) */
    size_t offset_size;
    /* total number of objects referenced in the offset table */
    size_t object_count;
    /* the offset_table has object_count integers;
     * the integers are big endian encoded, with a width
     * defined by offset_size
     */
    const uint8_t* offset_table;
} bplist_parse_state;

typedef struct _bplist_generate_state {
    uint32_t* offsets;
    uint32_t* offsets_end;
    uint32_t* current_offset;
    uint8_t* objects;
    uint8_t* objects_end;
    uint8_t* current_object;
    bplist_trailer trailer;
    PyObject* plist;
} bplist_generate_state;

enum plist_type {
    plist_type_primitive       = 0x00,
        plist_type_false       = 0x08,
        plist_type_true        = 0x09,
    plist_type_int             = 0x10,
        plist_type_int8        = 0x10,
        plist_type_int16       = 0x11,
        plist_type_int32       = 0x12,
        plist_type_int64       = 0x13,
    plist_type_float           = 0x20,
        plist_type_float32     = 0x22,
        plist_type_float64     = 0x23,
    plist_type_date            = 0x30,
    plist_type_data            = 0x40,
    plist_type_ascii_string    = 0x50,
    plist_type_utf16_string    = 0x60,
    plist_type_uid             = 0x80,
    plist_type_array           = 0xA0,
    plist_type_dict            = 0xD0,
};


/**
 * Cached reference to the bpylist.archiver.timestamp class; used by
 * parse_date.
 */
static PyObject* ts_class = NULL;

/**
 * Cached reference to the bpylist.archiver.uid class; used by
 * parse_uid.
 */
static PyObject* uid_class = NULL;

static int64_t
unpack_int(const uint8_t int_width, const uint8_t* const int_ptr)
{
    switch (int_width) {
    default:
    case plist_type_int64: return (int64_t)SwapBigToHost64(*(uint64_t*)int_ptr);
    case plist_type_int32: return (int64_t)SwapBigToHost32(*(uint32_t*)int_ptr);
    case plist_type_int16: return (int64_t)SwapBigToHost16(*(uint16_t*)int_ptr);
    case plist_type_int8:  return (int64_t)*int_ptr;
    }
}

static uint64_t
unpack_uint(const size_t int_width, const uint8_t* const int_ptr)
{
    switch (int_width) {
    default:
    case 8:  return (uint64_t)SwapBigToHost64(*(uint64_t*)int_ptr);
    case 4:  return (uint64_t)SwapBigToHost32(*(uint32_t*)int_ptr);
    case 2:  return (uint64_t)SwapBigToHost16(*(uint16_t*)int_ptr);
    case 1:  return (uint64_t)*int_ptr;
    }
}

static PyObject*
parse_int(const uint8_t type, const uint8_t* const object)
{
    const int64_t raw_int = unpack_int(type, object);
    return PyLong_FromSsize_t(raw_int);
}

static PyObject*
parse_uid(const uint8_t type, const uint8_t* const object)
{
    /* according to spec, UIDs have an arbitrary length, but in practice they
     * will always be 4 bytes (32 bits)
     */
    const size_t width = (type & 0xF) + 1;
    const uint64_t raw_int = unpack_uint(width, object);

    PyObject* const py_int = PyLong_FromSize_t(raw_int);

    if (unlikely(py_int == NULL))
        return NULL;

    PyObject* const int_tuple = PyTuple_New(1);
    if (unlikely(int_tuple == NULL)) {
        Py_DECREF(py_int);
        return NULL;
    }

    PyTuple_SET_ITEM(int_tuple, 0, py_int);

    PyObject* const py_uid = PyObject_CallObject(uid_class, int_tuple);
    Py_DECREF(int_tuple);

    return py_uid;
}

static void
pack_int(bplist_generate_state* const state, const int64_t int_val)
{
    const union {
        int64_t what_we_have;
        uint64_t what_we_want;
    } magic_box = {
        .what_we_have = int_val
    };

    const uint64_t swappy = magic_box.what_we_want;

    /**
     * Note: this calculation is to determine the index of least significant
     *       bit set to 0; this translates into the question "how many bits
     *       do I (not) need?", and the switch statement answers the question.
     *       the builtin intrinsic _should_ compile to a single instruction, and
     *       the switch _should_ compile to a jump table, so this _should_ end
     *       up being constant time an only a handful of instructions
     *
     * Note: since negative numbers set the most significant bit, they will
     *       always be encoded as 8 byte integers; this behaviour is consistent
     *       with Cocoa's plist generator and doesn't break plistlib's
     *       expectations about signedness
     */
    const int int_width = swappy == 0 ? 64 : __builtin_clzll(swappy);
    switch (int_width) {
    case  0: case  1: case  2: case  3: case  4: case  5:
    case  6: case  7: case  8: case  9: case 10: case 11:
    case 12: case 13: case 14: case 15: case 16: case 17:
    case 18: case 19: case 20: case 21: case 22: case 23:
    case 24: case 25: case 26: case 27: case 28: case 29:
    case 30: case 31:
        *state->current_object++ = plist_type_int64;
        *(uint64_t*)state->current_object = SwapHostToBig64(swappy);
        state->current_object += sizeof(uint64_t);
        return;
    case 32: case 33: case 34: case 35: case 36: case 37:
    case 38: case 39: case 40: case 41: case 42: case 43:
    case 44: case 45: case 46: case 47:
        *state->current_object++ = plist_type_int32;
        *(uint32_t*)state->current_object = SwapHostToBig32(swappy);
        state->current_object += sizeof(uint32_t);
        return;
    case 48: case 49: case 50: case 51: case 52: case 53:
    case 54: case 55:
        *state->current_object++ = plist_type_int16;
        *(uint16_t*)state->current_object = SwapHostToBig16(swappy);
        state->current_object += sizeof(uint16_t);
        return;
    case 56: case 57: case 58: case 59: case 60: case 61:
    case 62: case 63: case 64: default:
        *state->current_object++ = plist_type_int8;
        *state->current_object++ = (uint8_t)swappy;
        return;
    }
}

static void
pack_uint(const size_t int_width,
          uint8_t* const current_object,
          const uint64_t int_val)
{
    switch (int_width) {
    case 1: *current_object = (uint8_t)int_val; return;
    case 2: *(uint16_t*)current_object = SwapBigToHost16(int_val); return;
    case 4: *(uint32_t*)current_object = SwapBigToHost32(int_val); return;
    case 8: *(uint64_t*)current_object = SwapBigToHost64(int_val); return;
    default:
        abort(); /* this is unreachable */
    }
}

static int
generate_int(bplist_generate_state* const state, PyObject* const py_obj)
{
    pack_int(state, PyLong_AsSsize_t(py_obj));
    return 0;
}

static int
generate_uid(bplist_generate_state* const state, PyObject* const py_obj)
{
    const size_t uid_val = PyLong_AsSize_t(py_obj);


    const int int_width = __builtin_clzll(uid_val);
    switch (int_width) {
    case  0: case  1: case  2: case  3: case  4: case  5: case  6:
    case  7: case  8: case  9: case 10: case 11: case 12: case 13:
    case 14: case 15: case 16: case 17: case 18: case 19: case 20:
    case 21: case 22: case 23: case 24: case 25: case 26: case 27:
    case 28: case 29: case 30: case 31:
        *state->current_object++ = (plist_type_uid | 0x7);
        *(uint64_t*)state->current_object = SwapHostToBig64(uid_val);
        state->current_object += sizeof(uint64_t);
        return 0;
    case 32: case 33: case 34: case 35: case 36: case 37: case 38:
    case 39: case 40: case 41: case 42: case 43: case 44: case 45:
    case 46: case 47:
        *state->current_object++ = (plist_type_uid | 0x3);
        *(uint32_t*)state->current_object = SwapHostToBig32(uid_val);
        state->current_object += sizeof(uint32_t);
        return 0;
    case 48: case 49: case 50: case 51: case 52: case 53: case 54:
    case 55:
        *state->current_object++ = (plist_type_uid | 0x1);
        *(uint16_t*)state->current_object = SwapHostToBig16(uid_val);
        state->current_object += sizeof(uint16_t);
        return 0;
    case 56: case 57: case 58: case 59: case 60: case 61: case 62:
    case 63: case 64: default:
        *state->current_object++ = (plist_type_uid | 0x0);
        *state->current_object++ = (uint8_t)uid_val;
        return 0;
    }
}

static PyObject*
parse_float32(const uint8_t* const object)
{
    const union {
        uint32_t what_we_have;
        float what_we_want;
    } magic_box = {
        .what_we_have = SwapBigToHost32(*(uint32_t*)object)
    };

    return PyFloat_FromDouble((double)magic_box.what_we_want);
}

static PyObject*
parse_float64(const uint8_t* const object)
{
    const union {
        uint64_t what_we_have;
        double what_we_want;
    } magic_box = {
        .what_we_have = SwapBigToHost64(*(uint64_t*)object)
    };

    return PyFloat_FromDouble(magic_box.what_we_want);
}

static int
generate_float(bplist_generate_state* const state, PyObject* const py_obj)
{
    *state->current_object++ = plist_type_float64;

    const union {
        double what_we_have;
        uint64_t what_we_want;
    } magic_box = {
        .what_we_have = PyFloat_AsDouble(py_obj)
    };

    *((uint64_t*)state->current_object) =
        SwapBigToHost64(magic_box.what_we_want);

    state->current_object += sizeof(uint64_t);

    return 0;
}

/**
 * The difference, in seconds, between the Cocoa NSDate epoch (Jan. 1st, 2001),
 * and the UNIX timestamp epoch (Jan. 1st, 1970).
 *
 * This value comes directly from Apple's NSDate.h NSTimeIntervalSince1970.
 */
static const double unix2apple_epoch_delta = 978307200.0;

static PyObject*
parse_date(const uint8_t* const object)
{
    const union {
        uint64_t what_we_have;
        double what_we_want;
    } magic_box = {
        .what_we_have = SwapBigToHost64(*(uint64_t*)object)
    };

    const double float_date = unix2apple_epoch_delta + magic_box.what_we_want;

    PyObject* const py_float_date = PyFloat_FromDouble(float_date);

    if (unlikely(py_float_date == NULL))
        return NULL;

    PyObject* const date_tuple = PyTuple_New(1);
    if (unlikely(date_tuple == NULL)) {
        Py_DECREF(py_float_date);
        return NULL;
    }

    PyTuple_SET_ITEM(date_tuple, 0, py_float_date);

    PyObject* const py_stamp = PyObject_CallObject(ts_class, date_tuple);
    Py_DECREF(date_tuple);

    return py_stamp;
}

static int
generate_date(bplist_generate_state* const state, PyObject* const py_obj)
{
    *state->current_object++ = plist_type_date;

    const union {
        double what_we_have;
        uint64_t what_we_want;
    } magic_box = {
        .what_we_have = PyFloat_AsDouble(py_obj)
    };

    *((uint64_t*)state->current_object) =
        SwapBigToHost64(magic_box.what_we_want - unix2apple_epoch_delta);

    state->current_object += sizeof(uint64_t);

    return 0;
}

static ssize_t
unpack_length(const uint8_t type, const uint8_t** const object)
{
    ssize_t length = (ssize_t)(type & 0x0F);

    if (length == 0x0F) {
        const uint8_t int_type  = *(*object)++;
        const size_t  int_width = 1 << (int_type & 0x0F);
        length = unpack_int(int_type, *object);
        *object += int_width;
    }

    return length;
}

static PyObject*
parse_data(const uint8_t type,
           const uint8_t* object,
           const uint8_t* const object_end)
{
    const ssize_t length = unpack_length(type, &object);
    if (likely(object + length < object_end))
        return PyByteArray_FromStringAndSize((char*)object, length);

    PyErr_Format(PyExc_RuntimeError,
                 "bplist parsing hit an invalid data length: %zd",
                 length);
    return NULL;
}

static void
pack_type_and_length(bplist_generate_state* const state,
                     const uint8_t type,
                     const ssize_t length)
{
    if (length < 0x0F) {
        *state->current_object++ = (type | length);
        return;
    }

    *state->current_object++ = (type | 0x0F);
    pack_int(state, length);
    return;
}

static int
resize_plist_buffer(bplist_generate_state* const state,
                    const size_t new_length)
{
    /* need to save offset state first, since we use pointers
     * for everything instead of offsets, potentially changing
     * the source pointer would corrupt the buffer
     */
    const size_t object_offset = state->current_object - state->objects;

    const int did_resize = _PyBytes_Resize(&state->plist, new_length);

    if (unlikely(did_resize == -1))
        return -1;

    ssize_t signed_buffer_length;

    const int did_extract =
        PyBytes_AsStringAndSize(state->plist,
                                (char**)&state->objects,
                                &signed_buffer_length);

    if (unlikely(did_extract == -1))
        return -1;

    state->objects_end = state->objects + new_length;
    state->current_object = state->objects + object_offset;

    return 0;
}

static int
resize_plist_if_needed(bplist_generate_state* const state,
                       const size_t required_space)
{
    const size_t remaining_space =
        state->objects_end - state->current_object;

    if (remaining_space < required_space) {
        /* follow simple exponential growth curve for buffers;
         * is it the best choice? I don't know...measure it!
         */
        const size_t new_length =
            (state->objects_end - state->objects) * 2;

        return resize_plist_buffer(state, new_length);
    }

    return 0;
}

static int
grow_offset_table_buffer_if_needed(bplist_generate_state* const state)
{
    const size_t remaining_space =
        state->offsets_end - state->current_offset;

    if (remaining_space >= state->trailer.offset_size)
        return 0;

    const size_t offset_offset = state->current_offset - state->offsets;
    const size_t new_length = (state->offsets_end - state->offsets) * 2;

    uint32_t* const new_offset = realloc(state->offsets,
                                         new_length * sizeof(uint32_t));

    if (unlikely(new_offset == NULL)) {
        PyErr_NoMemory();
        return -1;
    }

    state->offsets = new_offset;
    state->offsets_end = new_offset + new_length;
    state->current_offset = new_offset + offset_offset;

    return 0;
}

static int
pack_data(bplist_generate_state* const state,
          PyObject* const py_obj,
          const uint8_t type)
{
    char* bytes;
    ssize_t bytes_length;

    const int did_extract =
        PyBytes_AsStringAndSize(py_obj, (char**)&bytes, &bytes_length);

    if (unlikely(did_extract == -1))
        return -1;

    pack_type_and_length(state, type, bytes_length);

    const size_t length = (size_t)bytes_length;

    const int enough_space = resize_plist_if_needed(state, length);
    if (unlikely(enough_space == -1))
        return -1;

    memcpy(state->current_object, bytes, length);
    state->current_object += length;

    return 0;
}

static int
generate_data(bplist_generate_state* const state,
              PyObject* const py_obj)
{
    return pack_data(state, py_obj, plist_type_data);
}

static PyObject*
parse_ascii_string(const uint8_t type,
                   const uint8_t* object,
                   const uint8_t* const object_end)
{
    const ssize_t length = unpack_length(type, &object);
    if (likely((object + length) < object_end))
        return PyUnicode_DecodeASCII((char*)object, length, NULL);

    PyErr_Format(PyExc_RuntimeError,
                 "bplist parsing hit an invalid ascii length: %zd",
                 length);
    return NULL;
}

static PyObject*
parse_utf16_string(const uint8_t type,
                   const uint8_t* object,
                   const uint8_t* const object_end)
{
    int byte_order = 1; /* 1 == big endian */
    ssize_t length = unpack_length(type, &object);

    length *= 2; /* multiply by 2 because string is UTF16 encoded */

    if (likely((object + length) < object_end))
        return PyUnicode_DecodeUTF16((char*)object,
                                     length,
                                     NULL,
                                     &byte_order);

    PyErr_Format(PyExc_RuntimeError,
                 "bplist parsing hit an invalid utf16 length: %zd",
                 length);
    return NULL;
}

static int
generate_ascii(bplist_generate_state* const state, PyObject* const py_obj)
{
    PyObject* const str_bytes = PyUnicode_AsASCIIString(py_obj);

    if (str_bytes == NULL)
        return -1;

    const int result = pack_data(state, str_bytes, plist_type_ascii_string);

    Py_DECREF(str_bytes);

    return result;
}

/**
 * Cached reference to the UTF16 big endian string encoder,
 * to avoid lookups later...
 */
static PyObject* utf16_encoder = NULL;

static int
generate_utf16(bplist_generate_state* const state, PyObject* const py_obj)
{
    PyObject* const str_tuple = PyTuple_New(1);
    if (unlikely(str_tuple == NULL))
        return -1;

    Py_INCREF(py_obj);
    PyTuple_SET_ITEM(str_tuple, 0, py_obj);

    PyObject* const str_bytes_tuple =
        PyObject_CallObject(utf16_encoder, str_tuple);

    Py_DECREF(str_tuple);

    if (unlikely(str_bytes_tuple == NULL))
        return -1;

    PyObject* const str_bytes = PyTuple_GET_ITEM(str_bytes_tuple, 0);

    if (unlikely(str_bytes == NULL))
        return -1;

    char* bytes;
    ssize_t bytes_length;

    const int did_extract =
        PyBytes_AsStringAndSize(str_bytes, (char**)&bytes, &bytes_length);

    if (unlikely(did_extract == -1)) {
        Py_DECREF(str_bytes_tuple);
        return -1;
    }

    const size_t length = (size_t)bytes_length;

    /* because 2 bytes per UTF16 character */
    pack_type_and_length(state, plist_type_utf16_string, length / 2);

    const int enough_space = resize_plist_if_needed(state, length);
    if (unlikely(enough_space == -1))
        return -1;

    memcpy(state->current_object, bytes, length);
    state->current_object += length;

    Py_DECREF(str_bytes_tuple);

    return 0;
}

static int
generate_string(bplist_generate_state* const state, PyObject* const py_obj)
{
    const int kind = PyUnicode_KIND(py_obj);

    if (kind == PyUnicode_1BYTE_KIND)
        return generate_ascii(state, py_obj);

    return generate_utf16(state, py_obj);
}

/* forward declaration! */
static PyObject*
parse_plist_object(const bplist_parse_state* const state,
                   const size_t object_index);

static PyObject*
parse_array(const bplist_parse_state* const state,
            const uint8_t type,
            const uint8_t* object,
            const uint8_t* const object_end)
{
    const ssize_t slength = unpack_length(type, &object);
    const size_t ref_size = state->ref_size;

    if (unlikely((object + (slength * ref_size)) > object_end)) {
        PyErr_Format(PyExc_RuntimeError,
                     "bplist parsing hit an invalid array length: %zd",
                     slength);
        return NULL;
    }

    PyObject* const list = PyList_New(slength);
    if (unlikely(list == NULL))
        return NULL;

    const size_t length = (size_t)slength;
    size_t i = 0;
    for (; i < length; i++) {
        const ssize_t list_obj_index = unpack_uint(state->ref_size, object);
        object += ref_size;

        PyObject* const list_obj = parse_plist_object(state, list_obj_index);

        if (unlikely(list_obj == NULL)) {
            Py_DECREF(list);
            return NULL;
        }

        PyList_SET_ITEM(list, i, list_obj);
    }

    return list;
}

static int
generate_plist_object(bplist_generate_state* const state,
                      PyObject* const py_obj);

static int
generate_array(bplist_generate_state* const state, PyObject* const py_obj)
{
    const size_t length = (size_t)PyList_GET_SIZE(py_obj);

    pack_type_and_length(state, plist_type_array, length);

    const size_t ref_size = state->trailer.ref_size;
    const size_t ref_length = (ref_size * length);

    const int we_have_enough_object_space =
        resize_plist_if_needed(state, ref_length);

    if (unlikely(we_have_enough_object_space == -1))
        return -1;

    size_t current_object_offset = state->current_object - state->objects;
    state->current_object += ref_length;

    size_t i = 0;
    for (; i < length; i++) {
        pack_uint(ref_size,
                  state->objects + current_object_offset,
                  state->trailer.num_objects);

        current_object_offset += ref_size;

        PyObject* const list_obj = PyList_GET_ITEM(py_obj, i);
        const int result = generate_plist_object(state, list_obj);

        if (unlikely(result != 0))
            return result;
    }

    return 0;
}

static PyObject*
parse_dict(const bplist_parse_state* const state,
           const uint8_t type,
           const uint8_t* object,
           const uint8_t* const object_end)
{
    const size_t length = (size_t)unpack_length(type, &object);
    const size_t ref_size = state->ref_size;

    if (unlikely((object + (length * ref_size * 2)) > object_end)) {
        PyErr_Format(PyExc_RuntimeError,
                     "bplist parsing hit an invalid dict length: %zd",
                     length);
        return NULL;
    }

    PyObject* const dict = PyDict_New();
    if (unlikely(dict == NULL))
        return NULL;

    const uint8_t* key_object = object;
    const uint8_t* val_object = object + (length * state->ref_size);

    size_t i = 0;
    for (; i < length; i++) {

        const size_t key_obj_index = unpack_uint(state->ref_size, key_object);
        const size_t val_obj_index = unpack_uint(state->ref_size, val_object);

        key_object += ref_size;
        val_object += ref_size;

        PyObject* const key_obj = parse_plist_object(state, key_obj_index);
        PyObject* const val_obj = parse_plist_object(state, val_obj_index);

        if (unlikely(key_obj == NULL || val_obj == NULL)) {
            Py_XDECREF(key_obj);
            Py_XDECREF(val_obj);
            Py_DECREF(dict);
            return NULL;
        }

        const int did_set_item = PyDict_SetItem(dict, key_obj, val_obj);

        Py_DECREF(key_obj);
        Py_DECREF(val_obj);

        if (unlikely(did_set_item == -1)) {
            Py_DECREF(dict);
            return NULL;
        }
    }

    return dict;
}

static int
generate_dict(bplist_generate_state* const state, PyObject* const py_obj)
{
    const size_t length = (size_t)PyDict_Size(py_obj);

    pack_type_and_length(state, plist_type_dict, length);

    const size_t ref_size = state->trailer.ref_size;
    const size_t ref_length = (ref_size * length);

    const int we_have_enough_object_space =
        resize_plist_if_needed(state, ref_length * 2);

    if (unlikely(we_have_enough_object_space == -1))
        return -1;

    /* because recursive calls to generate_plist_object might realloc
     * the buffers, we cannot cache pointers here; we will have to do
     * the slightly slower thing and just cache an offset which use
     * to calculate the address each time
     */

    size_t key_ref_offset = state->current_object - state->objects;
    size_t val_ref_offset = key_ref_offset + ref_length;
    state->current_object += ref_length * 2;

    ssize_t pos = 0;
    PyObject* key = NULL;
    PyObject* val = NULL;

    while (PyDict_Next(py_obj, &pos, &key, &val)) {

        pack_uint(ref_size,
                  state->objects + key_ref_offset,
                  state->trailer.num_objects);

        key_ref_offset += ref_size;

        const int key_result = generate_plist_object(state, key);

        if (unlikely(key_result != 0))
            return key_result;

        pack_uint(ref_size,
                  state->objects + val_ref_offset,
                  state->trailer.num_objects);

        val_ref_offset += ref_size;

        const int val_result = generate_plist_object(state, val);

        if (unlikely(val_result != 0))
            return val_result;
    }

    return 0;
}

/* this is where the magic starts for plist parsing */

static PyObject*
parse_plist_object(const bplist_parse_state* const state,
                   const size_t object_index)
{
    // TODO: check if object_index has already been unpacked, return
    //       that object pointer instead, to avoid circular refs

    /* offsets in the offset_table tell you where an object/value
     * starts in the data blob
     */

    const uint8_t* const object_ref =
        state->offset_table + (object_index * state->offset_size);

    if (unlikely(object_ref <= state->data || object_ref >= state->data_end)) {
        PyErr_Format(PyExc_RuntimeError,
                     "bplist parsing hit an invalid object index: %zd",
                     object_index);
        return NULL;
    }

    const uint8_t* object =
        state->data + unpack_uint(state->offset_size, object_ref);

    if (unlikely(object <= state->data || object >= state->data_end)) {
        PyErr_Format(PyExc_RuntimeError,
                     "bplist parsing hit object out of bounds of input bytes "\
                     "for object at index %zd",
                     object_index);
        return NULL;
    }

    const uint8_t object_type = *object++;

    switch (object_type & 0xF0)
    {
    case plist_type_int:
        return parse_int(object_type, object);

    case plist_type_uid:
        return parse_uid(object_type, object);

    case plist_type_ascii_string:
        return parse_ascii_string(object_type, object, state->data_end);

    case plist_type_utf16_string:
        return parse_utf16_string(object_type, object, state->data_end);

    case plist_type_data:
        return parse_data(object_type, object, state->data_end);

    case plist_type_dict:
        return parse_dict(state, object_type, object, state->data_end);

    case plist_type_array:
        return parse_array(state, object_type, object, state->data_end);

    case plist_type_primitive:
        /* NULL is defined in the spec, but I cannot get
         * PropertyListSerialization to generate a plist with
         * a nil top object or NSNull in a collection;
         * the pure python code also doesn't want to serialize None;
         * same story with UUIDs and URLs
         */
        if (object_type == plist_type_false) Py_RETURN_FALSE;
        if (object_type == plist_type_true)  Py_RETURN_TRUE;
        break;

    case plist_type_float:
        if (object_type == 0x23) return parse_float64(object);
        if (object_type == 0x22) return parse_float32(object);
        break;

    case plist_type_date:
        return parse_date(object);

    /* Support for parsing sets should be here, except I didn't
     * bother to implement them because I cannot get Cocoa to
     * generate a plist with sets---which is a prerequisite for
     * making a unit test...
     */

    default:
        break;
    }

    PyErr_Format(PyExc_RuntimeError,
                 "bplist parsing hit an unknown type: 0x%x",
                 object_type);
    return NULL;
}

static int
check_int_width(const size_t width, const char* const name)
{
    if (unlikely(width != 1 && width != 2 && width != 4 && width != 8)) {
        PyErr_Format(PyExc_RuntimeError,
                     "bplist parser found invalid %s: %d",
                     name,
                     width);
        return -1;
    }

    return 0;
}

static PyObject*
parse_plist(PyObject* const self, PyObject* const plist_data)
{
    char* signed_data;
    ssize_t signed_data_len;

    const int correct_type =
        PyBytes_AsStringAndSize(plist_data, &signed_data, &signed_data_len);

    if (unlikely(correct_type == -1))
        return NULL;

    if (unlikely(strncmp(signed_data,
                         bplist_header,
                         bplist_header_length) != 0))
    {
        PyErr_SetString(PyExc_RuntimeError, "invalid header for bplist");
        return NULL;
    }

    const size_t data_len = (size_t)signed_data_len;

    if (unlikely(data_len < (bplist_header_length + sizeof(bplist_trailer)))) {
        PyErr_SetString(PyExc_RuntimeError, "bplist is too short to be valid");
        return NULL;
    }

    const uint8_t* const data = (const uint8_t*)signed_data;
    const uint8_t* const data_end = data + data_len;

    const bplist_trailer* const trailer =
        (bplist_trailer*)(data_end - sizeof(bplist_trailer));

    const bplist_parse_state state = {
        .data = data,
        .data_end = (const uint8_t*)trailer, /* minor lie */
        .ref_size = (size_t)trailer->ref_size,
        .offset_size = (size_t)trailer->offset_size,
        .object_count = SwapBigToHost64(trailer->num_objects),
        .offset_table = data + SwapBigToHost64(trailer->offset_table_offset)
    };

    if (unlikely(check_int_width(state.offset_size, "offset_size") == -1 ||
                 check_int_width(state.ref_size, "ref_size") == -1))
    {
        return NULL;
    }

    if (unlikely(state.offset_table > state.data_end)) {
        PyErr_SetString(PyExc_RuntimeError,
                        "bplist offset_table is out of bounds of input bytes");
        return NULL;
    }

    /* top_object_index is an index into the offset_table
     * for the top-level object in the plist
     */
    const uint64_t top_object_index = SwapBigToHost64(trailer->top_object);

    return parse_plist_object(&state, top_object_index);
}

static int
generate_plist_object(bplist_generate_state* const state,
                      PyObject* const py_obj)
{
    const int we_have_enough_offset_table_space =
        grow_offset_table_buffer_if_needed(state);

    if (unlikely(we_have_enough_offset_table_space == -1))
        return -1;

    *state->current_offset++ = (state->current_object - state->objects);

    state->trailer.num_objects++;
    if (unlikely(state->trailer.num_objects == ((1 << 16) - 1)))
        return 1;

    /* arbitrarily deciding that 32 bytes is the maximum number of bytes
     * we need for a single fixed width object in a plist (bool, int, float)
     */
    const int we_have_enough_object_space =
        resize_plist_if_needed(state, 32);

    if (unlikely(we_have_enough_object_space == -1))
            return -1;

    const PyTypeObject* const type = Py_TYPE(py_obj);

    if (type == (PyTypeObject*)uid_class)
        return generate_uid(state, py_obj);

    if (PyLong_Check(py_obj))
        return generate_int(state, py_obj);

    if (PyUnicode_Check(py_obj))
        return generate_string(state, py_obj);

    if (PyBytes_Check(py_obj))
        return generate_data(state, py_obj);

    if (PyDict_Check(py_obj))
        return generate_dict(state, py_obj);

    if (PyList_Check(py_obj))
        return generate_array(state, py_obj);

    if (type == &PyBool_Type) {
        *state->current_object++ =
            (py_obj == Py_True ? plist_type_true : plist_type_false);
        return 0;
    }

    /* since bpylist.timestamp is a subclass of float, we need to
     * check for it before we check for other float objects
     */
    if (type == (PyTypeObject*)ts_class)
        return generate_date(state, py_obj);

    if (PyFloat_Check(py_obj))
        return generate_float(state, py_obj);

    PyErr_Format(PyExc_RuntimeError,
                 "bplist generation does not support serializing `%R'",
                 py_obj);
    return -1;
}

static int
generate_plist_offset_table_and_trailer(bplist_generate_state* const state)
{
    /* the final resizing! we can calculate the offset table
     * size here, and add the trailer size as well
     */

    const size_t objects_length =
        state->current_object - state->objects;

    /* decide how much we can shrink our offset table by */
    if (objects_length <= 0xFF) {
        state->trailer.offset_size = 1;
    }
    else if (objects_length <= 0xFFFF) {
        state->trailer.offset_size = 2;
    }
    else if (objects_length <= 0xFFFFFFFF) {
        state->trailer.offset_size = 4;
    }
    else {
        PyErr_Format(PyExc_RuntimeError,
                     "bplist generation object space overflow: %zd bytes",
                     objects_length);
        return -1;
    }

    const size_t num_objects =
        state->trailer.num_objects;

    const size_t offset_size =
        state->trailer.offset_size;

    const size_t offset_table_length =
        (num_objects * offset_size);

    const size_t trailer_length =
        sizeof(bplist_trailer);

    const size_t final_size =
        objects_length + offset_table_length + trailer_length;

    const int did_shrink = resize_plist_buffer(state, final_size);
    if (unlikely(did_shrink == -1))
        return -1;

    state->trailer.offset_table_offset =
        state->current_object - state->objects;

    /* reset for iteration! */
    state->current_offset = state->offsets;

    size_t i = 0;
    for (; i < num_objects; i++) {
        const size_t offset = (size_t)(*state->current_offset++);
        pack_uint(offset_size, state->current_object, offset);
        state->current_object += offset_size;
    }

    state->trailer.num_objects =
        SwapHostToBig64(num_objects);

    state->trailer.offset_table_offset =
        SwapHostToBig64(state->trailer.offset_table_offset);

    memcpy(state->current_object,
           &state->trailer,
           sizeof(bplist_trailer));

    /* feels weird not to have this, but it is not necessary...
     *
     * state->current_object += sizeof(bplist_trailer);
     */

    return 0;
}

static PyObject*
generate_plist(PyObject* const self, PyObject* const py_obj)
{
    static const size_t initial_offset_table_length = 4096;
    static const size_t initial_objects_buffer_length = 4096;

    bplist_generate_state state = {
        .offsets = malloc(initial_offset_table_length),
        .trailer = {
            .offset_size = 4,
            .ref_size = 2,
            .num_objects = 0,
            .top_object = 0,
            .offset_table_offset = 0
        },
        .plist = PyBytes_FromStringAndSize(NULL, initial_objects_buffer_length)
    };

    if (unlikely(state.plist == NULL || state.offsets == NULL)) {
        free(state.offsets);
        return PyErr_NoMemory();
    }

    state.offsets_end =
        state.offsets + (initial_offset_table_length / sizeof(uint32_t));
    state.current_offset = state.offsets;

    ssize_t signed_buffer_length;

    const int did_extract =
        PyBytes_AsStringAndSize(state.plist,
                                (char**)&state.objects,
                                &signed_buffer_length);

    if (unlikely(did_extract == -1)) {
        Py_DECREF(state.plist);
        free(state.offsets);
        return NULL;
    }

    state.objects_end = state.objects + initial_objects_buffer_length;
    state.current_object = state.objects;

    /* start off by adding the plist header */
    memcpy(state.current_object, bplist_header, bplist_header_length);
    state.current_object += bplist_header_length;

    /* and then the rest of the plist */
    const int object_generation = generate_plist_object(&state, py_obj);

    if (unlikely(object_generation != 0)) {
        Py_DECREF(state.plist);
        state.plist = NULL;

        if (object_generation == 1) {
            /* retry with larger ref_size */
            abort();
        }
    }
    else if (unlikely(generate_plist_offset_table_and_trailer(&state) != 0)) {
        Py_DECREF(state.plist);
        state.plist = NULL;
    }

    free(state.offsets);
    return state.plist;
}

PyDoc_STRVAR(parse__docstring__,
             "parse(plist_data: bytes) -> object\n\n" \
             "parse a binary plist!");

PyDoc_STRVAR(generate__docstring__,
             "generate(plist_object: bytes) -> bytes\n\n" \
             "generate a binary plist data blob from a python object");

static PyMethodDef plist_methods[] = {
    {
        "parse",
        parse_plist,
        METH_O,
        parse__docstring__
    },
    {
        "generate",
        generate_plist,
        METH_O,
        generate__docstring__
    },
    { /* sentinel */
        NULL,
        NULL,
        0,
        NULL
    }
};

PyDoc_STRVAR(plist__docstring__,
             "Binary property list parsing and generation--C accelerated."\
             "Zoom zoom!");

static PyModuleDef plist_module = {
    PyModuleDef_HEAD_INIT,
    "bplist",   /* module name */
    plist__docstring__,
    -1,         /* size of per-interpreter state */
    plist_methods
};

PyMODINIT_FUNC
PyInit_bplist(void)
{
    PyObject* const codec_module = PyImport_ImportModule("codecs");
    if (unlikely(codec_module == NULL))
        return NULL;

    PyObject* const codec_finder =
        PyObject_GetAttrString(codec_module, "getencoder");

    if (unlikely(codec_finder == NULL)) {
        Py_DECREF(codec_module);
        return NULL;
    }

    PyObject* const tuple = Py_BuildValue("(s)", "utf-16be");
    if (unlikely(tuple == NULL)) {
        Py_DECREF(codec_finder);
        Py_DECREF(codec_module);
        return NULL;
    }

    utf16_encoder = PyObject_CallObject(codec_finder, tuple);

    Py_DECREF(tuple);
    Py_DECREF(codec_finder);
    Py_DECREF(codec_module);

    if (utf16_encoder == NULL)
        return NULL;

    PyObject* const t_module =
        PyImport_ImportModule("bpylist.archive_types");

    if (t_module == NULL) {
        Py_CLEAR(utf16_encoder);
        return NULL;
    }

    ts_class = PyObject_GetAttrString(t_module, "timestamp");

    if (unlikely(ts_class == NULL)) {
        Py_CLEAR(utf16_encoder);
        Py_DECREF(t_module);
        return NULL;
    }

    uid_class = PyObject_GetAttrString(t_module, "uid");

    Py_DECREF(t_module);

    if (unlikely(uid_class == NULL)) {
        Py_CLEAR(utf16_encoder);
        Py_CLEAR(ts_class);
        return NULL;
    }

    return PyModule_Create(&plist_module);
}
