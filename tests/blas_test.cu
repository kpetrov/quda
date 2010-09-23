#include <stdio.h>
#include <stdlib.h>

#include <quda_internal.h>
#include <spinor_quda.h>
#include <blas_quda.h>

#include <test_util.h>

#define Nkernels 22

QudaPrecision cuda_prec;
QudaPrecision other_prec; // Used for copy benchmark
ParitySpinor x, y, z, w, v, p;

int nIters;

int Nthreads = 5;
int Ngrids = 9;
int blockSizes[] = {64, 128, 256, 512, 1024};
int gridSizes[] = {64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};

int prec;

void init()
{
  int X[4];

  X[0] = 24;
  X[1] = 24;
  X[2] = 24;
  X[3] = 24;

  int sp_pad = 0;

  switch(prec) {
  case 0:
    cuda_prec = QUDA_HALF_PRECISION;
    other_prec = QUDA_SINGLE_PRECISION;
    break;
  case 1:
    cuda_prec = QUDA_SINGLE_PRECISION;
    other_prec = QUDA_HALF_PRECISION;
    break;
  case 2:
    cuda_prec = QUDA_DOUBLE_PRECISION;
    other_prec = QUDA_HALF_PRECISION;
    break;
  }

  // need single parity dimensions
  X[0] /= 2;

  v = allocateParitySpinor(X, cuda_prec, sp_pad, QUDA_TWIST_NO);
  w = allocateParitySpinor(X, cuda_prec, sp_pad, QUDA_TWIST_NO);
  x = allocateParitySpinor(X, cuda_prec, sp_pad, QUDA_TWIST_NO);
  y = allocateParitySpinor(X, cuda_prec, sp_pad, QUDA_TWIST_NO);
  z = allocateParitySpinor(X, cuda_prec, sp_pad, QUDA_TWIST_NO);
  p = allocateParitySpinor(X, other_prec, sp_pad, QUDA_TWIST_NO);

  // check for successful allocation
  checkCudaError();

  // turn off error checking in blas kernels
  setBlasTuning(1);
}


void end()
{
  // release memory
  freeParitySpinor(p);
  freeParitySpinor(v);
  freeParitySpinor(w);
  freeParitySpinor(x);
  freeParitySpinor(y);
  freeParitySpinor(z);
}


double benchmark(int kernel) {

  double a, b;
  double2 a2, b2;

  cudaEvent_t start, end;
  cudaEventCreate(&start);
  cudaEventCreate(&end);
  cudaEventRecord(start, 0);

  for (int i=0; i < nIters; ++i) {
    switch (kernel) {

    case 0:
      copyCuda(y, p);
      break;

    case 1:
      axpbyCuda(a, x, b, y);
      break;

    case 2:
      xpyCuda(x, y);
      break;

    case 3:
      axpyCuda(a, x, y);
      break;

    case 4:
      xpayCuda(x, a, y);
      break;

    case 5:
      mxpyCuda(x, y);
      break;

    case 6:
      axCuda(a, x);
      break;

    case 7:
      caxpyCuda(a2, x, y);
      break;

    case 8:
      caxpbyCuda(a2, x, b2, y);
      break;

    case 9:
      cxpaypbzCuda(x, a2, y, b2, z);
      break;

    case 10:
      axpyZpbxCuda(a, x, y, z, b);
      break;

    case 11:
      caxpbypzYmbwCuda(a2, x, b2, y, z, w);
      break;
      
      // double
    case 12:
      sumCuda(x);
      break;

    case 13:
      normCuda(x);
      break;

    case 14:
      reDotProductCuda(x, y);
      break;

    case 15:
      axpyNormCuda(a, x, y);
      break;

    case 16:
      xmyNormCuda(x, y);
      break;
      
      // double2
    case 17:
      cDotProductCuda(x, y);
      break;

    case 18:
      xpaycDotzyCuda(x, a, y, z);
      break;
      
      // double3
    case 19:
      cDotProductNormACuda(x, y);
      break;

    case 20:
      cDotProductNormBCuda(x, y);
      break;

    case 21:
      caxpbypzYmbwcDotProductWYNormYQuda(a2, x, b2, y, z, w, v);
      break;
      
    default:
      printf("Undefined blas kernel %d\n", kernel);
      exit(1);
    }
  }
  
  cudaEventRecord(end, 0);
  cudaEventSynchronize(end);
  float runTime;
  cudaEventElapsedTime(&runTime, start, end);
  cudaEventDestroy(start);
  cudaEventDestroy(end);

  double secs = runTime / 1000;
  return secs;
}


void write(char *names[], int threads[][3], int blocks[][3])
{
  printf("\nWriting optimal parameters to blas_param.h\n");

  FILE *fp = fopen("blas_param.h", "w");
  fprintf(fp, "//\n// Auto-tuned blas CUDA parameters, generated by blas_test\n//\n\n");

  fprintf(fp, "static int blas_threads[%d][3] = {\n", Nkernels);

  for (int i=0; i<Nkernels; i++) {
    fprintf(fp, "  {%4d, %4d, %4d}%c  // Kernel %2d: %s\n", threads[i][0], threads[i][1], threads[i][2],
	    ((i == Nkernels-1) ? ' ' : ','), i, names[i]);
  }
  fprintf(fp, "};\n\n");

  fprintf(fp, "static int blas_blocks[%d][3] = {\n", Nkernels);

  for (int i=0; i<Nkernels; i++) {
    fprintf(fp, "  {%4d, %4d, %4d}%c  // Kernel %2d: %s\n", blocks[i][0], blocks[i][1], blocks[i][2],
	    ((i == Nkernels-1) ? ' ' : ','), i, names[i]);
  }
  fprintf(fp, "};\n");

  fclose(fp);
}


int main(int argc, char** argv)
{
  int dev = 0;
  if (argc == 2) dev = atoi(argv[1]);
  initQuda(dev);

  int threads[Nkernels][3];
  int blocks[Nkernels][3];

  int kernels[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
  char *names[] = {
    "copyCuda",
    "axpbyCuda",
    "xpyCuda",
    "axpyCuda",
    "xpayCuda",
    "mxpyCuda",
    "axCuda",
    "caxpyCuda",
    "caxpbyCuda",
    "cxpaypbzCuda",
    "axpyZpbxCuda",
    "caxpbypzYmbwCuda",
    "sumCuda",
    "normCuda",
    "reDotProductCuda",
    "axpyNormCuda",
    "xmyNormCuda",
    "cDotProductCuda",
    "xpaycDotzyCuda",
    "cDotProductNormACuda",
    "cDotProductNormBCuda",
    "caxpbypzYmbwcDotProductWYNormYQuda"
  };

  for (prec = 0; prec < 3; prec++) {

    init();

    printf("\nBenchmarking %d bit precision\n", (int)(pow(2.0,prec)*16));

    for (int i = 0; i < Nkernels; i++) {
 
      double gflops_max = 0.0;
      double gbytes_max = 0.0;
      int threads_max = 0; 
      int blocks_max = 0;

      for (int thread = 0; thread < Nthreads; thread++) {
	for (int grid = 0; grid < Ngrids; grid++) {
	  setBlasParam(i, prec, blockSizes[thread], gridSizes[grid]);

	  // first do warmup run
	  nIters = 1;
	  benchmark(kernels[i]);
	  
	  nIters = 300;
	  blas_quda_flops = 0;
	  blas_quda_bytes = 0;

	  double secs = benchmark(kernels[i]);
	  double flops = blas_quda_flops;
	  double bytes = blas_quda_bytes;
	  
	  double gflops = (flops*1e-9)/(secs);
	  double gbytes = bytes/(secs*(1<<30));

	  cudaError_t error = cudaGetLastError();

	  if (gbytes > gbytes_max && error == cudaSuccess) { // prevents selection of failed parameters
	    gflops_max = gflops;
	    gbytes_max = gbytes;
	    threads_max = blockSizes[thread];
	    blocks_max = gridSizes[grid];
	  }
	  
	  // printf("%d %d %-36s %f s, flops = %e, Gflops/s = %f, GiB/s = %f\n\n", 
	  //        blockSizes[thread], gridSizes[grid], names[i], secs, flops, gflops, gbytes);
	}
      }

      if (threads_max == 0) errorQuda("Autotuning failed for %s kernel", names[i]);
      
      printf("%-32s: %d threads per block, %d blocks per grid, Gflops/s = %f, GiB/s = %f\n", 
	     names[i], threads_max, blocks_max, gflops_max, gbytes_max);

      threads[i][prec] = threads_max;
      blocks[i][prec] = blocks_max;
    }
    end();
  }
  write(names, threads, blocks);
  endQuda();
}


