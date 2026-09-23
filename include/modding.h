#ifndef DK64_RA_MODDING_H
#define DK64_RA_MODDING_H

/* N64Recomp's import/callback section contract. Keep in sync with its mod template. */
#define RECOMP_IMPORT(dependency, function) \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"") \
    _Pragma("GCC diagnostic ignored \"-Wreturn-type\"") \
    __attribute__((noinline, weak, used, section(".recomp_import." dependency))) function {} \
    _Pragma("GCC diagnostic pop")

#define RECOMP_CALLBACK(dependency, event) \
    __attribute__((retain, section(".recomp_callback." dependency ":" #event)))

#define RECOMP_HOOK_RETURN(func) \
    __attribute__((retain, section(".recomp_hook_return." func)))

#endif
