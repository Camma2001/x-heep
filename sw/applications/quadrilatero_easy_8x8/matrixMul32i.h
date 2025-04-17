// Copyright 2024 EPFL
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// Author: Danilo Cammarata

#ifndef _MATMUL64INT32_
#define _MATMUL64INT32_
// This file is not ;) automatically generated
int32_t __attribute__((section(".xheep_data_interleaved"))) matrix_A[] = {
  1,2,0x12345678,4,5,6,7,8,
  0,1,0,0,0,0,0,0,
  0,0,1,0,0,0,0,0,
  0,0,0,1,0,0,0,0,
  0,0,0,0,1,0,0,0,
  0,0,0,0,0,1,0,0,
  0,0,0,0,0,0,1,0,
  0,0,0,0,0,0,0,1
 	};

int32_t __attribute__((section(".xheep_data_interleaved"))) matrix_BT[] = {
  2,0,0,0,0,0,0,0,
  0,2,0,0,0,0,0,0,
  0,0,2,0,0,0,0,0,
  0,0,0,2,0,0,0,0,
  0,0,0,0,2,0,0,0,
  0,0,0,0,0,2,0,0,
  0,0,0,0,0,0,2,0,
  0,0,0,0,0,0,0,2
	};

int32_t __attribute__((section(".xheep_data_interleaved"))) matrix_C[] = {
  1,1,1,1,2,2,2,2,
  1,1,1,1,2,2,2,2,
  1,1,1,1,2,2,2,2,
  1,1,1,1,2,2,2,2,
  3,3,3,3,4,4,4,4,
  3,3,3,3,4,4,4,4,
  3,3,3,3,4,4,4,4,
  3,3,3,3,4,4,4,4
  };
  

int32_t __attribute__((section(".xheep_data_interleaved"))) matrix_EXP[] = {
  3,5,610839793,9,12,14,16,18,
  1,3,1,1,2,2,2,2,
  1,1,3,1,2,2,2,2,
  1,1,1,3,2,2,2,2,
  3,3,3,3,6,4,4,4,
  3,3,3,3,4,6,4,4,
  3,3,3,3,4,4,6,4,
  3,3,3,3,4,4,4,6
  };

#define SIZE 8
#endif