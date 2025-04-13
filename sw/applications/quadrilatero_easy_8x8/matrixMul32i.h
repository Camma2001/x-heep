// Copyright 2024 EPFL
// Solderpad Hardware License, Version 2.1, see LICENSE.md for details.
// SPDX-License-Identifier: Apache-2.0 WITH SHL-2.1
//
// Author: Danilo Cammarata

#ifndef _MATMUL64INT32_
#define _MATMUL64INT32_
// This file is not ;) automatically generated
int32_t __attribute__((section(".xheep_data_interleaved"))) matrix_A[] = {
  1,0,0,0,0,0,0,0,
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

int32_t __attribute__((section(".xheep_data_interleaved"))) matrix_EXP[] = {
  2,0,0,0,0,0,0,0,
  0,2,0,0,0,0,0,0,
  0,0,2,0,0,0,0,0,
  0,0,0,2,0,0,0,0,
  0,0,0,0,2,0,0,0,
  0,0,0,0,0,2,0,0,
  0,0,0,0,0,0,2,0,
  0,0,0,0,0,0,0,2
	};

#define SIZE 8
#endif