// The following indexing routines work for arbitrary (including odd) lattice dimensions.

// compute an index into the local volume from an index into the face (used by the face packing routines)
template <int dim, int nLayers>
static inline __device__ int indexFromFaceIndex(int face_idx, const int &face_volume,
						const int &face_num, const int &parity)
{
  // dimensions of the face (FIXME: optimize using constant cache)

  int face_X = X1, face_Y = X2, face_Z = X3; // face_T = X4;
  switch (dim) {
  case 0:
    face_X = nLayers;
    break;
  case 1:
    face_Y = nLayers;
    break;
  case 2:
    face_Z = nLayers;
    break;
  case 3:
    // face_T = nLayers;
    break;
  }
  int face_XYZ = face_X * face_Y * face_Z;
  int face_XY = face_X * face_Y;

  // intrinsic parity of the face depends on offset of first element

  int face_parity;
  switch (dim) {
  case 0:
    face_parity = (parity + face_num * (X1 - nLayers)) & 1;
    break;
  case 1:
    face_parity = (parity + face_num * (X2 - nLayers)) & 1;
    break;
  case 2:
    face_parity = (parity + face_num * (X3 - nLayers)) & 1;
    break;
  case 3:
    face_parity = (parity + face_num * (X4 - nLayers)) & 1;
    break;
  }

  // reconstruct full face index from index into the checkerboard

  face_idx *= 2;

  if (!(face_X & 1)) { // face_X even
    //   int t = face_idx / face_XYZ;
    //   int z = (face_idx / face_XY) % face_Z;
    //   int y = (face_idx / face_X) % face_Y;
    //   face_idx += (face_parity + t + z + y) & 1;
    // equivalent to the above, but with fewer divisions/mods:
    int aux1 = face_idx / face_X;
    int aux2 = aux1 / face_Y;
    int y = aux1 - aux2 * face_Y;
    int t = aux2 / face_Z;
    int z = aux2 - t * face_Z;
    face_idx += (face_parity + t + z + y) & 1;
  } else if (!(face_Y & 1)) { // face_Y even
    int t = face_idx / face_XYZ;
    int z = (face_idx / face_XY) % face_Z;
    face_idx += (face_parity + t + z) & 1;
  } else if (!(face_Z & 1)) { // face_Z even
    int t = face_idx / face_XYZ;
    face_idx += (face_parity + t) & 1;
  } else {
    face_idx += face_parity;
  }

  // compute index into the full local volume

  int idx = face_idx;
  int gap, aux;

  switch (dim) {
  case 0:
    gap = X1 - nLayers;
    aux = face_idx / face_X;
    idx += (aux + face_num) * gap;
    break;
  case 1:
    gap = X2 - nLayers;
    aux = face_idx / face_XY;
    idx += (aux + face_num) * gap * face_X;
    break;
  case 2:
    gap = X3 - nLayers;
    aux = face_idx / face_XYZ;
    idx += (aux + face_num) * gap * face_XY;
    break;
  case 3:
    gap = X4 - nLayers;
    idx += face_num * gap * face_XYZ;
    break;
  }

  // return index into the checkerboard

  return idx >> 1;
}

// compute an index into the local volume from an index into the face (used by the face packing routines)

//AS that is continues face_idx (that is actually a global thread ID from exe domain) is mapped to 
//AS corresponding lattice coordinate for a given face (dimention and face_num) and spinor parity
//AS face_num = 0 for back face, 1 for forward face
//AS nLayers = 1 for Wilson-like actions (why not via constant cashe?)
//AS this routine is used for DW MPI dslash:

template <int dim, int nLayers>
static inline __device__ int indexFromDWFaceIndex(int face_idx, const int &face_volume,
						const int &face_num, const int &parity)
{
  // dimensions of the face (FIXME: optimize using constant cache)

  //A.S.: Also used for computing offsets in physical lattice
  //A.S.: note that in the case of DW fermions one is dealing with 4d faces
  
  // intrinsic parity of the face depends on offset of first element, used for MPI DW as well
  int face_X = X1, face_Y = X2, face_Z = X3, face_T = X4;
  int face_parity;  
  
  switch (dim) {
  case 0:
    face_X = nLayers;
    face_parity = (parity + face_num * (X1 - nLayers)) & 1;
    break;
  case 1:
    face_Y = nLayers;
    face_parity = (parity + face_num * (X2 - nLayers)) & 1;
    break;
  case 2:
    face_Z = nLayers;
    face_parity = (parity + face_num * (X3 - nLayers)) & 1;
    break;
  case 3:
    face_T = nLayers;    
    face_parity = (parity + face_num * (X4 - nLayers)) & 1;
    break;
  }
  
  int face_XYZT = face_X * face_Y * face_Z * face_T;  
  int face_XYZ = face_X * face_Y * face_Z;
  int face_XY = face_X * face_Y;

  // reconstruct full face index from index into the checkerboard

  face_idx *= 2;

  if (!(face_X & 1)) { // face_X even
    //   int s = face_idx / face_XYZT;    
    //   int t = (face_idx / face_XYZ) % face_T;
    //   int z = (face_idx / face_XY) % face_Z;
    //   int y = (face_idx / face_X) % face_Y;
    //   face_idx += (face_parity + s + t + z + y) & 1;
    // equivalent to the above, but with fewer divisions/mods:
    int aux1 = face_idx / face_X;
    int aux2 = aux1 / face_Y;
    int aux3 = aux2 / face_Z;
    int y = aux1 - aux2 * face_Y;
    int z = aux2 - aux3 * face_Z;    
    int s = aux3 / face_T;
    int t = aux3 - s * face_T;
    face_idx += (face_parity + s + t + z + y) & 1;
  } else if (!(face_Y & 1)) { // face_Y even
    int s = face_idx / face_XYZT;    
    int t = (face_idx / face_XYZ) % face_T;
    int z = (face_idx / face_XY) % face_Z;
    face_idx += (face_parity + s + t + z) & 1;
  } else if (!(face_Z & 1)) { // face_Z even
    int s = face_idx / face_XYZT;        
    int t = (face_idx / face_XYZ) % face_T;
    face_idx += (face_parity + s + t) & 1;
  } else if(!(face_T)){
    int s = face_idx / face_XYZT;        
    face_idx += (face_parity + s) & 1;
  }else{    
    face_idx += face_parity;
  }

  // compute index into the full local volume

  int idx = face_idx;
  int gap, aux;

  switch (dim) {
  case 0:
    gap = X1 - nLayers;
    aux = face_idx / face_X;
    idx += (aux + face_num) * gap;
    break;
  case 1:
    gap = X2 - nLayers;
    aux = face_idx / face_XY;
    idx += (aux + face_num) * gap * face_X;
    break;
  case 2:
    gap = X3 - nLayers;
    aux = face_idx / face_XYZ;
    idx += (aux + face_num) * gap * face_XY;
    break;
  case 3:
    gap = X4 - nLayers;
    aux = face_idx / face_XYZT;
    idx += (aux + face_num) * gap * face_XYZ;
    break;
  }

  // return index into the checkerboard

  return idx >> 1;
}

// compute full coordinates from an index into the face (used by the exterior Dslash kernels)
template <int nLayers, typename Int>
static inline __device__ void coordsFromFaceIndex(int &idx, int &cb_idx, Int &X, Int &Y, Int &Z, Int &T, int face_idx,
						  const int &face_volume, const int &dim, const int &face_num, const int &parity)
{
  // dimensions of the face (FIXME: optimize using constant cache)

  int face_X = X1, face_Y = X2, face_Z = X3;
  int face_parity;
  switch (dim) {
  case 0:
    face_X = nLayers;
    face_parity = (parity + face_num * (X1 - nLayers)) & 1;
    break;
  case 1:
    face_Y = nLayers;
    face_parity = (parity + face_num * (X2 - nLayers)) & 1;
    break;
  case 2:
    face_Z = nLayers;
    face_parity = (parity + face_num * (X3 - nLayers)) & 1;
    break;
  case 3:
    face_parity = (parity + face_num * (X4 - nLayers)) & 1;
    break;
  }
  int face_XYZ = face_X * face_Y * face_Z;
  int face_XY = face_X * face_Y;

  // compute coordinates from (checkerboard) face index

  face_idx *= 2;

  int x, y, z, t;

  if (!(face_X & 1)) { // face_X even
    //   t = face_idx / face_XYZ;
    //   z = (face_idx / face_XY) % face_Z;
    //   y = (face_idx / face_X) % face_Y;
    //   face_idx += (face_parity + t + z + y) & 1;
    //   x = face_idx % face_X;
    // equivalent to the above, but with fewer divisions/mods:
    int aux1 = face_idx / face_X;
    x = face_idx - aux1 * face_X;
    int aux2 = aux1 / face_Y;
    y = aux1 - aux2 * face_Y;
    t = aux2 / face_Z;
    z = aux2 - t * face_Z;
    x += (face_parity + t + z + y) & 1;
    // face_idx += (face_parity + t + z + y) & 1;
  } else if (!(face_Y & 1)) { // face_Y even
    t = face_idx / face_XYZ;
    z = (face_idx / face_XY) % face_Z;
    face_idx += (face_parity + t + z) & 1;
    y = (face_idx / face_X) % face_Y;
    x = face_idx % face_X;
  } else if (!(face_Z & 1)) { // face_Z even
    t = face_idx / face_XYZ;
    face_idx += (face_parity + t) & 1;
    z = (face_idx / face_XY) % face_Z;
    y = (face_idx / face_X) % face_Y;
    x = face_idx % face_X;
  } else {
    face_idx += face_parity;
    t = face_idx / face_XYZ; 
    z = (face_idx / face_XY) % face_Z;
    y = (face_idx / face_X) % face_Y;
    x = face_idx % face_X;
  }

  //printf("Local sid %d (%d, %d, %d, %d)\n", cb_int, x, y, z, t);

  // need to convert to global coords, not face coords
  switch(dim) {
  case 0:
    x += face_num * (X1-nLayers);
    break;
  case 1:
    y += face_num * (X2-nLayers);
    break;
  case 2:
    z += face_num * (X3-nLayers);
    break;
  case 3:
    t += face_num * (X4-nLayers);
    break;
  }

  // compute index into the full local volume

  idx = X1*(X2*(X3*t + z) + y) + x; 

  // compute index into the checkerboard

  cb_idx = idx >> 1;

  X = x;
  Y = y;
  Z = z;
  T = t;  

  //printf("Global sid %d (%d, %d, %d, %d)\n", cb_int, x, y, z, t);
}

// compute full coordinates from an index into the face (used by the exterior Dslash kernels)
template <int nLayers, typename Int>
static inline __device__ void coordsFromDWFaceIndex(int &cb_idx, Int &X, Int &Y, Int &Z, Int &T, Int &S, int face_idx,
						  const int &face_volume, const int &dim, const int &face_num, const int &parity)
{
  // dimensions of the face (FIXME: optimize using constant cache)

  int face_X = X1, face_Y = X2, face_Z = X3, face_T = X4;
  int face_parity;
  switch (dim) {
  case 0:
    face_X = nLayers;
    face_parity = (parity + face_num * (X1 - nLayers)) & 1;
    break;
  case 1:
    face_Y = nLayers;
    face_parity = (parity + face_num * (X2 - nLayers)) & 1;
    break;
  case 2:
    face_Z = nLayers;
    face_parity = (parity + face_num * (X3 - nLayers)) & 1;
    break;
  case 3:
    face_T = nLayers;    
    face_parity = (parity + face_num * (X4 - nLayers)) & 1;
    break;
  }
  int face_XYZT = face_X * face_Y * face_Z * face_T;  
  int face_XYZ  = face_X * face_Y * face_Z;
  int face_XY   = face_X * face_Y;

  // compute coordinates from (checkerboard) face index

  face_idx *= 2;

  int x, y, z, t, s;

  if (!(face_X & 1)) { // face_X even
    //   s = face_idx / face_XYZT;        
    //   t = (face_idx / face_XYZ) % face_T;
    //   z = (face_idx / face_XY) % face_Z;
    //   y = (face_idx / face_X) % face_Y;
    //   face_idx += (face_parity + s + t + z + y) & 1;
    //   x = face_idx % face_X;
    // equivalent to the above, but with fewer divisions/mods:
    int aux1 = face_idx / face_X;
    x = face_idx - aux1 * face_X;
    int aux2 = aux1 / face_Y;
    y = aux1 - aux2 * face_Y;
    int aux3 = aux2 / face_Z;
    z = aux2 - aux3 * face_Z;
    s = aux3 / face_T;
    t = aux3 - s * face_T;
    x += (face_parity + s + t + z + y) & 1;
    // face_idx += (face_parity + t + z + y) & 1;
  } else if (!(face_Y & 1)) { // face_Y even
    s = face_idx / face_XYZT;    
    t = (face_idx / face_XYZ) % face_T;
    z = (face_idx / face_XY) % face_Z;
    face_idx += (face_parity + s + t + z) & 1;
    y = (face_idx / face_X) % face_Y;
    x = face_idx % face_X;
  } else if (!(face_Z & 1)) { // face_Z even
    s = face_idx / face_XYZT;    
    t = (face_idx / face_XYZ) % face_T;
    face_idx += (face_parity + s + t) & 1;
    z = (face_idx / face_XY) % face_Z;
    y = (face_idx / face_X) % face_Y;
    x = face_idx % face_X;
  } else {
    s = face_idx / face_XYZT;        
    face_idx += face_parity;
    t = (face_idx / face_XYZ) % face_T;
    z = (face_idx / face_XY) % face_Z;
    y = (face_idx / face_X) % face_Y;
    x = face_idx % face_X;
  }

  //printf("Local sid %d (%d, %d, %d, %d)\n", cb_int, x, y, z, t);

  // need to convert to global coords, not face coords
  switch(dim) {
  case 0:
    x += face_num * (X1-nLayers);
    break;
  case 1:
    y += face_num * (X2-nLayers);
    break;
  case 2:
    z += face_num * (X3-nLayers);
    break;
  case 3:
    t += face_num * (X4-nLayers);
    break;
  }

  // compute index into the checkerboard

  cb_idx = (X1*(X2*(X3*(X4*s + t) + z) + y) + x) >> 1;

  X = x;
  Y = y;
  Z = z;
  T = t;  
  S = s;

  //printf("Global sid %d (%d, %d, %d, %d)\n", cb_int, x, y, z, t);
}


// compute coordinates from index into the checkerboard (used by the interior Dslash kernels)
template <typename Int>
static __device__ __forceinline__ void coordsFromIndex(int &idx, Int &X, Int &Y, Int &Z, Int &T, const int &cb_idx, const int &parity)
{

  int &LX = X1;
  int &LY = X2;
  int &LZ = X3;
  int &XYZ = X3X2X1;
  int &XY = X2X1;

  idx = 2*cb_idx;

  int x, y, z, t;

  if (!(LX & 1)) { // X even
    //   t = idx / XYZ;
    //   z = (idx / XY) % Z;
    //   y = (idx / X) % Y;
    //   idx += (parity + t + z + y) & 1;
    //   x = idx % X;
    // equivalent to the above, but with fewer divisions/mods:
    int aux1 = idx / LX;
    x = idx - aux1 * LX;
    int aux2 = aux1 / LY;
    y = aux1 - aux2 * LY;
    t = aux2 / LZ;
    z = aux2 - t * LZ;
    aux1 = (parity + t + z + y) & 1;
    x += aux1;
    idx += aux1;
  } else if (!(LY & 1)) { // Y even
    t = idx / XYZ;
    z = (idx / XY) % LZ;
    idx += (parity + t + z) & 1;
    y = (idx / LX) % LY;
    x = idx % LX;
  } else if (!(LZ & 1)) { // Z even
    t = idx / XYZ;
    idx += (parity + t) & 1;
    z = (idx / XY) % LZ;
    y = (idx / LX) % LY;
    x = idx % LX;
  } else {
    idx += parity;
    t = idx / XYZ;
    z = (idx / XY) % LZ;
    y = (idx / LX) % LY;
    x = idx % LX;
  }

  X = x;
  Y = y;
  Z = z;
  T = t;
}


// routines for packing the ghost zones (multi-GPU only)

#ifdef MULTI_GPU

#ifdef GPU_WILSON_DIRAC

// double precision
#ifdef DIRECT_ACCESS_WILSON_PACK_SPINOR
#define READ_SPINOR READ_SPINOR_DOUBLE
#define READ_SPINOR_UP READ_SPINOR_DOUBLE_UP
#define READ_SPINOR_DOWN READ_SPINOR_DOUBLE_DOWN
#define SPINORTEX in
#else
#define READ_SPINOR READ_SPINOR_DOUBLE_TEX
#define READ_SPINOR_UP READ_SPINOR_DOUBLE_UP_TEX
#define READ_SPINOR_DOWN READ_SPINOR_DOUBLE_DOWN_TEX
#define SPINORTEX spinorTexDouble
#endif
#define WRITE_HALF_SPINOR WRITE_HALF_SPINOR_DOUBLE2
#define SPINOR_DOUBLE
template <int dim, int dagger>
static inline __device__ void packFaceWilsonCore(double2 *out, float *outNorm, const double2 *in, const float *inNorm,
						 const int &idx, const int &face_idx, const int &face_volume, const int &face_num)
{
#if (__CUDA_ARCH__ >= 130)
    if (dagger) {
#include "wilson_pack_face_dagger_core.h"
    } else {
#include "wilson_pack_face_core.h"
    }
#endif // (__CUDA_ARCH__ >= 130)
}
#undef READ_SPINOR
#undef READ_SPINOR_UP
#undef READ_SPINOR_DOWN
#undef SPINORTEX
#undef WRITE_HALF_SPINOR
#undef SPINOR_DOUBLE


// single precision
#ifdef DIRECT_ACCESS_WILSON_PACK_SPINOR
#define READ_SPINOR READ_SPINOR_SINGLE
#define READ_SPINOR_UP READ_SPINOR_SINGLE_UP
#define READ_SPINOR_DOWN READ_SPINOR_SINGLE_DOWN
#define SPINORTEX in
#else
#define READ_SPINOR READ_SPINOR_SINGLE_TEX
#define READ_SPINOR_UP READ_SPINOR_SINGLE_UP_TEX
#define READ_SPINOR_DOWN READ_SPINOR_SINGLE_DOWN_TEX
#define SPINORTEX spinorTexSingle
#endif
#define WRITE_HALF_SPINOR WRITE_HALF_SPINOR_FLOAT4
template <int dim, int dagger>
static inline __device__ void packFaceWilsonCore(float4 *out, float *outNorm, const float4 *in, const float *inNorm,
						 const int &idx, const int &face_idx, const int &face_volume, const int &face_num)
{
    if (dagger) {
#include "wilson_pack_face_dagger_core.h"
    } else {
#include "wilson_pack_face_core.h"
    }
}
#undef READ_SPINOR
#undef READ_SPINOR_UP
#undef READ_SPINOR_DOWN
#undef SPINORTEX
#undef WRITE_HALF_SPINOR


// half precision
#ifdef DIRECT_ACCESS_WILSON_PACK_SPINOR
#define READ_SPINOR READ_SPINOR_HALF
#define READ_SPINOR_UP READ_SPINOR_HALF_UP
#define READ_SPINOR_DOWN READ_SPINOR_HALF_DOWN
#define SPINORTEX in
#else
#define READ_SPINOR READ_SPINOR_HALF_TEX
#define READ_SPINOR_UP READ_SPINOR_HALF_UP_TEX
#define READ_SPINOR_DOWN READ_SPINOR_HALF_DOWN_TEX
#define SPINORTEX spinorTexHalf
#endif
#define WRITE_HALF_SPINOR WRITE_HALF_SPINOR_SHORT4
template <int dim, int dagger>
static inline __device__ void packFaceWilsonCore(short4 *out, float *outNorm, const short4 *in, const float *inNorm,
						 const int &idx, const int &face_idx, const int &face_volume, const int &face_num)
{
    if (dagger) {
#include "wilson_pack_face_dagger_core.h"
    } else {
#include "wilson_pack_face_core.h"
    }
}
#undef READ_SPINOR
#undef READ_SPINOR_UP
#undef READ_SPINOR_DOWN
#undef SPINORTEX
#undef WRITE_HALF_SPINOR


template <int dim, int dagger, typename FloatN>
__global__ void packFaceWilsonKernel(FloatN *out, float *outNorm, const FloatN *in, const float *inNorm, const int face_num, const int parity)
{
  int face_volume = ghostFace[dim];

  int face_idx = blockIdx.x*blockDim.x + threadIdx.x;
  if (face_idx >= face_volume) return;

  // compute an index into the local volume from the index into the face
  int idx = indexFromFaceIndex<dim, 1>(face_idx, face_volume, face_num, parity);

  // read spinor, spin-project, and write half spinor to face
  packFaceWilsonCore<dim, dagger>(out, outNorm, in, inNorm, idx, face_idx, face_volume, face_num);
}

#endif // GPU_WILSON_DIRAC

template <typename FloatN>
void packFaceWilson(FloatN *faces, float *facesNorm, const FloatN *in, const float *inNorm, 
		    const int dim, const QudaDirection dir, const int dagger, const int parity, 
		    const dim3 &gridDim, const dim3 &blockDim, const cudaStream_t &stream)
{
#ifdef GPU_WILSON_DIRAC

  const int face_num = (dir == QUDA_FORWARDS);

  if (dagger) {
    switch (dim) {
    case 0: packFaceWilsonKernel<0,1><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm, face_num, parity); break;
    case 1: packFaceWilsonKernel<1,1><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm, face_num, parity); break;
    case 2: packFaceWilsonKernel<2,1><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm, face_num, parity); break;
    case 3: packFaceWilsonKernel<3,1><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm, face_num, parity); break;
    }
  } else {
    switch (dim) {
    case 0: packFaceWilsonKernel<0,0><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm, face_num, parity); break;
    case 1: packFaceWilsonKernel<1,0><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm, face_num, parity); break;
    case 2: packFaceWilsonKernel<2,0><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm, face_num, parity); break;
    case 3: packFaceWilsonKernel<3,0><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm, face_num, parity); break;
    }
  }
#else
  errorQuda("Wilson face packing kernel is not built");
#endif  
}


void packFaceWilson(void *ghost_buf, cudaColorSpinorField &in, const int dim, const QudaDirection dir, const int dagger, 
		    const int parity, const cudaStream_t &stream) {

  dim3 blockDim(64, 1, 1); // TODO: make this a parameter for auto-tuning
  dim3 gridDim( (in.ghostFace[dim]+blockDim.x-1) / blockDim.x, 1, 1);

  int Ninternal = in.nColor * in.nSpin; // assume spin projection
  float *ghostNorm = (float*)((char*)ghost_buf + Ninternal*in.ghostFace[dim]*in.precision); // norm zone

  //printfQuda("Starting face packing: dimension = %d, direction = %d, face size = %d\n", dim, dir, in.ghostFace[dim]);

  switch(in.precision) {
  case QUDA_DOUBLE_PRECISION:
    packFaceWilson((double2*)ghost_buf, ghostNorm, (double2*)in.v, (float*)in.norm, 
		   dim, dir, dagger, parity, gridDim, blockDim, stream);
    break;
  case QUDA_SINGLE_PRECISION:
    packFaceWilson((float4*)ghost_buf, ghostNorm, (float4*)in.v, (float*)in.norm, 
		   dim, dir, dagger, parity, gridDim, blockDim, stream);
    break;
  case QUDA_HALF_PRECISION:
    packFaceWilson((short4*)ghost_buf, ghostNorm, (short4*)in.v, (float*)in.norm, 
		   dim, dir, dagger, parity, gridDim, blockDim, stream);
    break;
  }  
  CUERR;

  //printfQuda("Completed face packing\n", dim, dir, ghostFace[dir]);
}


// At the moment, these staggered face-packing routines aren't actually used.
//
// TODO: add support for textured reads
// TODO: pack only one face at a time

#ifdef GPU_STAGGERED_DIRAC
template <int dir, typename Float2>
__global__ void packFaceAsqtadKernel(Float2 *out, float *outNorm, const Float2 *in, const float *inNorm, const int parity)
{
  int face_volume = ghostFace[dir];

  int face_idx = blockIdx.x*blockDim.x + threadIdx.x;
  if (face_idx >= 2*face_volume) return;

  // are we on face 0 or face 1?
  int face_num = (face_idx >= face_volume);

  face_idx -= face_num*face_volume;

  // set "out" to point to the beginning of face 1 if necessary
  out += face_num * 3 * face_volume;

  // compute an index into the local volume from the index into the face
  int idx = indexFromFaceIndex<dir, 3>(face_idx, face_volume, face_num, parity);

  out[face_idx + 0*face_volume] = in[idx + 0*sp_stride];
  out[face_idx + 1*face_volume] = in[idx + 1*sp_stride];
  out[face_idx + 2*face_volume] = in[idx + 2*sp_stride];

  if (typeid(Float2) == typeid(short2)) outNorm[face_idx] = inNorm[idx];
}
#endif // GPU_STAGGERED_DIRAC


template <typename Float2>
void packFaceAsqtad(Float2 *faces, float *facesNorm, const Float2 *in, const float *inNorm, int dir,
		    const dim3 &gridDim, const dim3 &blockDim, const cudaStream_t &stream)
{
#ifdef GPU_STAGGERED_DIRAC
  switch (dir) {
  case 0: packFaceAsqtadKernel<0><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm); break;
  case 1: packFaceAsqtadKernel<1><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm); break;
  case 2: packFaceAsqtadKernel<2><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm); break;
  case 3: packFaceAsqtadKernel<3><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm); break;
  }
#else
  errorQuda("Asqtad face packing kernel is not built");
#endif  
}

//BEGIN NEW

#ifdef GPU_DOMAIN_WALL_DIRAC
template <int dim, int dagger, typename FloatN>
__global__ void packFaceDWKernel(FloatN *out, float *outNorm, const FloatN *in, const float *inNorm, const int face_num, const int parity)
{
  int face_volume = Ls*ghostFace[dim];

  int face_idx = blockIdx.x*blockDim.x + threadIdx.x;
  if (face_idx >= face_volume) return;

  // compute an index into the local volume from the index into the face
  int idx = indexFromDWFaceIndex<dim, 1>(face_idx, face_volume, face_num, parity);

  // read spinor, spin-project, and write half spinor to face (the same kernel as for Wilson): 
  packFaceWilsonCore<dim, dagger>(out, outNorm, in, inNorm, idx, face_idx, face_volume, face_num);
}

template <typename FloatN>
void packFaceDW(FloatN *faces, float *facesNorm, const FloatN *in, const float *inNorm, 
		    const int dim, const QudaDirection dir, const int dagger, const int parity, 
		    const dim3 &gridDim, const dim3 &blockDim, const cudaStream_t &stream)
{

  const int face_num = (dir == QUDA_FORWARDS);

  if (dagger) {
    switch (dim) {
    case 0: packFaceDWKernel<0,1><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm, face_num, parity); break;
    case 1: packFaceDWKernel<1,1><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm, face_num, parity); break;
    case 2: packFaceDWKernel<2,1><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm, face_num, parity); break;
    case 3: packFaceDWKernel<3,1><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm, face_num, parity); break;
    }
  } else {
    switch (dim) {
    case 0: packFaceDWKernel<0,0><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm, face_num, parity); break;
    case 1: packFaceDWKernel<1,0><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm, face_num, parity); break;
    case 2: packFaceDWKernel<2,0><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm, face_num, parity); break;
    case 3: packFaceDWKernel<3,0><<<gridDim, blockDim, 0, stream>>>(faces, facesNorm, in, inNorm, face_num, parity); break;
    }
  }
}

void packFaceDW(void *ghost_buf, cudaColorSpinorField &in, const int dim, const QudaDirection dir, const int dagger, 
		    const int parity, const cudaStream_t &stream) {
#ifdef GPU_WILSON_DIRAC
  dim3 blockDim(64, 1, 1); // TODO: make this a parameter for auto-tuning
  dim3 gridDim( (in.ghostFace[dim]+blockDim.x-1) / blockDim.x, 1, 1);

  int Ninternal = in.nColor * in.nSpin; // assume spin projection
  float *ghostNorm = (float*)((char*)ghost_buf + Ninternal*in.ghostFace[dim]*in.precision); // norm zone

  //printfQuda("Starting face packing: dimension = %d, direction = %d, face size = %d\n", dim, dir, in.ghostFace[dim]);
  switch(in.precision) {
  case QUDA_DOUBLE_PRECISION:
    packFaceDW((double2*)ghost_buf, ghostNorm, (double2*)in.v, (float*)in.norm, 
		   dim, dir, dagger, parity, gridDim, blockDim, stream);
    break;
  case QUDA_SINGLE_PRECISION:
    packFaceDW((float4*)ghost_buf, ghostNorm, (float4*)in.v, (float*)in.norm, 
		   dim, dir, dagger, parity, gridDim, blockDim, stream);
    break;
  case QUDA_HALF_PRECISION:
    packFaceDW((short4*)ghost_buf, ghostNorm, (short4*)in.v, (float*)in.norm, 
		   dim, dir, dagger, parity, gridDim, blockDim, stream);
    break;
  }  
  CUERR;

  //printfQuda("Completed face packing\n", dim, dir, ghostFace[dir]);
#else
  errorQuda("DW face parking kernels are not built. Check that both GPU_WILSON_DIRAC and GPU_DOMAIN_WALL_DIRAC compiler flags are set.");
#endif //GPU_WILSON_DIRAC
}
#endif //GPU_DOMAIN_WALL
#endif // MULTI_GPU

//END NEW
