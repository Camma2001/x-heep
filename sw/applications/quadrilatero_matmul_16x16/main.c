// Copyright 2024 EPFL
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// Author: Danilo Cammarata

/*Variable Data Type*/
// Supported types: int32_t (0), float (1), int8_t (2), int16_t (3)
#define TYPE 0

/* Output tile size */
// Supported values: 16 (4x4), 64 (8x8).
#define OUTPUT_TILE_SIZE 64

/*Register Length*/
// Supported values: 128 (4x4), 256 (8x8)
#define RLEN 128

/* By default, printfs are deactivated. */
#define PRINTF_IN_FPGA  0
#define PRINTF_IN_SIM   1

/* VCD Files generation */
// Supported Values: 0 (No), 1 (Yes)
// #define VCD_ENABLE 0

// ************************************************************************************************************
// *****************************                                                  *****************************
// *****************************            DO NOT TOUCH LINES BELOW !            *****************************
// *****************************                                                  *****************************
// ************************************************************************************************************

/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include "csr.h"
#include "x-heep.h"
#include "gpio.h"
// #include "vcd_util.h"

/* Define Datatype and set of data */
#if TYPE == 0
    #include "matrixMul32i.h"
    #define FS_INITIAL 0x0
    #define SIMD_FACTOR 1
    #define HEAD_LINE "mmasa.w"
    #define SIMD_SHIFT 2
    typedef int32_t DATA_IN_t ;
    typedef int32_t DATA_OUT_t;
#elif TYPE == 1
    #include "matrixMul32f.h"
    #define FS_INITIAL 0x1
    #define SIMD_FACTOR 1
    #define HEAD_LINE "fmmacc.s"
    #define SIMD_SHIFT 2
    typedef float DATA_IN_t ;
    typedef float DATA_OUT_t;
#elif TYPE == 2
    #include "matrixMul8i.h"
    #define FS_INITIAL 0x0
    #define SIMD_FACTOR 4
    #define HEAD_LINE "mmaqa.b"
    #define SIMD_SHIFT 0
    typedef int8_t  DATA_IN_t ;
    typedef int32_t DATA_OUT_t;
#elif TYPE == 3
    #include "matrixMul16i.h"
    #define FS_INITIAL 0x0
    #define SIMD_FACTOR 2
    #define HEAD_LINE "mmada.h"
    #define SIMD_SHIFT 1
    typedef int16_t DATA_IN_t ;
    typedef int32_t DATA_OUT_t;
#else    
#endif

/* Declare functions and global variables */
DATA_OUT_t __attribute__((section(".xheep_data_interleaved"))) matrix_C[SIZE*SIZE]; 
void __attribute__ ((noinline)) matrixMul_4x4(DATA_IN_t* addrA,DATA_IN_t* addrB,DATA_OUT_t* addrC, int K, int N, int M, int shift);
void __attribute__ ((noinline)) matrixMul_8x8(DATA_IN_t* addrA,DATA_IN_t* addrB,DATA_OUT_t* addrC, int K, int N, int M, int shift);
void __attribute__ ((noinline)) matrixMulBigRF_8x8(DATA_IN_t* addrA,DATA_IN_t* addrB,DATA_OUT_t* addrC, int K, int N, int M, int shift);
void __attribute__ ((noinline)) matrixMul_CPU(DATA_IN_t* addrA,DATA_IN_t* addrB,DATA_OUT_t* addrC, int K, int N, int M, int shift);
void __attribute__ ((noinline))  matrixMul_16x16(DATA_IN_t* addrA,DATA_IN_t* addrB,DATA_OUT_t* addrC, int K, int N, int M, int shift);
void print_matrix(DATA_OUT_t* matrix, int K, int N);
int float_condition(int index);
int int_condition(int index);
uint32_t check_results(int K, int N, int M);


/* Select print mode */

#if TARGET_SIM && PRINTF_IN_SIM
        #define PRINTF(fmt, ...)    printf(fmt, ## __VA_ARGS__)
#elif TARGET_PYNQ_Z2 && PRINTF_IN_FPGA
    #define PRINTF(fmt, ...)    printf(fmt, ## __VA_ARGS__)
#else
    #define PRINTF(...)
#endif

/* Select kernel */
#if OUTPUT_TILE_SIZE == 16
    #define MATRIX_MUL(addrA, addrB, addrC, K, N, M, shift) matrixMul_4x4((DATA_IN_t*)addrA, (DATA_IN_t*)addrB, (DATA_OUT_t*)addrC, (int) K, (int) N, (int) M, (int) shift)
#elif OUTPUT_TILE_SIZE == 64
    #define MATRIX_MUL(addrA, addrB, addrC, K, N, M, shift) matrixMul_8x8((DATA_IN_t*)addrA, (DATA_IN_t*)addrB, (DATA_OUT_t*)addrC, (int) K, (int) N, (int) M, (int) shift)
#elif OUTPUT_TILE_SIZE == 0
    #define MATRIX_MUL(addrA, addrB, addrC, K, N, M, shift) matrixMul_CPU((DATA_IN_t*)addrA, (DATA_IN_t*)addrB, (DATA_OUT_t*)addrC, (int) K, (int) N, (int) M, (int) shift)
#else
    #define MATRIX_MUL(addrA, addrB, addrC, K, N, M, shift)
#endif


/* Select check condition */
#if FS_INITIAL == 0x1
    #define CHECK_CONDITION(index) float_condition((int) index)
#elif FS_INITIAL == 0x0
    #define CHECK_CONDITION(index) int_condition  ((int) index)
#else 
#endif

/* VCD Functions */
#if VCD_ENABLE == 1
    #define VCD_START()  vcd_init(); vcd_enable()
    #define VCD_STOP()   vcd_disable()
#else
    #define VCD_START()  
    #define VCD_STOP()  
#endif

/* Matrices */
#define MAT_A       matrix_A
#define MAT_B       matrix_BT
#define MAT_C       matrix_C
#define MAT_EXP     matrix_EXP

#define MACC(HEAD,__mat1__, __mat2__, __mat3__) HEAD "  m" #__mat1__", m"#__mat2__", m"#__mat3__
// -------------------------------------------------------------------------------------------------------------------------------------


int main()
{
    uint32_t errors = 0;
    unsigned int cycles;

    // Save the address of the matrices
    DATA_IN_t*  addrA = MAT_A; 
    DATA_IN_t*  addrB = MAT_B; 
    DATA_OUT_t* addrC = MAT_C; 

    int K_size = SIZE/SIMD_FACTOR;
    int N_size = SIZE            ;
    int M_size = SIZE            ;

    //enable FP operations
    CSR_SET_BITS(CSR_REG_MSTATUS, (FS_INITIAL << 13)); 

    //start mcycle csr
    CSR_CLEAR_BITS(CSR_REG_MCOUNTINHIBIT, 0x1);
    CSR_WRITE(CSR_REG_MCYCLE, 0);

    //execute the kernel
    // vcd_init();
    // vcd_enable();
    VCD_START();
    matrixMul_16x16(addrA,addrB,addrC,K_size,N_size,M_size,SIMD_SHIFT);
    VCD_STOP();
    // vcd_disable();

    //read mcycle csr
    CSR_READ(CSR_REG_MCYCLE, &cycles);

    //check results
    errors = check_results(K_size,N_size,M_size);

    PRINTF("program finished with %d errors and %d cycles\n\r", errors, cycles);
    // PRINTF("MATRIX C:\n\r");
    // print_matrix(addrC, M_size, N_size);
    // PRINTF("MATRIX EXP:\n\r");
    // print_matrix((DATA_OUT_t *) MAT_EXP, M_size, N_size);
    return errors;
}


// -------------------------------------------------------------------------------------------------------------------------------------


// Output tile size: 4x4
void  __attribute__ ((noinline))  matrixMul_4x4(DATA_IN_t* addrA,DATA_IN_t* addrB,DATA_OUT_t* addrC, int K, int N, int M, int shift)
{
    asm volatile("addi	sp, sp, -0x18           "                            );   // 
    asm volatile("sw	s3 , 0x18(sp)           "                            );   // 
    asm volatile("sw	s4 , 0x14(sp)           "                            );   // 
    asm volatile("sw	s5 , 0x10(sp)           "                            );   // 
    asm volatile("sw	s7 , 0x0c(sp)           "                            );   // 
    asm volatile("sw	s9 , 0x08(sp)           "                            );   // 
    asm volatile("sw	s10, 0x04(sp)           "                            );   // 
    asm volatile("sw	s11, 0x00(sp)           "                            );   // 

    //--------------------------------------------------------------------------------
    asm volatile("addi    t5,x0, 0              "                            );   // t5  = m0 =0;
    asm volatile("slli    s4,%0, 2              " :: "r" (N)                 );   // s4  = N*4;
    asm volatile("sll     s7,%0,%1              " :: "r" (N),"r" (shift)     );   // s7  = N* 2**SIMD_SHIFT;
    asm volatile("slli    s3,%0, 2              " :: "r" (K)                 );   // s3  = K*4;
    asm volatile("loopM_start4x4:               "                            );   // while(m0<M) {
    asm volatile("mul     t1,s4,t5              "                            );   // t1  = N*4*m0;
    asm volatile("addi    t4,x0, 0              "                            );   // t4  = n0 =0;
    asm volatile("mul     t0,s3,t5              "                            );   // t0  = K*4*m0;
    asm volatile("loopN_start4x4:               "                            );   // while(n0<N) {
    asm volatile("slli    s5,t4, 2              "                            );   // s5  = n0*4;
    asm volatile("add     s11,%0,t1             " :: "r" (addrC)             );   // s11 = startAddrC = addrC + N*4*m0
    asm volatile("add     s11,s11,s5            "                            );   // s11 = startAddrC += n0*4
    asm volatile("mzero   m2                    "                            );   // m2  = 0
    asm volatile("mul     t2,s3,t4              "                            );   // t2  = K*4*n0;
    asm volatile("addi    t3,x0, 0              "                            );   // t3  = k0 =0;
    asm volatile("loopK_start4x4:               "                            );   // while(k0<K) { 
    asm volatile("slli    t6,t3, 2              "                            );   // t6  = k0*4;
    asm volatile("add     s10,%0,t6             " :: "r" (addrB)             );   // s10 = startAddrB = addrB + k0*4
    asm volatile("add     s10,s10,t2            "                            );   // s10 = startAddrB += K*4*n0;  
    asm volatile("mld.w   m1, (s10), s7         "                            );   // m1  = B[s1]  
    asm volatile("add     s9,%0,t0              " :: "r" (addrA)             );   // s9  = startAddrA = addrA + K*4*m0
    asm volatile("add     s9,s9,t6              "                            );   // s9  = startAddrA += k0*4
    asm volatile("mld.w   m0, (s9) , s3         "                            );   // m1  = B[s9] 
    asm volatile("addi    t3,t3, 4              "                            );   // k0+=4;
    asm volatile(MACC(HEAD_LINE,2,1,0)                            );   // m2 +=  m1 * m0
    asm volatile("blt     t3, %0, loopK_start4x4" :: "r" (K)                 );   // endwhile(k0<K)
    asm volatile("addi    t4,t4, 4              "                            );   // n0+=4;
    asm volatile("mst.w   m2, (s11), s4         "                            );   // m2 -> (s11)
    asm volatile("blt     t4, %0, loopN_start4x4" :: "r" (N)                 );   // endwhile(n0<N)
    asm volatile("addi    t5,t5, 4              "                            );   // m0+=4;
    asm volatile("blt     t5, %0, loopM_start4x4" :: "r" (M)                 );   // endwhile(m0<M)
    //--------------------------------------------------------------------------------

    asm volatile("lw	s3 , 0x18(sp)           "                            );   // 
    asm volatile("lw	s4 , 0x14(sp)           "                            );   // 
    asm volatile("lw	s5 , 0x10(sp)           "                            );   // 
    asm volatile("lw	s7 , 0x0c(sp)           "                            );   // 
    asm volatile("lw	s9 , 0x08(sp)           "                            );   // 
    asm volatile("lw	s10, 0x04(sp)           "                            );   // 
    asm volatile("lw	s11, 0x00(sp)           "                            );   // 
    asm volatile("addi	sp, sp, 0x18            "                            );   //
}


// Output tile size: 8x8
void  __attribute__ ((noinline))  matrixMul_8x8(DATA_IN_t* addrA,DATA_IN_t* addrB,DATA_OUT_t* addrC, int K, int N, int M, int shift)
{

    asm volatile("addi	sp, sp, -0x30           "                            );   // 
    asm volatile("sw	s0 , 0x2c(sp)           "                            );   // 
    asm volatile("sw	s1 , 0x28(sp)           "                            );   // 
    asm volatile("sw	s2 , 0x24(sp)           "                            );   // 
    asm volatile("sw	s3 , 0x20(sp)           "                            );   // 
    asm volatile("sw	s4 , 0x1c(sp)           "                            );   // 
    asm volatile("sw	s5 , 0x18(sp)           "                            );   // 
    asm volatile("sw	s6 , 0x14(sp)           "                            );   // 
    asm volatile("sw	s7 , 0x10(sp)           "                            );   // 
    asm volatile("sw	s8 , 0x0c(sp)           "                            );   // 
    asm volatile("sw	s9 , 0x08(sp)           "                            );   // 
    asm volatile("sw	s10, 0x04(sp)           "                            );   // 
    asm volatile("sw	s11, 0x00(sp)           "                            );   // 

    //--------------------------------------------------------------------------------
    // asm volatile("addi    a7,x0, 4              "                            );   // a7  = WIDTH;
    asm volatile("sll     a6,%0,%1              " :: "r" (N),"r" (shift)     );   // a6  = N* 2**SIMD_SHIFT
    asm volatile("addi    t0,x0, 0              "                            );   // t0  = m0 =0;
    asm volatile("slli    s3,%0, 2              " :: "r" (K)                 );   // s3  = K*4;
    asm volatile("slli    s4,%0, 2              " :: "r" (N)                 );   // s4  = N*4;

    asm volatile("loopM_start8x8:               "                            );   // while(m0<M) {
    asm volatile("addi    t1,x0, 0              "                            );   // t1   = n0 =0
    asm volatile("addi    t3,t0,4               "                            );   // t3   = m0+WIDTH
    asm volatile("mul     s1,s3,t0              "                            );   // s1   = K*4*m0
    asm volatile("mul     s2,s3,t3              "                            );   // s2   = K*4*(m0+WIDTH)
    asm volatile("mul     s0,s4,t0              "                            );   // s0   = N*4*m0;
    asm volatile("mul     s10,s4,t3             "                            );   // s10  = N*4*(m0+WIDTH)
    asm volatile("add     s1,%0,s1              " :: "r" (addrA)             );   // s1   = startAddrA0  = addrA + K*4*m0 
    asm volatile("add     s2,%0,s2              " :: "r" (addrA)             );   // s2   = startAddrA1  = addrA + K*4*(m0+WIDTH)
    asm volatile("add     s0,%0,s0              " :: "r" (addrC)             );   // s0   = startAddrC0x = addrC + N*4*m0
    asm volatile("add     s10,%0,s10            " :: "r" (addrC)             );   // s10  = startAddrC1x = addrC + N*4*(m0+WIDTH)

    asm volatile("loopN_start8x8:               "                            );   // while(n0<N) {
    asm volatile("addi    t4,t1,4               "                            );   // t4  = n0+WIDTH;
    asm volatile("addi    t2,x0,16              "                            );   // t2  = k0 = 16;
    asm volatile("slli    t5,t1, 2              "                            );   // t5  = n0*4;
    asm volatile("mld.w   m0, (s1) , s3         "                            );   // m0  = A[s1] 
    asm volatile("mzero   m4                    "                            );   // m4  = 0;
    asm volatile("mul     s9,s3,t1              "                            );   // s9  = K*4*n0;
    asm volatile("add     s9 ,%0,s9             " :: "r" (addrB)             );   // s9  = startAddrB0 = addrB + K*4*n0 
    asm volatile("mld.w   m1, (s9) , a6         "                            );   // m1  = B[s9]
    asm volatile("mzero   m6                    "                            );   // m6  = 0
    asm volatile("mul     s11,s3,t4             "                            );   // s11 = K*4*(n0+WIDTH);
    asm volatile(MACC(HEAD_LINE,4,1,0)                                                 );   // m4 += m1 * m0
    asm volatile("mld.w   m2, (s2) , s3         "                            );   // m2  = A[s2]
    asm volatile("mzero   m5                    "                            );   // m5  = 0
    asm volatile("add     s11,%0,s11            " :: "r" (addrB)             );   // s11 = startAddrB1 = addrB + K*4*(n0+WIDTH)
    asm volatile(MACC(HEAD_LINE,6,1,2)                                                 );   // m6 +=  m1 * m2
    asm volatile("mld.w   m3, (s11), a6         "                            );   // m3  = B[s11]
    asm volatile("mzero   m7                    "                            );   // m7  = 0

    asm volatile("loopK_start8x8:               "                            );   // while(k0*4<K*4) { 
    asm volatile("add     s6 ,s1 ,t2            "                            );   // s6  = startAddrA0 += k0*4
    asm volatile(MACC(HEAD_LINE,5,3,0)                                                 );   // m5 +=  m3 * m0
    asm volatile("mld.w   m0, (s6) , s3         "                            );   // m0  = A[s1]
    asm volatile("add     s7 ,s9 ,t2            "                            );   // s7  = startAddrB0 += k0*4
    asm volatile("add     s5 ,s2 ,t2            "                            );   // s5  = startAddrA1 += k0*4
    asm volatile(MACC(HEAD_LINE,7,3,2)                                                 );   // m7 += m3 * m2
    asm volatile("mld.w   m1, (s7) , a6         "                            );   // m1  = B[s7]
    asm volatile("add     s8,s11,t2             "                            );   // s8  = startAddrB1 += k0*4
    asm volatile("addi    t2,t2,16              "                            );   // t2  = k0*4 += 4*4;
    asm volatile(MACC(HEAD_LINE,4,1,0)                                                 );   // m4 += m1 * m0 
    asm volatile("mld.w   m2, (s5) , s3         "                            );   // m2  = A[s5]
    asm volatile("mld.w   m3, (s8) , a6         "                            );   // m3  = B[s8]
    asm volatile(MACC(HEAD_LINE,6,1,2)                                                 );   // m6 += m1 * m2
    asm volatile("blt     t2, s3, loopK_start8x8"                            );   // endwhile(k0*4<K*4)

    asm volatile("add     s6,t5,s0              "                            );   // s6  = startAddrC00 += n0*4
    asm volatile("mst.w   m4, (s6) , s4         "                            );   // m4  -> (s6) 
    asm volatile(MACC(HEAD_LINE,5,3,0)                                                 );   // m5 += m3 * m0
    asm volatile("add     s7,t5,s10             "                            );   // s7  = startAddrC10 += n0*4
    asm volatile("mst.w   m6, (s7) , s4         "                            );   // m6  -> (s7)
    asm volatile(MACC(HEAD_LINE,7,3,2)                                                 );   // m7 += m3 * m2
    asm volatile("slli    t6,t4, 2              "                            );   // t6  = (n0+WIDTH)*4;
    asm volatile("add     s5,t6,s0              "                            );   // s5  = startAddrC01 += (n0+WIDTH)*4
    asm volatile("mst.w   m5, (s5) , s4         "                            );   // m5  -> (s5)
    asm volatile("addi    t1,t1, 8              "                            );   // t1 = n0+=2*WIDTH;
    asm volatile("add     s8,t6,s10             "                            );   // s8  = startAddrC11 += (n0+WIDTH)*4
    asm volatile("mst.w   m7, (s8) , s4         "                            );   // m7  -> (s8) 
    asm volatile("blt     t1, %0, loopN_start8x8" :: "r" (N)                 );   // endwhile(n0<N)
    
    asm volatile("add     t0,t0, 8              "                            );   // t0 = m0 +=2*WIDTH;
    asm volatile("blt     t0, %0, loopM_start8x8" :: "r" (M)                 );   // endwhile(m0<M)
  //--------------------------------------------------------------------------------

    asm volatile("lw	s0 , 0x2c(sp)           "                            );   // 
    asm volatile("lw	s1 , 0x28(sp)           "                            );   // 
    asm volatile("lw	s2 , 0x24(sp)           "                            );   // 
    asm volatile("lw	s3 , 0x20(sp)           "                            );   // 
    asm volatile("lw	s4 , 0x1c(sp)           "                            );   // 
    asm volatile("lw	s5 , 0x18(sp)           "                            );   // 
    asm volatile("lw	s6 , 0x14(sp)           "                            );   // 
    asm volatile("lw	s7 , 0x10(sp)           "                            );   // 
    asm volatile("lw	s8 , 0x0c(sp)           "                            );   // 
    asm volatile("lw	s9 , 0x08(sp)           "                            );   // 
    asm volatile("lw	s10, 0x04(sp)           "                            );   // 
    asm volatile("lw	s11, 0x00(sp)           "                            );   // 
    asm volatile("addi	sp, sp, 0x30            "                            );   //
}

// Output tile size: 8x8 only for RLEN = 256
// Naive C code
// for(int m = 0; m < M; m+= 8){
//     for(int n = 0; n < N; n += 8){
//         mzero m0;
//         for (int k = 0; k < K; k+= 8){
//             mld.w m1, (addrA + m*4*K + 4*k), 4*K; //load matrix A
//             mld.w m2, (addrB + n*4*K + k*4), 4*N //load matrix B
//             MACC(m0, m1, m2);
//         }
//         mst.w m0, (addrC + m*4*N + n*4), 4*N; //store matrix C
//     }
// }
// more optimized C code using loop unrolling in the K loop
// for(int m = 0; m < M; m+= 8){
//     for(int n = 0; n < N; n += 8){
//         mzero m0;
//         for (int k = 0; k < K; k+= 16){
//             mld.w m1, (addrA + m*4*K + 4*k), 4*K; //load matrix A
//             mld.w m2, (addrB + n*4*K + k*4), 4*N; //load matrix B
//             MACC(m0, m1, m2);
//             mld.w m3, (addrA + m*4*K + 4*(k+8)), 4*K
//             mld.w m4, (addrB + n*4*K + (k+8)*4), 4*N;
//             MACC(m0, m3,m4);
//         }
//         mst.w m0, (addrC + m*4*N + n*4), 4*N; //store matrix C
        
//     }
// }
// even more optimized 
// for(int m = 0; m < M; m+= 8){
//  for(int n = 0; n < N; n += 8){
//     asm volatile("mzero m0");
//     asm volatile("mld.w m1, (addrA + m*4*K), 4*K"); //load matrix A
//     asm volatile("mld.w m2, (addrB + n*4*K), 4*N"); //load matrix B
//     asm volatile("MACC(m0, m1, m2)");
//     for(int k = 8; k < K; k+= 24){
//         asm volatile("mld.w m3, (addrA + m*4*K + 4*k), 4*K");
//         asm volatile("mld.w m4, (addrB + n*4*K + k*4), 4*N");
//         asm volatile("MACC(m0, m3,m4)");
//         asm volatile("mld.w m5, (addrA + m*4*K + 4*(k+8)), 4*K");
//         asm volatile("mld.w m6, (addrB + n*4*K + (k+8)*4), 4*N");
//         asm volatile("MACC(m0, m5,m6)");
//         asm volatile("mld.w m7, (addrA + m*4*K + 4*(k+16)), 4*K");
//         asm volatile("mld.w m1, (addrB + n*4*K + (k+16)*4), 4*N");
//         asm volatile("MACC(m0, m7,m1)");      
//     }   
//     asm volatile("mld.w m2, (addrA + m*4*K + 4*(K-8)), 4*K");
//     asm volatile("mld.w m3, (addrB + n*4*K + (K-8)*4), 4*N");
//     asm volatile("MACC(m0, m2,m3)");
//     asm volatile("mst.w m0, (addrC + m*4*N + n*4), 4*N"); //store matrix C   
//     }
// }

// //unrolling the m/n loops -> way to go
/*void __attribute__ ((noinline))  matrixMul_16x16_C(DATA_IN_t* addrA,DATA_IN_t* addrB,DATA_OUT_t* addrC, int K, int N, int M, int shift){
  uint32_t K_4 = K*4;
  uint32_t N_4 = N*4;
  for(int m = 0; m < M; m+= 16){
      for(int n = 0; n < N; n+=16){
          asm volatile("mzero m0"); //m0 = C00
          asm volatile("mzero m1"); //m1 = C01
          asm volatile("mzero m2"); //m2 = C10
          asm volatile("mzero m3"); //m3 = C11
          for(int k = 0; k < K; k+=8){
              //compute C00
              asm volatile("mld.w m4, (addrA + m*4*K + 4*k), 4*K");
              asm volatile("mld.w m5, (addrB + n*4*K + 4*k), 4*N");
              asm volatile("MACC(m0, m4, m5)");
              //compute C01
              asm volatile("mld.w m7, (addrB + (n+8)*4*K + 4*k), 4*N");
              asm volatile("MACC(m1, m4, m7)");
              //compute C10
              asm volatile("mld.w m6, (addrA + (m+8)*4*K + 4*k), 4*K");
              asm volatile("MACC(m2, m6, m5)");
              //compute C11
              asm volatile("MACC(m3, m6, m7)");
          }
          //store C00
          asm volatile("mst.w m0, (addrC + m*4*N + n*4), 4*N");
          //store C01
          asm volatile("mst.w m1, (addrC + m*4*N + (n+8)*4), 4*N");
          //store C10
          asm volatile("mst.w m2, (addrC + (m+8)*4*N + n*4), 4*N");
          //store C11
          asm volatile("mst.w m3, (addrC + (m+8)*4*N + (n+8)*4), 4*N");

      }
  }
} */
void  __attribute__ ((noinline))  matrixMul_16x16(DATA_IN_t* addrA,DATA_IN_t* addrB,DATA_OUT_t* addrC, int K, int N, int M, int shift)
{
    asm volatile("addi	sp, sp, -0x30           "                            );   // 
    asm volatile("sw	s0 , 0x2c(sp)           "                            );   // 
    asm volatile("sw	s1 , 0x28(sp)           "                            );   // 
    asm volatile("sw	s2 , 0x24(sp)           "                            );   // 
    asm volatile("sw	s3 , 0x20(sp)           "                            );   // 
    asm volatile("sw	s4 , 0x1c(sp)           "                            );   // 
    asm volatile("sw	s5 , 0x18(sp)           "                            );   // 
    asm volatile("sw	s6 , 0x14(sp)           "                            );   // 
    asm volatile("sw	s7 , 0x10(sp)           "                            );   // 
    asm volatile("sw	s8 , 0x0c(sp)           "                            );   // 
    asm volatile("sw	s9 , 0x08(sp)           "                            );   // 
    asm volatile("sw	s10, 0x04(sp)           "                            );   // 
    asm volatile("sw	s11, 0x00(sp)           "                            );   // 

    //--------------------------------------------------------------------------------
    // asm volatile("addi    a7,x0, 4              "                            );   // a7  = WIDTH;
    asm volatile("sll     a6,%0,%1              " :: "r" (N),"r" (shift)     );   // a6  = N* 2**SIMD_SHIFT
    asm volatile("addi    t0,x0, 0              "                            );   // t0  = m0 =0;
    asm volatile("slli    s3,%0, 2              " :: "r" (K)                 );   // s3  = K*4;
    asm volatile("slli    s4,%0, 2              " :: "r" (N)                 );   // s4  = N*4;

    asm volatile("loopM_start16x16:               "                            );   // while(m0<M) {
    asm volatile("addi    t1,x0, 0              "                            );   // t1   = n0 =0
    asm volatile("addi    t3,t0,8               "                            );   // t3   = m0+WIDTH
    asm volatile("mul     s1,s3,t0              "                            );   // s1   = K*4*m0
    asm volatile("mul     s2,s3,t3              "                            );   // s2   = K*4*(m0+WIDTH)
    asm volatile("mul     s0,s4,t0              "                            );   // s0   = N*4*m0;
    asm volatile("mul     s10,s4,t3             "                            );   // s10  = N*4*(m0+WIDTH)
    asm volatile("add     s1,%0,s1              " :: "r" (addrA)             );   // s1   = startAddrA0  = addrA + K*4*m0 
    asm volatile("add     s2,%0,s2              " :: "r" (addrA)             );   // s2   = startAddrA1  = addrA + K*4*(m0+WIDTH)
    asm volatile("add     s0,%0,s0              " :: "r" (addrC)             );   // s0   = startAddrC0x = addrC + N*4*m0
    asm volatile("add     s10,%0,s10            " :: "r" (addrC)             );   // s10  = startAddrC1x = addrC + N*4*(m0+WIDTH)

    asm volatile("loopN_start16x16:               "                            );   // while(n0<N) {
    asm volatile("addi    t4,t1,8               "                            );   // t4  = n0+WIDTH;
    asm volatile("addi    t2,x0,16              "                            );   // t2  = k0 = 16;
    asm volatile("slli    t5,t1, 2              "                            );   // t5  = n0*4;
    asm volatile("mld.w   m0, (s1) , s3         "                            );   // m0  = A[s1] 
    asm volatile("mzero   m4                    "                            );   // m4  = 0;
    asm volatile("mul     s9,s3,t1              "                            );   // s9  = K*4*n0;
    asm volatile("add     s9 ,%0,s9             " :: "r" (addrB)             );   // s9  = startAddrB0 = addrB + K*4*n0 
    asm volatile("mld.w   m1, (s9) , a6         "                            );   // m1  = B[s9]
    asm volatile("mzero   m6                    "                            );   // m6  = 0
    asm volatile("mul     s11,s3,t4             "                            );   // s11 = K*4*(n0+WIDTH);
    asm volatile(MACC(HEAD_LINE,4,1,0)                                                 );   // m4 += m1 * m0
    asm volatile("mld.w   m2, (s2) , s3         "                            );   // m2  = A[s2]
    asm volatile("mzero   m5                    "                            );   // m5  = 0
    asm volatile("add     s11,%0,s11            " :: "r" (addrB)             );   // s11 = startAddrB1 = addrB + K*4*(n0+WIDTH)
    asm volatile(MACC(HEAD_LINE,6,1,2)                                                 );   // m6 +=  m1 * m2
    asm volatile("mld.w   m3, (s11), a6         "                            );   // m3  = B[s11]
    asm volatile("mzero   m7                    "                            );   // m7  = 0

    asm volatile("loopK_start16x16:               "                            );   // while(k0*4<K*4) { 
    asm volatile("add     s6 ,s1 ,t2            "                            );   // s6  = startAddrA0 += k0*4
    asm volatile(MACC(HEAD_LINE,5,3,0)                                                 );   // m5 +=  m3 * m0
    asm volatile("mld.w   m0, (s6) , s3         "                            );   // m0  = A[s1]
    asm volatile("add     s7 ,s9 ,t2            "                            );   // s7  = startAddrB0 += k0*4
    asm volatile("add     s5 ,s2 ,t2            "                            );   // s5  = startAddrA1 += k0*4
    asm volatile(MACC(HEAD_LINE,7,3,2)                                                 );   // m7 += m3 * m2
    asm volatile("mld.w   m1, (s7) , a6         "                            );   // m1  = B[s7]
    asm volatile("add     s8,s11,t2             "                            );   // s8  = startAddrB1 += k0*4
    asm volatile("addi    t2,t2,16              "                            );   // t2  = k0*4 += 4*4;
    asm volatile(MACC(HEAD_LINE,4,1,0)                                                 );   // m4 += m1 * m0 
    asm volatile("mld.w   m2, (s5) , s3         "                            );   // m2  = A[s5]
    asm volatile("mld.w   m3, (s8) , a6         "                            );   // m3  = B[s8]
    asm volatile(MACC(HEAD_LINE,6,1,2)                                                 );   // m6 += m1 * m2
    asm volatile("blt     t2, s3, loopK_start16x16"                            );   // endwhile(k0*4<K*4)

    asm volatile("add     s6,t5,s0              "                            );   // s6  = startAddrC00 += n0*4
    asm volatile("mst.w   m4, (s6) , s4         "                            );   // m4  -> (s6) 
    asm volatile(MACC(HEAD_LINE,5,3,0)                                                 );   // m5 += m3 * m0
    asm volatile("add     s7,t5,s10             "                            );   // s7  = startAddrC10 += n0*4
    asm volatile("mst.w   m6, (s7) , s4         "                            );   // m6  -> (s7)
    asm volatile(MACC(HEAD_LINE,7,3,2)                                                 );   // m7 += m3 * m2
    asm volatile("slli    t6,t4, 2              "                            );   // t6  = (n0+WIDTH)*4;
    asm volatile("add     s5,t6,s0              "                            );   // s5  = startAddrC01 += (n0+WIDTH)*4
    asm volatile("mst.w   m5, (s5) , s4         "                            );   // m5  -> (s5)
    asm volatile("addi    t1,t1, 16              "                            );   // t1 = n0+=2*WIDTH;
    asm volatile("add     s8,t6,s10             "                            );   // s8  = startAddrC11 += (n0+WIDTH)*4
    asm volatile("mst.w   m7, (s8) , s4         "                            );   // m7  -> (s8) 
    asm volatile("blt     t1, %0, loopN_start16x16" :: "r" (N)                 );   // endwhile(n0<N)
    
    asm volatile("add     t0,t0, 16              "                             );   // t0 = m0 +=2*WIDTH;
    asm volatile("blt     t0, %0, loopM_start16x16" :: "r" (M)                 );   // endwhile(m0<M)
  //--------------------------------------------------------------------------------

  //--------------------------------------------------------------------------------



    asm volatile("lw	s0 , 0x2c(sp)           "                            );   // 
    asm volatile("lw	s1 , 0x28(sp)           "                            );   // 
    asm volatile("lw	s2 , 0x24(sp)           "                            );   // 
    asm volatile("lw	s3 , 0x20(sp)           "                            );   // 
    asm volatile("lw	s4 , 0x1c(sp)           "                            );   // 
    asm volatile("lw	s5 , 0x18(sp)           "                            );   // 
    asm volatile("lw	s6 , 0x14(sp)           "                            );   // 
    asm volatile("lw	s7 , 0x10(sp)           "                            );   // 
    asm volatile("lw	s8 , 0x0c(sp)           "                            );   // 
    asm volatile("lw	s9 , 0x08(sp)           "                            );   // 
    asm volatile("lw	s10, 0x04(sp)           "                            );   // 
    asm volatile("lw	s11, 0x00(sp)           "                            );   // 
    asm volatile("addi	sp, sp, 0x30            "                            );   //

} 






void  __attribute__ ((noinline))  matrixMulbigRF_8x8(DATA_IN_t* addrA,DATA_IN_t* addrB,DATA_OUT_t* addrC, int K, int N, int M, int shift)
{
    asm volatile("addi	sp, sp, -0x18           "                            );   // 
    asm volatile("sw	s3 , 0x18(sp)           "                            );   // 
    asm volatile("sw	s4 , 0x14(sp)           "                            );   // 
    asm volatile("sw	s5 , 0x10(sp)           "                            );   // 
    asm volatile("sw	s7 , 0x0c(sp)           "                            );   // 
    asm volatile("sw	s9 , 0x08(sp)           "                            );   // 
    asm volatile("sw	s10, 0x04(sp)           "                            );   // 
    asm volatile("sw	s11, 0x00(sp)           "                            );   // 

    //--------------------------------------------------------------------------------
    asm volatile("addi    t5,x0, 0              "                            );   // t5  = m0 =0;
    asm volatile("slli    s4,%0, 2              " :: "r" (N)                 );   // s4  = N*4;
    asm volatile("sll     s7,%0,%1              " :: "r" (N),"r" (shift)     );   // s7  = N* 2**SIMD_SHIFT;
    asm volatile("slli    s3,%0, 2              " :: "r" (K)                 );   // s3  = K*4;
    asm volatile("loopM_start8x8big:               "                            );   // while(m0<M) {
    asm volatile("mul     t1,s4,t5              "                            );   // t1  = N*4*m0;
    asm volatile("addi    t4,x0, 0              "                            );   // t4  = n0 =0;
    asm volatile("mul     t0,s3,t5              "                            );   // t0  = K*4*m0;
    asm volatile("loopN_start8x8big:               "                            );   // while(n0<N) {
    asm volatile("slli    s5,t4, 3              "                            );   // s5  = n0*8;
    asm volatile("add     s11,%0,t1             " :: "r" (addrC)             );   // s11 = startAddrC = addrC + N*4*m0
    asm volatile("add     s11,s11,s5            "                            );   // s11 = startAddrC += n0*4 
    //set matrix C to zero
    asm volatile("mzero   m0                    "                            );   // m0  = 0
    //load matrix A
    asm volatile("add     s9,%0,t0              " :: "r" (addrA)             );   // s9  = startAddrA = addrA + K*4*m0
    asm volatile("mld.w   m1, (s9), s3          "                             );   // m1  = A[s9]  
    asm volatile("mul     t2,s3,t4              "                            );   // t2  = K*4*n0;
    //load matrix B
    asm volatile("mul     t6,s3,t4              "                            );   // t6 = n0*4*K;
    asm volatile("add     s10,%0,t6             " :: "r" (addrB)             );   // s10 = startAddrB = addrB + 4*n0*K
    asm volatile("mld.w   m2, (s10), s4         "                            );   // m2  = B[s9] 
    //begin k0 counter 
    asm volatile("addi    t3,x0, 8              "                            );   // t3  = k0 =8;
    //do first MACC
    asm volatile(MACC(HEAD_LINE,0,1,2)                                       );   // m0 +=  m1 * m2

    asm volatile("loopK_start8x8big:            "                            );   // while(k0<K) { 
    asm volatile("slli    t6,t3, 2              "                            );   // t6  = k0*4;
    //load A1
    asm volatile("add     s9,s9,t6              "                            );   // s9  = startAddrA += k0*4
    asm volatile("mld.w   m3, (s9) , s3         "                            );   // m3  = A[s9] 
    //load B1
    asm volatile("add     s10,s10,t6            "                            );   // s10 = startAddrB += k0*4;  
    asm volatile("mld.w   m4, (s10), s4         "                            );   // m4  = B[s10]  
    asm volatile(MACC(HEAD_LINE,0,3,4)                                       );   // m0 +=  m3 * m4
    //load A2
    asm volatile("add     s9,s9,t6              "                            );   // s9  = startAddrA += k0*4
    asm volatile("mld.w   m5, (s9) , s3         "                            );   // m5  = A[s9] 
    //load B2
    asm volatile("add     s10,s10,t6            "                            );   // s10 = startAddrB += k0*4;  
    asm volatile("mld.w   m6, (s10), s4         "                            );   // m6  = B[s10]  
    asm volatile(MACC(HEAD_LINE,0,5,6)                                       );   // m0 +=  m5 * m6
    //load A3
    asm volatile("add     s9,s9,t6              "                            );   // s9  = startAddrA += k0*4
    asm volatile("mld.w   m7, (s9) , s3         "                            );   // m7  = A[s9] 
    //load B3
    asm volatile("add     s10,s10,t6            "                            );   // s10 = startAddrB += k0*4;  
    asm volatile("mld.w   m1, (s10), s4         "                            );   // m1  = B[s10]  
    asm volatile(MACC(HEAD_LINE,0,7,1)                                       );   // m0 +=  m7 * m1

   
    asm volatile("addi    t3,t3, 24              "                            );   // k0+=24;
    asm volatile("blt     t3, %0, loopK_start8x8big" :: "r" (K)                 );   // endwhile(k0<K)
    //asm volatile("addi    t6,s3, -32              "                            );   // t6 = (K-8) * 4
    //load last A
    asm volatile("add    s9,s9, 32              "                            );   // s9 = startAddrA + (K-8) * 4 not sure about that, might be wrong
    asm volatile("mld.w   m2, (s9) , s3         "                            );   // m2  = A[s9] 
    //load last B
    asm volatile("add     s10,s10,32           "                            );   // s10 = startAddrB + (K-8) * 4;  
    asm volatile("mld.w   m3, (s10), s4         "                            );   // m3  = B[s10]  
    asm volatile(MACC(HEAD_LINE,0,2,3)                                       );   // m0 +=  m2 * m3

    asm volatile("addi    t4,t4, 8              "                            );   // n0+=8;
    //store matrix C
    asm volatile("mst.w   m0, (s11), s4         "                            );   // m0 -> (s11)
    asm volatile("blt     t4, %0, loopN_start8x8big" :: "r" (N)                 );   // endwhile(n0<N)
    asm volatile("addi    t5,t5, 8              "                            );   // m0+=8;
    asm volatile("blt     t5, %0, loopM_start8x8big" :: "r" (M)                 );   // endwhile(m0<M)
    //--------------------------------------------------------------------------------

    asm volatile("lw	s3 , 0x18(sp)           "                            );   // 
    asm volatile("lw	s4 , 0x14(sp)           "                            );   // 
    asm volatile("lw	s5 , 0x10(sp)           "                            );   // 
    asm volatile("lw	s7 , 0x0c(sp)           "                            );   // 
    asm volatile("lw	s9 , 0x08(sp)           "                            );   // 
    asm volatile("lw	s10, 0x04(sp)           "                            );   // 
    asm volatile("lw	s11, 0x00(sp)           "                            );   // 
    asm volatile("addi	sp, sp, 0x18            "                            );   //
}





void __attribute__ ((noinline))  matrixMul_CPU(DATA_IN_t* addrA,DATA_IN_t* addrB,DATA_OUT_t* addrC, int K, int N, int M, int shift)
{
    for(int i=0;i<M;i++){
        for(int j=0;j<N;j++){
            DATA_OUT_t acc=0;
            for(int k=0;k<K;k++){
                acc += addrA[i*K+k] * addrB[j*K+k];
            }
            addrC[i*N+j] = acc;
        }
    }
}

int float_condition(int index){
    DATA_OUT_t diff;

    diff = MAT_C[index] - MAT_EXP[index];
    diff = diff * (diff >= 0);
    return (diff > 0.001f);
}

int int_condition(int index){
    return (MAT_C[index] != MAT_EXP[index]);
}

uint32_t check_results(int K, int N, int M)
{
    // check
    int i, j;
    uint32_t err = 0;

    // Check errors
    for(i = 0; i < M; i++) {
        for(j = 0; j < N; j++) {
           if(CHECK_CONDITION(i*N+j)) {
                err ++;
                PRINTF("Error at index %d, %d, expected %x, got %x\n\r", i, j, MAT_EXP[i*N+j], MAT_C[i*N+j]);
            }
        }
    }

    return err;
}

void print_matrix(DATA_OUT_t* matrix, int K, int N)
{
    for(int i=0;i<K;i++){
        for(int j=0;j<N;j++){
            PRINTF("%d ", matrix[i*N+j]);
        }
        PRINTF("\n\r");
    }
}