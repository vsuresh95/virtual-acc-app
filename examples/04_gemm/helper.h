#ifndef __HELPER_H__
#define __HELPER_H__

#include <stdio.h>
#include <common_helper.h>
#include <hpthread.h>
#include <gemm_hpthread_args.h>
#include <nn_token.h>

const float ERR_TH = 0.05;

void gemm(const nn_token_t* A, const nn_token_t* B, nn_token_t* C, unsigned m, unsigned n, unsigned k);

#endif // __HELPER_H__