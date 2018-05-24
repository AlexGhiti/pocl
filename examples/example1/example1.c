/* example1 - Simple example from OpenCL 1.0 specification, modified


   Copyright (c) 2011 Universidad Rey Juan Carlos
                 2014 Pekka Jääskeläinen

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <CL/opencl.h>
#include <poclu.h>

#define N 128

#ifdef __cplusplus
#  define CALLAPI "C"
#else 
#  define CALLAPI
#endif

extern CALLAPI int
exec_dot_product_kernel (cl_context context, cl_device_id device,
                         cl_command_queue cmd_queue, cl_program program, int n,
                         cl_float4 *srcA, cl_float4 *srcB, cl_float *dst);

int
main (int argc, char **argv)
{
  cl_float4 *srcA, *srcB;
  cl_float *dst;
  int i, err, testing_spir;

  cl_context context = NULL;
  cl_device_id device = NULL;
  cl_platform_id platform = NULL;
  cl_command_queue queue = NULL;
  cl_program program = NULL;

  err = poclu_get_any_device2 (&context, &device, &queue, &platform);
  CHECK_OPENCL_ERROR_IN ("clCreateContext");

  testing_spir = (argc > 1 && argv[1][0] == 's');

  const char *basename = "example1";
  err = poclu_load_program (context, device, basename, testing_spir, &program);
  if (err != CL_SUCCESS)
    goto FINISH;

  srcA = (cl_float4 *) malloc (N * sizeof (cl_float4));
  srcB = (cl_float4 *) malloc (N * sizeof (cl_float4));
  dst = (cl_float *) malloc (N * sizeof (cl_float));

  for (i = 0; i < N; ++i)
    {
      srcA[i].s[0] = (cl_float)i;
      srcA[i].s[1] = (cl_float)i;
      srcA[i].s[2] = (cl_float)i;
      srcA[i].s[3] = (cl_float)i;
      srcB[i].s[0] = (cl_float)i;
      srcB[i].s[1] = (cl_float)i;
      srcB[i].s[2] = (cl_float)i;
      srcB[i].s[3] = (cl_float)i;
      dst[i] = (cl_float)i;
    }

  err = 0;

  if (exec_dot_product_kernel (context, device, queue, program, N, srcA, srcB,
                               dst))
    {
      printf ("Error running the tests\n");
      err = 1;
      goto FINISH;
    }

  for (i = 0; i < 4; ++i)
    {
      printf ("(%f, %f, %f, %f) . (%f, %f, %f, %f) = %f\n",
        srcA[i].s[0], srcA[i].s[1], srcA[i].s[2], srcA[i].s[3],
        srcB[i].s[0], srcB[i].s[1], srcB[i].s[2], srcB[i].s[3],
        dst[i]);
      if (srcA[i].s[0] * srcB[i].s[0] +
          srcA[i].s[1] * srcB[i].s[1] +
          srcA[i].s[2] * srcB[i].s[2] +
          srcA[i].s[3] * srcB[i].s[3] != dst[i])
        {
          printf ("FAIL\n");
          err = 1;
          goto FINISH;
        }
    }

  printf ("OK\n");

FINISH:
  CHECK_CL_ERROR (clReleaseProgram (program));
  CHECK_CL_ERROR (clReleaseCommandQueue (queue));
  CHECK_CL_ERROR (clUnloadPlatformCompiler (platform));
  CHECK_CL_ERROR (clReleaseContext (context));

  return err;
}
