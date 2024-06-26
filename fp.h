#ifndef FPH
#define FPH

#include <stdint.h>

/* the saved state for the floating point unit, as defined at
 * http://www.intel.com/content/dam/www/public/us/en/documents/manuals/\
       64-ia-32-architectures-software-developer-manual-325462.pdf
 * This area must be 16-byte aligned.
 */
#if defined(__x86_64) || defined(__x86_64__)
struct __attribute__ ((aligned(16))) __attribute__ ((__packed__)) fxsave {
  #ifdef  __clang__
  uint8_t all[512];
  #else
  uint16_t pad1;
  uint16_t cs_ip_bits;
  uint32_t fpu_ip_bits;
  uint16_t fop;
  uint8_t  pad2;
  uint8_t  ftw;
  uint16_t fsw;
  uint16_t fcw;
  uint32_t mxcsr_mask;
  uint32_t mxcsr;
  uint16_t pad3;
  uint16_t fpu_dp_bits_high;
  uint32_t fpu_dp_bits_low;

  uint8_t padst0[6]; __float80 st0;
  uint8_t padst1[6]; __float80 st1;
  uint8_t padst2[6]; __float80 st2;
  uint8_t padst3[6]; __float80 st3;
  uint8_t padst4[6]; __float80 st4;
  uint8_t padst5[6]; __float80 st5;
  uint8_t padst6[6]; __float80 st6;
  uint8_t padst7[6]; __float80 st7;

  __float128 xmm0;
  __float128 xmm1;
  __float128 xmm2;
  __float128 xmm3;
  __float128 xmm4;
  __float128 xmm5;
  __float128 xmm6;
  __float128 xmm7;
  __float128 xmm8;
  __float128 xmm9;
  __float128 xmm10;
  __float128 xmm11;
  __float128 xmm12;
  __float128 xmm13;
  __float128 xmm14;
  __float128 xmm15;
  uint8_t pad4[48];         /* For safety.  Manual says it's 512 bytes */
  #endif
};

/* This was captured from a live FPU.  All of these bits are probably not
 * necessary, but that's a task for another day.
 */
#define FPU_INIT ((union {  struct fxsave fxsave;  uint8_t array[512];} ) \
  {.array={\
     0x7f,0x03,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x80,0x1f,0x00,0x00,0xff,0xff,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x25,0x25,0x25,0x25,0x25,0x25,0x25,0x25,\
     0x25,0x25,0x25,0x25,0x25,0x25,0x25,0x25,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0xff,0x00,0x00,0x00,0x00,0xff,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}).fxsave
#else
  #error "This only works on x86_64 for now"
#endif


#endif
