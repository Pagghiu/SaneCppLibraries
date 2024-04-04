#include "sc_hashing.h"
#include "../../../Libraries/Hashing/Hashing.h"

SC::Hashing& sc_hashing_self(sc_hashing_t* hashing)
{
    static_assert(sizeof(sc_hashing_t::opaque) >= sizeof(SC::Hashing), "sc_hashing_t::opaque size");
    static_assert(alignof(sc_hashing_t) >= alignof(SC::Hashing), "sc_hashing_t::opaque alignment");
    return *reinterpret_cast<SC::Hashing*>(hashing->opaque);
}

extern "C" bool sc_hashing_init(sc_hashing_t* hashing, sc_hashing_type type)
{
    SC::Hashing& self = sc_hashing_self(hashing);
    placementNew(self);
    switch (type)
    {
    case SC_HASHING_TYPE_MD5: return self.setType(SC::Hashing::TypeMD5);
    case SC_HASHING_TYPE_SHA1: return self.setType(SC::Hashing::TypeSHA1);
    case SC_HASHING_TYPE_SHA256: return self.setType(SC::Hashing::TypeSHA256);
    }
    return false;
}

extern "C" void sc_hashing_close(sc_hashing_t* hashing) { sc_hashing_self(hashing).~Hashing(); }

extern "C" bool sc_hashing_add(sc_hashing_t* hashing, sc_hashing_span_t span)
{
    return sc_hashing_self(hashing).add({reinterpret_cast<const uint8_t*>(span.data), span.length});
}

extern "C" bool sc_hashing_get(sc_hashing_t* hashing, sc_hashing_result_t* result)
{
    static_assert(sizeof(sc_hashing_result_t) == sizeof(SC::Hashing::Result), "sc_hashing_result_t size");
    static_assert(alignof(sc_hashing_result_t) == alignof(SC::Hashing::Result), "sc_hashing_result_t alignment");
    return sc_hashing_self(hashing).getHash(*reinterpret_cast<SC::Hashing::Result*>(result));
}
