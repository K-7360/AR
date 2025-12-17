/*
 *  Copyright (c) 2003-2010, Mark Borgerding. All rights reserved.
 *  This file is part of KISS FFT - https://github.com/mborgerding/kissfft
 *
 *  SPDX-License-Identifier: BSD-3-Clause
 *  See COPYING file for more information.
 */

#ifndef kiss_fft_log_h
#define kiss_fft_log_h

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Simplified logging for embedded - just define as no-ops in release
#define KISS_FFT_ERROR(...) ((void)0)
#define KISS_FFT_WARNING(...) ((void)0)
#define KISS_FFT_INFO(...) ((void)0)
#define KISS_FFT_DEBUG(...) ((void)0)

#endif /* kiss_fft_log_h */
