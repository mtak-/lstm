#ifndef LSTM_DETAIL_COMPILER_HPP
#define LSTM_DETAIL_COMPILER_HPP

#ifdef __cplusplus
# define LSTM_COMPILER_IS_Comeau 0
# define LSTM_COMPILER_IS_Intel 0
# define LSTM_COMPILER_IS_PathScale 0
# define LSTM_COMPILER_IS_Embarcadero 0
# define LSTM_COMPILER_IS_Borland 0
# define LSTM_COMPILER_IS_Watcom 0
# define LSTM_COMPILER_IS_OpenWatcom 0
# define LSTM_COMPILER_IS_SunPro 0
# define LSTM_COMPILER_IS_HP 0
# define LSTM_COMPILER_IS_Compaq 0
# define LSTM_COMPILER_IS_zOS 0
# define LSTM_COMPILER_IS_XL 0
# define LSTM_COMPILER_IS_VisualAge 0
# define LSTM_COMPILER_IS_PGI 0
# define LSTM_COMPILER_IS_Cray 0
# define LSTM_COMPILER_IS_TI 0
# define LSTM_COMPILER_IS_Fujitsu 0
# define LSTM_COMPILER_IS_SCO 0
# define LSTM_COMPILER_IS_AppleClang 0
# define LSTM_COMPILER_IS_Clang 0
# define LSTM_COMPILER_IS_GNU 0
# define LSTM_COMPILER_IS_MSVC 0
# define LSTM_COMPILER_IS_ADSP 0
# define LSTM_COMPILER_IS_IAR 0
# define LSTM_COMPILER_IS_ARMCC 0
# define LSTM_COMPILER_IS_MIPSpro 0

#if defined(__COMO__)
# undef LSTM_COMPILER_IS_Comeau
# define LSTM_COMPILER_IS_Comeau 1

#elif defined(__INTEL_COMPILER) || defined(__ICC)
# undef LSTM_COMPILER_IS_Intel
# define LSTM_COMPILER_IS_Intel 1

#elif defined(__PATHCC__)
# undef LSTM_COMPILER_IS_PathScale
# define LSTM_COMPILER_IS_PathScale 1

#elif defined(__BORLANDC__) && defined(__CODEGEARC_VERSION__)
# undef LSTM_COMPILER_IS_Embarcadero
# define LSTM_COMPILER_IS_Embarcadero 1

#elif defined(__BORLANDC__)
# undef LSTM_COMPILER_IS_Borland
# define LSTM_COMPILER_IS_Borland 1

#elif defined(__WATCOMC__) && __WATCOMC__ < 1200
# undef LSTM_COMPILER_IS_Watcom
# define LSTM_COMPILER_IS_Watcom 1

#elif defined(__WATCOMC__)
# undef LSTM_COMPILER_IS_OpenWatcom
# define LSTM_COMPILER_IS_OpenWatcom 1

#elif defined(__SUNPRO_CC)
# undef LSTM_COMPILER_IS_SunPro
# define LSTM_COMPILER_IS_SunPro 1

#elif defined(__HP_aCC)
# undef LSTM_COMPILER_IS_HP
# define LSTM_COMPILER_IS_HP 1

#elif defined(__DECCXX)
# undef LSTM_COMPILER_IS_Compaq
# define LSTM_COMPILER_IS_Compaq 1

#elif defined(__IBMCPP__) && defined(__COMPILER_VER__)
# undef LSTM_COMPILER_IS_zOS
# define LSTM_COMPILER_IS_zOS 1

#elif defined(__IBMCPP__) && !defined(__COMPILER_VER__) && __IBMCPP__ >= 800
# undef LSTM_COMPILER_IS_XL
# define LSTM_COMPILER_IS_XL 1

#elif defined(__IBMCPP__) && !defined(__COMPILER_VER__) && __IBMCPP__ < 800
# undef LSTM_COMPILER_IS_VisualAge
# define LSTM_COMPILER_IS_VisualAge 1

#elif defined(__PGI)
# undef LSTM_COMPILER_IS_PGI
# define LSTM_COMPILER_IS_PGI 1

#elif defined(_CRAYC)
# undef LSTM_COMPILER_IS_Cray
# define LSTM_COMPILER_IS_Cray 1

#elif defined(__TI_COMPILER_VERSION__)
# undef LSTM_COMPILER_IS_TI
# define LSTM_COMPILER_IS_TI 1

#elif defined(__FUJITSU) || defined(__FCC_VERSION) || defined(__fcc_version)
# undef LSTM_COMPILER_IS_Fujitsu
# define LSTM_COMPILER_IS_Fujitsu 1

#elif defined(__SCO_VERSION__)
# undef LSTM_COMPILER_IS_SCO
# define LSTM_COMPILER_IS_SCO 1

#elif defined(__clang__) && defined(__apple_build_version__)
# undef LSTM_COMPILER_IS_AppleClang
# define LSTM_COMPILER_IS_AppleClang 1

#elif defined(__clang__)
# undef LSTM_COMPILER_IS_Clang
# define LSTM_COMPILER_IS_Clang 1

#elif defined(__GNUC__)
# undef LSTM_COMPILER_IS_GNU
# define LSTM_COMPILER_IS_GNU 1

#elif defined(_MSC_VER)
# undef LSTM_COMPILER_IS_MSVC
# define LSTM_COMPILER_IS_MSVC 1

#elif defined(__VISUALDSPVERSION__) || defined(__ADSPBLACKFIN__) || defined(__ADSPTS__) || defined(__ADSP21000__)
# undef LSTM_COMPILER_IS_ADSP
# define LSTM_COMPILER_IS_ADSP 1

#elif defined(__IAR_SYSTEMS_ICC__ ) || defined(__IAR_SYSTEMS_ICC)
# undef LSTM_COMPILER_IS_IAR
# define LSTM_COMPILER_IS_IAR 1

#elif defined(__ARMCC_VERSION)
# undef LSTM_COMPILER_IS_ARMCC
# define LSTM_COMPILER_IS_ARMCC 1

#elif defined(_SGI_COMPILER_VERSION) || defined(_COMPILER_VERSION)
# undef LSTM_COMPILER_IS_MIPSpro
# define LSTM_COMPILER_IS_MIPSpro 1


#endif

/****************** thread_local ******************/
#  if LSTM_COMPILER_IS_GNU

#    if !((__GNUC__ * 100 + __GNUC_MINOR__) >= 404)
#      error Unsupported compiler version
#    endif

# define LSTM_COMPILER_VERSION_MAJOR (__GNUC__)
# if defined(__GNUC_MINOR__)
#  define LSTM_COMPILER_VERSION_MINOR (__GNUC_MINOR__)
# endif
# if defined(__GNUC_PATCHLEVEL__)
#  define LSTM_COMPILER_VERSION_PATCH (__GNUC_PATCHLEVEL__)
# endif

#    if (__GNUC__ * 100 + __GNUC_MINOR__) >= 408 && __cplusplus >= 201103L
#      define LSTM_COMPILER_CXX_THREAD_LOCAL 1
#    else
#      define LSTM_COMPILER_CXX_THREAD_LOCAL 0
#    endif

#  elif LSTM_COMPILER_IS_Clang

#    if !(((__clang_major__ * 100) + __clang_minor__) >= 304)
#      error Unsupported compiler version
#    endif

# define LSTM_COMPILER_VERSION_MAJOR (__clang_major__)
# define LSTM_COMPILER_VERSION_MINOR (__clang_minor__)
# define LSTM_COMPILER_VERSION_PATCH (__clang_patchlevel__)
# if defined(_MSC_VER)
   /* _MSC_VER = VVRR */
#  define LSTM_SIMULATE_VERSION_MAJOR (_MSC_VER / 100)
#  define LSTM_SIMULATE_VERSION_MINOR (_MSC_VER % 100)
# endif

#    if ((__clang_major__ * 100) + __clang_minor__) >= 304 && __has_feature(cxx_thread_local)
#      define LSTM_COMPILER_CXX_THREAD_LOCAL 1
#    else
#      define LSTM_COMPILER_CXX_THREAD_LOCAL 0
#    endif

#  elif LSTM_COMPILER_IS_AppleClang

#    if !(((__clang_major__ * 100) + __clang_minor__) >= 400)
#      error Unsupported compiler version
#    endif

# define LSTM_COMPILER_VERSION_MAJOR (__clang_major__)
# define LSTM_COMPILER_VERSION_MINOR (__clang_minor__)
# define LSTM_COMPILER_VERSION_PATCH (__clang_patchlevel__)
# if defined(_MSC_VER)
   /* _MSC_VER = VVRR */
#  define LSTM_SIMULATE_VERSION_MAJOR (_MSC_VER / 100)
#  define LSTM_SIMULATE_VERSION_MINOR (_MSC_VER % 100)
# endif
# define LSTM_COMPILER_VERSION_TWEAK (__apple_build_version__)

#    if ((__clang_major__ * 100) + __clang_minor__) >= 400 && __has_feature(cxx_thread_local)
#      define LSTM_COMPILER_CXX_THREAD_LOCAL 1
#    else
#      define LSTM_COMPILER_CXX_THREAD_LOCAL 0
#    endif

#  elif LSTM_COMPILER_IS_MSVC

#    if !(_MSC_VER >= 1600)
#      error Unsupported compiler version
#    endif

  /* _MSC_VER = VVRR */
# define LSTM_COMPILER_VERSION_MAJOR (_MSC_VER / 100)
# define LSTM_COMPILER_VERSION_MINOR (_MSC_VER % 100)
# if defined(_MSC_FULL_VER)
#  if _MSC_VER >= 1400
    /* _MSC_FULL_VER = VVRRPPPPP */
#   define LSTM_COMPILER_VERSION_PATCH (_MSC_FULL_VER % 100000)
#  else
    /* _MSC_FULL_VER = VVRRPPPP */
#   define LSTM_COMPILER_VERSION_PATCH (_MSC_FULL_VER % 10000)
#  endif
# endif
# if defined(_MSC_BUILD)
#  define LSTM_COMPILER_VERSION_TWEAK (_MSC_BUILD)
# endif

#    if _MSC_VER >= 1900
#      define LSTM_COMPILER_CXX_THREAD_LOCAL 1
#    else
#      define LSTM_COMPILER_CXX_THREAD_LOCAL 0
#    endif

#  else
#    error Unsupported compiler
#  endif

#  if LSTM_COMPILER_CXX_THREAD_LOCAL
#    define LSTM_THREAD_LOCAL thread_local
#  else
#    error "lstm requires support for thread_local storage duration specifier"
#  endif
/**************** end thread_local ****************/

/******************* inline var *******************/
#  define LSTM_INLINE_VAR(...)                                                                     \
    template<std::nullptr_t = nullptr>                                                             \
    __VA_ARGS__                                                                                    \
    /**/

#  define LSTM_ACCESS_INLINE_VAR(...)                                                              \
    __VA_ARGS__ <>                                                                                 \
    /**/
/***************** end inline var *****************/

/********************* inline *********************/
#  ifndef NDEBUG
#    define LSTM_ALWAYS_INLINE inline
#    define LSTM_NOINLINE /**/
#    define LSTM_LIKELY(...) (__VA_ARGS__)
#    define LSTM_UNLIKELY(...) (__VA_ARGS__)
#  else
#    if LSTM_COMPILER_IS_MSVC
#      define LSTM_ALWAYS_INLINE inline __forceinline
#      define LSTM_NOINLINE __declspec(noinline)
#      define LSTM_LIKELY(...) (__VA_ARGS__)
#      define LSTM_UNLIKELY(...) (__VA_ARGS__)
#    elif LSTM_COMPILER_IS_Clang || LSTM_COMPILER_IS_GNU || LSTM_COMPILER_IS_AppleClang
#      define LSTM_ALWAYS_INLINE inline __attribute__((always_inline))
#      define LSTM_NOINLINE __attribute__((noinline))
#      define LSTM_LIKELY(...) __builtin_expect(!!(__VA_ARGS__), 1)
#      define LSTM_UNLIKELY(...) __builtin_expect(!!(__VA_ARGS__), 0)
#    else
#      define LSTM_ALWAYS_INLINE inline
#      define LSTM_NOINLINE /**/
#      define LSTM_LIKELY(...) (__VA_ARGS__)
#      define LSTM_UNLIKELY(...) (__VA_ARGS__)
#    endif
#  endif /* NDEBUG */
/******************* end inline *******************/

/******************* escape var *******************/
#  if LSTM_COMPILER_IS_Clang || LSTM_COMPILER_IS_GNU || LSTM_COMPILER_IS_AppleClang
#    define LSTM_ESCAPE_VAR(x) asm volatile("" : : "g"(&x) : "memory") /**/
#  else
#    define LSTM_ESCAPE_VAR(x) /**/
#  endif
/***************** end escape var *****************/

#endif

#endif
