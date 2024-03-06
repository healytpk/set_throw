#ifndef HEADER_INCLUSION_GUARD_SET_THROW_HPP
#define HEADER_INCLUSION_GUARD_SET_THROW_HPP

#include <cstdint>        // uint32_t, UINTPTR_MAX
#include <atomic>         // atomic
#include <exception>      // exception
#include <stdexcept>      // runtime_error
#include <typeinfo>       // type_info

#ifdef _MSC_VER
    extern "C" void *__stdcall LoadLibraryA(char const*);
    extern "C" void *__stdcall GetProcAddress(void*,char const*);
    extern "C" void *__stdcall GetModuleHandleA(char const*);
#else
    extern "C" void *dlsym(void*,char const*);
#  ifndef RTLD_NEXT
#    define RTLD_NEXT (reinterpret_cast<void*>(UINTPTR_MAX))
#  endif
#endif

namespace std {
    using throw_handler = void (*)(void const *, type_info const &);  // cannot mark as noexcept?
    inline std::atomic<throw_handler> detail_current_throw_handler{nullptr};
    inline throw_handler get_throw(void) noexcept
    {
        return detail_current_throw_handler.load();
    }
    inline throw_handler set_throw(throw_handler const f) noexcept
    {
        return atomic_exchange(&detail_current_throw_handler, f);
    }
}

namespace detail_for_set_throw {

#ifdef _MSC_VER
    using FuncPtr = void (__stdcall *)(void*,void const*);   // _s__ThowInfo is built into compiler

    std::uint32_t const *Pointer(std::uint32_t const p)
    {
        if constexpr ( 8u == sizeof(void*) )
        {
            auto const base = static_cast<std::uint32_t const*>(static_cast<void const*>(GetModuleHandleA(nullptr)));
            return base + p/4;
        }
        else
        {
            return reinterpret_cast<std::uint32_t const*>(p);
        }
    }
#else
    using FuncPtr = void (*)(void*,void*,void (*)(void*));
#endif

    FuncPtr GetOriginalThrower(void) noexcept
    {
#ifdef _MSC_VER
        auto const h = ::LoadLibraryA("vcruntime140.dll");
        if ( nullptr == h ) return nullptr;
        return (FuncPtr) ::GetProcAddress(h, "_CxxThrowException");
#else
        return (FuncPtr) ::dlsym(RTLD_NEXT, "__cxa_throw");
#endif
    }

    // The next line ensures that 'GetOriginalThrower' is called before 'main'
    FuncPtr const original_thrower = GetOriginalThrower();

    std::type_info const *GetTypeInfoFromSecondArgument(void const *const arg)
    {
        if ( nullptr == arg ) return &typeid(void);
#ifdef _MSC_VER
        auto const pThrowInfo = static_cast<std::uint32_t const*>(arg);
        auto const base = static_cast<char const*>(static_cast<void const*>(GetModuleHandleA(nullptr)));
        if ( nullptr == base ) return &typeid(void);
        std::uint32_t p;
        p = pThrowInfo[3];
        if ( 0 == p ) return &typeid(void);
        auto const pCatchArray = Pointer(p);
        p = pCatchArray[1];
        if ( 0 == p ) return &typeid(void);
        auto const pCatchType = Pointer(p);
        p = pCatchType[1];
        if ( 0 == p ) return &typeid(void);
        return static_cast<std::type_info const*>( static_cast<void const*>(Pointer(p)) );
#else
        return static_cast<std::type_info const *>(arg);
#endif
    }
}

#ifdef _MSC_VER
#  ifdef _WIN64
extern "C" [[noreturn]] void _CxxThrowException(void *const p1, _s__ThrowInfo const *const p2) noexcept(false)
#  else
extern "C" [[noreturn]] void _CxxThrowException(void *const p1, void const *const p2) noexcept(false)
#  endif
#else
extern "C" [[noreturn]] void __cxa_throw(void *const p1, void *const p2, void (*const pf)(void*)) noexcept(false)
#endif
{
    static thread_local bool already_entered = false;
    if ( already_entered ) std::terminate();

    auto const handler = std::detail_current_throw_handler.load();
    if ( handler )
    {
        already_entered = true;
        handler(p1, *detail_for_set_throw::GetTypeInfoFromSecondArgument(p2));
        already_entered = false;
    }

#ifdef _MSC_VER
    detail_for_set_throw::original_thrower(p1, p2);
#else
    detail_for_set_throw::original_thrower(p1,p2,pf);
#endif

    std::terminate();  // shouldn't be needed -- but belt and braces
}

#endif    // HEADER_INCLUSION_GUARD_SET_THROW_HPP
