# sources.mk - Makefile definitions for the including the vecmathlib implementations
# 
# Copyright (c) 2013 Pekka Jääskeläinen / Tampere University of Technology
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# This file should be included from the Makefile.am of the target kernel
# library in case vecmathlib versions of the builtins are wanted.

include ../sources.mk

vpath %.cl @srcdir@:@srcdir@/../vecmathlib/pocl:@srcdir@/..
vpath %.c @srcdir@:@srcdir@/../vecmathlib/pocl:@srcdir@/..
vpath %.cc @srcdir@:@srcdir@/../vecmathlib/pocl:@srcdir@/..

LKERNEL_SRCS += acos.cc \
	acosh.cc 	\
	asin.cc	 	\
	asinh.cc	\
	atan.cc		\
	atanh.cc	\
	cbrt.cc		\
	ceil.cc		\
	copysign.cc	\
	cos.cc		\
	cosh.cc		\
	exp10.cc	\
	exp2.cc		\
	exp.cc		\
	expm1.cc	\
	fabs.cc		\
	fdim.cc		\
	floor.cc	\
	fma.cc		\
	fmax.cc		\
	fmin.cc		\
	fmod.cc		\
	hypot.cc	\
	ilogb_.cc	\
	isfinite.cc	\
	isinf.cc	\
	isnan.cc	\
	isnormal.cc	\
	ldexp_.cc	\
	log10.cc	\
	log1p.cc	\
	log2.cc		\
	log.cc		\
	pow.cc		\
	remainder.cc	\
	round.cc	\
	rsqrt.cc	\
	signbit.cc	\
	sin.cc		\
	sinh.cc		\
	sqrt.cc		\
	tan.cc		\
	tanh.cc		\
	trunc.cc

