#pragma once

#define strBuilderAppendFixdKeyVPtr_d(result, key, vptr) \
    {                                                    \
        auto vp = vptr;                                  \
        if (nullptr != vp) {                             \
            result.append_key_value<key>(vp);            \
            result.append_comma();                       \
        }                                                \
    }
