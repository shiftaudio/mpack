/*
 * Copyright (c) 2015 Nicholas Fraser
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#define MPACK_INTERNAL 1

#include "mpack-common.h"

#if MPACK_DEBUG && MPACK_STDIO
#include <stdarg.h>
#endif

const char* mpack_error_to_string(mpack_error_t error) {
    #if MPACK_DEBUG
    switch (error) {
        #define MPACK_ERROR_STRING_CASE(e) case e: return #e
        MPACK_ERROR_STRING_CASE(mpack_ok);
        MPACK_ERROR_STRING_CASE(mpack_error_io);
        MPACK_ERROR_STRING_CASE(mpack_error_invalid);
        MPACK_ERROR_STRING_CASE(mpack_error_type);
        MPACK_ERROR_STRING_CASE(mpack_error_too_big);
        MPACK_ERROR_STRING_CASE(mpack_error_memory);
        MPACK_ERROR_STRING_CASE(mpack_error_bug);
        MPACK_ERROR_STRING_CASE(mpack_error_data);
        #undef MPACK_ERROR_STRING_CASE
        default: break;
    }
    mpack_assert(0, "unrecognized error %i", (int)error);
    return "(unknown mpack_error_t)";
    #else
    MPACK_UNUSED(error);
    return "";
    #endif
}

const char* mpack_type_to_string(mpack_type_t type) {
    #if MPACK_DEBUG
    switch (type) {
        #define MPACK_TYPE_STRING_CASE(e) case e: return #e
        MPACK_TYPE_STRING_CASE(mpack_type_nil);
        MPACK_TYPE_STRING_CASE(mpack_type_bool);
        MPACK_TYPE_STRING_CASE(mpack_type_float);
        MPACK_TYPE_STRING_CASE(mpack_type_double);
        MPACK_TYPE_STRING_CASE(mpack_type_int);
        MPACK_TYPE_STRING_CASE(mpack_type_uint);
        MPACK_TYPE_STRING_CASE(mpack_type_str);
        MPACK_TYPE_STRING_CASE(mpack_type_bin);
        MPACK_TYPE_STRING_CASE(mpack_type_ext);
        MPACK_TYPE_STRING_CASE(mpack_type_array);
        MPACK_TYPE_STRING_CASE(mpack_type_map);
        #undef MPACK_TYPE_STRING_CASE
        default: break;
    }
    mpack_assert(0, "unrecognized type %i", (int)type);
    return "(unknown mpack_type_t)";
    #else
    MPACK_UNUSED(type);
    return "";
    #endif
}

int mpack_tag_cmp(mpack_tag_t left, mpack_tag_t right) {

    // positive numbers may be stored as int; convert to uint
    if (left.type == mpack_type_int && left.v.i >= 0) {
        left.type = mpack_type_uint;
        left.v.u = left.v.i;
    }
    if (right.type == mpack_type_int && right.v.i >= 0) {
        right.type = mpack_type_uint;
        right.v.u = right.v.i;
    }

    if (left.type != right.type)
        return (int)left.type - (int)right.type;

    switch (left.type) {
        case mpack_type_nil:
            return 0;

        case mpack_type_bool:
            return (int)left.v.b - (int)right.v.b;

        case mpack_type_int:
            if (left.v.i == right.v.i)
                return 0;
            return (left.v.i < right.v.i) ? -1 : 1;

        case mpack_type_uint:
            if (left.v.u == right.v.u)
                return 0;
            return (left.v.u < right.v.u) ? -1 : 1;

        case mpack_type_array:
        case mpack_type_map:
            if (left.v.n == right.v.n)
                return 0;
            return (left.v.n < right.v.n) ? -1 : 1;

        case mpack_type_str:
        case mpack_type_bin:
            if (left.v.l == right.v.l)
                return 0;
            return (left.v.l < right.v.l) ? -1 : 1;

        case mpack_type_ext:
            if (left.exttype == right.exttype) {
                if (left.v.l == right.v.l)
                    return 0;
                return (left.v.l < right.v.l) ? -1 : 1;
            }
            return (int)left.exttype - (int)right.exttype;

        // floats should not normally be compared for equality. we compare
        // with memcmp() to silence compiler warnings, but this will return
        // equal if both are NaNs with the same representation (though we may
        // want this, for instance if you are for some bizarre reason using
        // floats as map keys.) i'm not sure what the right thing to
        // do is here. check for NaN first? always return false if the type
        // is float? use operator== and pragmas to silence compiler warning?
        // please send me your suggestions.
        // note also that we don't convert floats to doubles, so when this is
        // used for ordering purposes, all floats are ordered before all
        // doubles.
        case mpack_type_float:
            return mpack_memcmp(&left.v.f, &right.v.f, sizeof(left.v.f));
        case mpack_type_double:
            return mpack_memcmp(&left.v.d, &right.v.d, sizeof(left.v.d));

        default:
            break;
    }
    
    mpack_assert(0, "unrecognized type %i", (int)left.type);
    return false;
}



#if MPACK_TRACKING

#ifndef MPACK_TRACKING_INITIAL_CAPACITY
// seems like a reasonable number. we grow by doubling, and it only
// needs to be as long as the maximum depth of the message.
#define MPACK_TRACKING_INITIAL_CAPACITY 8
#endif

mpack_error_t mpack_track_init(mpack_track_t* track) {
    track->count = 0;
    track->capacity = MPACK_TRACKING_INITIAL_CAPACITY;
    track->elements = (mpack_track_element_t*)MPACK_MALLOC(sizeof(mpack_track_element_t) * track->capacity);
    if (track->elements == NULL)
        return mpack_error_memory;
    return mpack_ok;
}

mpack_error_t mpack_track_push(mpack_track_t* track, mpack_type_t type, uint64_t count) {
    mpack_assert(track->elements, "null track elements!");

    // maps have twice the number of elements (key/value pairs)
    if (type == mpack_type_map)
        count *= 2;

    // grow if needed
    if (track->count == track->capacity) {
        size_t new_capacity = track->capacity * 2;
        mpack_track_element_t* new_elements = (mpack_track_element_t*)
                MPACK_MALLOC(sizeof(mpack_track_element_t) * new_capacity);
        if (new_elements == NULL)
            return mpack_error_bug;
        mpack_memcpy(new_elements, track->elements, sizeof(mpack_track_element_t) * track->count);
        MPACK_FREE(track->elements);
        track->elements = new_elements;
        track->capacity = new_capacity;
    }

    // insert new track
    ++track->count;
    track->elements[track->count - 1].type = type;
    track->elements[track->count - 1].left = count;
    return mpack_ok;
}

mpack_error_t mpack_track_pop(mpack_track_t* track, mpack_type_t type) {
    mpack_assert(track->elements, "null track elements!");

    if (track->count == 0) {
        mpack_assert(0, "attempting to close a %s but nothing was opened!", mpack_type_to_string(type));
        return mpack_error_bug;
    }

    mpack_track_element_t* element = &track->elements[track->count - 1];

    if (element->type != type) {
        mpack_assert(0, "attempting to close a %s but the open element is a %s!",
                mpack_type_to_string(type), mpack_type_to_string(element->type));
        return mpack_error_bug;
    }

    if (element->left != 0) {
        mpack_assert(0, "attempting to close a %s but there are %"PRIu64" %s left",
                mpack_type_to_string(type), element->left,
                (type == mpack_type_map || type == mpack_type_array) ? "elements" : "bytes");
        return mpack_error_bug;
    }

    --track->count;
    return mpack_ok;
}

mpack_error_t mpack_track_element(mpack_track_t* track, bool read) {
    MPACK_UNUSED(read);
    mpack_assert(track->elements, "null track elements!");

    // if there are no open elements, that's fine, we can read elements at will
    if (track->count == 0)
        return mpack_ok;

    mpack_track_element_t* element = &track->elements[track->count - 1];

    if (element->type != mpack_type_map && element->type != mpack_type_array) {
        mpack_assert(0, "elements cannot be %s within an %s", read ? "read" : "written",
                mpack_type_to_string(element->type));
        return mpack_error_bug;
    }

    if (element->left == 0) {
        mpack_assert(0, "too many elements %s for %s", read ? "read" : "written",
                mpack_type_to_string(element->type));
        return mpack_error_bug;
    }

    --element->left;
    return mpack_ok;
}

mpack_error_t mpack_track_bytes(mpack_track_t* track, bool read, uint64_t count) {
    MPACK_UNUSED(read);
    mpack_assert(track->elements, "null track elements!");

    if (track->count == 0) {
        mpack_assert(0, "bytes cannot be %s with no open map or array", read ? "read" : "written");
        return mpack_error_bug;
    }

    mpack_track_element_t* element = &track->elements[track->count - 1];

    if (element->type == mpack_type_map || element->type == mpack_type_array) {
        mpack_assert(0, "bytes cannot be %s within an %s", read ? "read" : "written",
                mpack_type_to_string(element->type));
        return mpack_error_bug;
    }

    if (element->left < count) {
        mpack_assert(0, "too many bytes %s for %s", read ? "read" : "written",
                mpack_type_to_string(element->type));
        return mpack_error_bug;
    }

    element->left -= count;
    return mpack_ok;
}

#endif

