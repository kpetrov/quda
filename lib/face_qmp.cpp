#include <quda_internal.h>
#include <face_quda.h>
#include <cstdio>
#include <cstdlib>
#include <quda.h>
#include <string.h>
#include <gauge_quda.h>

#ifdef QMP_COMMS
#include <qmp.h>
#endif

/*
  Multi-GPU TODOs
  - test qmp code
  - implement OpenMP version?
  - split face kernels
  - separate block sizes for body and face
  - minimize pointer arithmetic in core code (need extra constant to replace SPINOR_HOP)
 */

#define PINNED_COPY

using namespace std;

cudaStream_t *stream;

bool globalReduce = true;

// Easy to switch between overlapping communication or not
#ifdef OVERLAP_COMMS
#define CUDAMEMCPY(dst, src, size, type, stream) cudaMemcpyAsync(dst, src, size, type, stream)
#else
#define CUDAMEMCPY(dst, src, size, type, stream) cudaMemcpy(dst, src, size, type)
#endif

FaceBuffer::FaceBuffer(const int *X, const int nDim, const int Ninternal, 
		       const int nFace, const QudaPrecision precision, const int Ls /*=1*/ ) :
  Ninternal(Ninternal), precision(precision), nDim(nDim), nFace(nFace)
{
  if (nDim > QUDA_MAX_DIM) errorQuda("nDim = %d is greater than the maximum of %d\n", nDim, QUDA_MAX_DIM);

  setupDims(X);

  // set these both = 0 `for no overlap of qmp and cudamemcpyasync
  // sendBackStrmIdx = 0, and sendFwdStrmIdx = 1 for overlap
  sendBackStrmIdx = 0;
  sendFwdStrmIdx = 1;
  recFwdStrmIdx = sendBackStrmIdx;
  recBackStrmIdx = sendFwdStrmIdx;
  
  unsigned int flag = cudaHostAllocDefault;

  //printf("nDim = %d\n", nDim);

  // Buffers hold half spinors
  for (int i=0; i<nDim; i++) {
    nbytes[i] = nFace*faceVolumeCB[i]*Ninternal*precision;

    // add extra space for the norms for half precision
    if (precision == QUDA_HALF_PRECISION) nbytes[i] += nFace*faceVolumeCB[i]*sizeof(float);
    //printf("bytes = %d, nFace = %d, faceVolume = %d, Ndof = %d, prec =  %d\n", 
    //	   nbytes[i], nFace, faceVolumeCB[i], Ninternal, precision);

    cudaHostAlloc(&(my_fwd_face[i]), nbytes[i], flag);
    if( !my_fwd_face[i] ) errorQuda("Unable to allocate my_fwd_face with size %lu", nbytes[i]);
  
    //printf("%d\n", nbytes[i]);

    cudaHostAlloc(&(my_back_face[i]), nbytes[i], flag);
    if( !my_back_face[i] ) errorQuda("Unable to allocate my_back_face with size %lu", nbytes[i]);
  }

  for (int i=0; i<nDim; i++) {
#ifdef QMP_COMMS
    unsigned int flag = cudaHostAllocWriteCombined;
    cudaHostAlloc(&(from_fwd_face[i]), nbytes[i], flag);
    if( !from_fwd_face[i] ) errorQuda("Unable to allocate from_fwd_face with size %lu", nbytes[i]);
    
    cudaHostAlloc(&(from_back_face[i]), nbytes[i], flag);
    if( !from_back_face[i] ) errorQuda("Unable to allocate from_back_face with size %lu", nbytes[i]);

#ifdef PINNED_COPY
    ib_my_fwd_face[i] = malloc(nbytes[i]);
    if (!ib_my_fwd_face[i]) errorQuda("Unable to allocate ib_my_fwd_face with size %lu", nbytes[i]);

    ib_my_back_face[i] = malloc(nbytes[i]);
    if (!ib_my_back_face[i]) errorQuda("Unable to allocate ib_my_back_face with size %lu", nbytes[i]);

    ib_from_fwd_face[i] = malloc(nbytes[i]);
    if (!ib_from_fwd_face[i]) errorQuda("Unable to allocate ib_from_fwd_face with size %lu", nbytes[i]);

    ib_from_back_face[i] = malloc(nbytes[i]);
    if (!ib_from_back_face[i]) errorQuda("Unable to allocate ib_from_back_face with size %lu", nbytes[i]);
#endif

#else
    from_fwd_face[i] = my_back_face[i];
    from_back_face[i] = my_fwd_face[i];
#endif  
  }

#ifdef QMP_COMMS
  for (int i=0; i<nDim; i++) {

#ifdef PINNED_COPY
    mm_send_fwd[i] = QMP_declare_msgmem(ib_my_fwd_face[i], nbytes[i]);
    if( mm_send_fwd[i] == NULL ) errorQuda("Unable to allocate send fwd message mem");
    
    mm_send_back[i] = QMP_declare_msgmem(ib_my_back_face[i], nbytes[i]);
    if( mm_send_back[i] == NULL ) errorQuda("Unable to allocate send back message mem");
    
    mm_from_fwd[i] = QMP_declare_msgmem(ib_from_fwd_face[i], nbytes[i]);
    if( mm_from_fwd[i] == NULL ) errorQuda("Unable to allocate recv from fwd message mem");
    
    mm_from_back[i] = QMP_declare_msgmem(ib_from_back_face[i], nbytes[i]);
    if( mm_from_back[i] == NULL ) errorQuda("Unable to allocate recv from back message mem");
#else
    mm_send_fwd[i] = QMP_declare_msgmem(my_fwd_face[i], nbytes[i]);
    if( mm_send_fwd[i] == NULL ) errorQuda("Unable to allocate send fwd message mem");
    
    mm_send_back[i] = QMP_declare_msgmem(my_back_face[i], nbytes[i]);
    if( mm_send_back[i] == NULL ) errorQuda("Unable to allocate send back message mem");
    
    mm_from_fwd[i] = QMP_declare_msgmem(from_fwd_face[i], nbytes[i]);
    if( mm_from_fwd[i] == NULL ) errorQuda("Unable to allocate recv from fwd message mem");
    
    mm_from_back[i] = QMP_declare_msgmem(from_back_face[i], nbytes[i]);
    if( mm_from_back[i] == NULL ) errorQuda("Unable to allocate recv from back message mem");
#endif    

    mh_send_fwd[i] = QMP_declare_send_relative(mm_send_fwd[i], i, +1, 0);
    if( mh_send_fwd[i] == NULL ) errorQuda("Unable to allocate forward send");
    
    mh_send_back[i] = QMP_declare_send_relative(mm_send_back[i], i, -1, 0);
    if( mh_send_back[i] == NULL ) errorQuda("Unable to allocate backward send");
    
    mh_from_fwd[i] = QMP_declare_receive_relative(mm_from_fwd[i], i, +1, 0);
    if( mh_from_fwd[i] == NULL ) errorQuda("Unable to allocate forward recv");
    
    mh_from_back[i] = QMP_declare_receive_relative(mm_from_back[i], i, -1, 0);
    if( mh_from_back[i] == NULL ) errorQuda("Unable to allocate backward recv");
  }
#endif

}

FaceBuffer::FaceBuffer(const FaceBuffer &face) {
  errorQuda("FaceBuffer copy constructor not implemented");
}

void FaceBuffer::setupDims(const int* X)
{
  Volume = 1;
  for (int d=0; d< nDim; d++) {
    this->X[d] = X[d];
    Volume *= this->X[d];    
  }
  VolumeCB = Volume/2;

  for (int i=0; i<nDim; i++) {
    faceVolume[i] = 1;
    for (int j=0; j<nDim; j++) {
      if (i==j) continue;
      faceVolume[i] *= this->X[j];
    }
    faceVolumeCB[i] = faceVolume[i]/2;

  }

}

FaceBuffer::~FaceBuffer()
{
  
  //printf("Ndim = %d\n", nDim);
  for (int i=0; i<nDim; i++) {
#ifdef QMP_COMMS

#ifdef PINNED_COPY
    free(ib_my_fwd_face[i]);
    free(ib_my_back_face[i]);
    free(ib_from_fwd_face[i]);
    free(ib_from_back_face[i]);
#endif

    QMP_free_msghandle(mh_send_fwd[i]);
    QMP_free_msghandle(mh_send_back[i]);
    QMP_free_msghandle(mh_from_fwd[i]);
    QMP_free_msghandle(mh_from_back[i]);
    QMP_free_msgmem(mm_send_fwd[i]);
    QMP_free_msgmem(mm_send_back[i]);
    QMP_free_msgmem(mm_from_fwd[i]);
    QMP_free_msgmem(mm_from_back[i]);
    cudaFreeHost(from_fwd_face[i]); // these are aliasing pointers for non-qmp case
    cudaFreeHost(from_back_face[i]);// these are aliasing pointers for non-qmp case
#endif
    cudaFreeHost(my_fwd_face[i]);
    cudaFreeHost(my_back_face[i]);
  }

  for (int i=0; i<nDim; i++) {
    my_fwd_face[i]=NULL;
    my_back_face[i]=NULL;
    from_fwd_face[i]=NULL;
    from_back_face[i]=NULL;
  }
}

void FaceBuffer::exchangeFacesStart(cudaColorSpinorField &in, int parity,
				    int dagger, int dir, cudaStream_t *stream_p)
{
  if(!commDimPartitioned(dir)) return;

  in.allocateGhostBuffer();   // allocate the ghost buffer if not yet allocated

  stream = stream_p;
  
#ifdef QMP_COMMS
    // Prepost all receives
    QMP_start(mh_from_fwd[dir]);
    QMP_start(mh_from_back[dir]);
#endif

    // gather for backwards send
    in.packGhost(my_back_face[dir], dir, QUDA_BACKWARDS, (QudaParity)parity, dagger, &stream[2*dir+sendBackStrmIdx]);

    // gather for forwards send
    in.packGhost(my_fwd_face[dir], dir, QUDA_FORWARDS, (QudaParity)parity, dagger, &stream[2*dir+sendFwdStrmIdx]);

}

void FaceBuffer::exchangeFacesComms(int dir) {
  if(!commDimPartitioned(dir)) return;

#ifdef OVERLAP_COMMS
  // Need to wait for copy to finish before sending to neighbour
  cudaStreamSynchronize(stream[2*dir + sendBackStrmIdx]);
#endif

#ifdef QMP_COMMS
  // Begin backward send
#ifdef PINNED_COPY
  memcpy(ib_my_back_face[dir], my_back_face[dir], nbytes[dir]);
#endif
  QMP_start(mh_send_back[dir]);
#endif

#ifdef OVERLAP_COMMS
  // Need to wait for copy to finish before sending to neighbour
  cudaStreamSynchronize(stream[2*dir + sendFwdStrmIdx]);
#endif

#ifdef QMP_COMMS
  // Begin forward send
#ifdef PINNED_COPY
  memcpy(ib_my_fwd_face[dir], my_fwd_face[dir], nbytes[dir]);
#endif
  QMP_start(mh_send_fwd[dir]);
#endif

} 

// Finish backwards send and forwards receive
#ifdef QMP_COMMS				

#ifdef PINNED_COPY
#define QMP_finish_from_fwd(dir)					\
  QMP_wait(mh_send_back[dir]);						\
  QMP_wait(mh_from_fwd[dir]);						\
  memcpy(from_fwd_face[dir], ib_from_fwd_face[dir], nbytes[dir]);		

// Finish forwards send and backwards receive
#define QMP_finish_from_back(dir)					\
  QMP_wait(mh_send_fwd[dir]);						\
  QMP_wait(mh_from_back[dir]);						\
  memcpy(from_back_face[dir], ib_from_back_face[dir], nbytes[dir]);		

#else

#define QMP_finish_from_fwd(dir)					\
  QMP_wait(mh_send_back[dir]);						\
  QMP_wait(mh_from_fwd[dir]);						

// Finish forwards send and backwards receive
#define QMP_finish_from_back(dir)					\
  QMP_wait(mh_send_fwd[dir]);						\
  QMP_wait(mh_from_back[dir]);						

#endif

#else
#define QMP_finish_from_fwd					

#define QMP_finish_from_back					

#endif

void FaceBuffer::exchangeFacesWait(cudaColorSpinorField &out, int dagger, int dir)
{
  if(!commDimPartitioned(dir)) return;

  // replaced this memcopy with aliasing pointers - useful benchmarking
#ifndef QMP_COMMS
  // NO QMP -- do copies
  //CUDAMEMCPY(from_fwd_face, my_back_face, nbytes, cudaMemcpyHostToHost, stream[sendBackStrmIdx]); // 174 without these
  //CUDAMEMCPY(from_back_face, my_fwd_face, nbytes, cudaMemcpyHostToHost, stream[sendFwdStrmIdx]);
#endif // QMP_COMMS

  // Scatter faces.
  QMP_finish_from_fwd(dir);
  
  out.unpackGhost(from_fwd_face[dir], dir, QUDA_FORWARDS, dagger, &stream[2*dir+recFwdStrmIdx]); // 0, 2, 4, 6

  QMP_finish_from_back(dir);
  
  out.unpackGhost(from_back_face[dir], dir, QUDA_BACKWARDS, dagger, &stream[2*dir+recBackStrmIdx]); // 1, 3, 5, 7
}

// This is just an initial hack for CPU comms - should be creating the message handlers at instantiation
void FaceBuffer::exchangeCpuSpinor(cpuColorSpinorField &spinor, int oddBit, int dagger)
{

  // allocate the ghost buffer if not yet allocated
  spinor.allocateGhostBuffer();

  for(int i=0;i < 4; i++){
    spinor.packGhost(spinor.backGhostFaceSendBuffer[i], i, QUDA_BACKWARDS, (QudaParity)oddBit, dagger);
    spinor.packGhost(spinor.fwdGhostFaceSendBuffer[i], i, QUDA_FORWARDS, (QudaParity)oddBit, dagger);
  }

#ifdef QMP_COMMS

  QMP_msgmem_t mm_send_fwd[4];
  QMP_msgmem_t mm_from_back[4];
  QMP_msgmem_t mm_from_fwd[4];
  QMP_msgmem_t mm_send_back[4];
  QMP_msghandle_t mh_send_fwd[4];
  QMP_msghandle_t mh_from_back[4];
  QMP_msghandle_t mh_from_fwd[4];
  QMP_msghandle_t mh_send_back[4];

  for (int i=0; i<4; i++) {
    mm_send_fwd[i] = QMP_declare_msgmem(spinor.fwdGhostFaceSendBuffer[i], nbytes[i]);
    if( mm_send_fwd[i] == NULL ) errorQuda("Unable to allocate send fwd message mem");
    
    mm_send_back[i] = QMP_declare_msgmem(spinor.backGhostFaceSendBuffer[i], nbytes[i]);
    if( mm_send_back == NULL ) errorQuda("Unable to allocate send back message mem");
    
    mm_from_fwd[i] = QMP_declare_msgmem(spinor.fwdGhostFaceBuffer[i], nbytes[i]);
    if( mm_from_fwd[i] == NULL ) errorQuda("Unable to allocate recv from fwd message mem");
    
    mm_from_back[i] = QMP_declare_msgmem(spinor.backGhostFaceBuffer[i], nbytes[i]);
    if( mm_from_back[i] == NULL ) errorQuda("Unable to allocate recv from back message mem");
    
    mh_send_fwd[i] = QMP_declare_send_relative(mm_send_fwd[i], i, +1, 0);
    if( mh_send_fwd[i] == NULL ) errorQuda("Unable to allocate forward send");
    
    mh_send_back[i] = QMP_declare_send_relative(mm_send_back[i], i, -1, 0);
    if( mh_send_back[i] == NULL ) errorQuda("Unable to allocate backward send");
    
    mh_from_fwd[i] = QMP_declare_receive_relative(mm_from_fwd[i], i, +1, 0);
    if( mh_from_fwd[i] == NULL ) errorQuda("Unable to allocate forward recv");
    
    mh_from_back[i] = QMP_declare_receive_relative(mm_from_back[i], i, -1, 0);
    if( mh_from_back[i] == NULL ) errorQuda("Unable to allocate backward recv");
  }

  for (int i=0; i<4; i++) {
    QMP_start(mh_from_back[i]);
    QMP_start(mh_from_fwd[i]);
    QMP_start(mh_send_fwd[i]);
    QMP_start(mh_send_back[i]);
  }

  for (int i=0; i<4; i++) {
    QMP_wait(mh_send_fwd[i]);
    QMP_wait(mh_send_back[i]);
    QMP_wait(mh_from_back[i]);
    QMP_wait(mh_from_fwd[i]);
  }

  for (int i=0; i<4; i++) {
    QMP_free_msghandle(mh_send_fwd[i]);
    QMP_free_msghandle(mh_send_back[i]);
    QMP_free_msghandle(mh_from_fwd[i]);
    QMP_free_msghandle(mh_from_back[i]);
    QMP_free_msgmem(mm_send_fwd[i]);
    QMP_free_msgmem(mm_send_back[i]);
    QMP_free_msgmem(mm_from_back[i]);
    QMP_free_msgmem(mm_from_fwd[i]);
  }

#else

  for (int i=0; i<4; i++) {
    //printf("%d COPY length = %d\n", i, nbytes[i]/precision);
    memcpy(spinor.fwdGhostFaceBuffer[i], spinor.backGhostFaceSendBuffer[i], nbytes[i]);
    memcpy(spinor.backGhostFaceBuffer[i], spinor.fwdGhostFaceSendBuffer[i], nbytes[i]);
  }

#endif
}

void FaceBuffer::exchangeCpuLink(void** ghost_link, void** link_sendbuf) {

#ifdef QMP_COMMS

  QMP_msgmem_t mm_send_fwd[4];
  QMP_msgmem_t mm_from_back[4];
  QMP_msghandle_t mh_send_fwd[4];
  QMP_msghandle_t mh_from_back[4];

  for (int i=0; i<4; i++) {
    int len = 2*nFace*faceVolumeCB[i]*Ninternal;
    mm_send_fwd[i] = QMP_declare_msgmem(link_sendbuf[i], len*precision);
    if( mm_send_fwd[i] == NULL ) errorQuda("Unable to allocate send fwd message mem");
    
    mm_from_back[i] = QMP_declare_msgmem(ghost_link[i], len*precision);
    if( mm_from_back[i] == NULL ) errorQuda("Unable to allocate recv from back message mem");
    
    mh_send_fwd[i] = QMP_declare_send_relative(mm_send_fwd[i], i, +1, 0);
    if( mh_send_fwd[i] == NULL ) errorQuda("Unable to allocate forward send");
    
    mh_from_back[i] = QMP_declare_receive_relative(mm_from_back[i], i, -1, 0);
    if( mh_from_back[i] == NULL ) errorQuda("Unable to allocate backward recv");
  }

  for (int i=0; i<4; i++) {
    QMP_start(mh_send_fwd[i]);
    QMP_start(mh_from_back[i]);
  }

  for (int i=0; i<4; i++) {
    QMP_wait(mh_send_fwd[i]);
    QMP_wait(mh_from_back[i]);
  }

  for (int i=0; i<4; i++) {
    QMP_free_msghandle(mh_send_fwd[i]);
    QMP_free_msghandle(mh_from_back[i]);
    QMP_free_msgmem(mm_send_fwd[i]);
    QMP_free_msgmem(mm_from_back[i]);
  }

#else

  for(int dir =0; dir < 4; dir++) {
    int len = 2*nFace*faceVolumeCB[dir]*Ninternal; // factor 2 since we have both parities
    //printf("%d COPY length = %d\n", dir, len);
    memcpy(ghost_link[dir], link_sendbuf[dir], len*precision); 
  }

#endif

}



void transferGaugeFaces(void *gauge, void *gauge_face, QudaPrecision precision,
			int Nvec, ReconstructType reconstruct, int V, int Vs)
{
  int nblocks, ndim=4;
  size_t blocksize;//, nbytes;
  ptrdiff_t offset, stride;
  void *g;

  nblocks = ndim*reconstruct/Nvec;
  blocksize = Vs*Nvec*precision;
  offset = (V-Vs)*Nvec*precision;
  stride = (V+Vs)*Nvec*precision; // assume that pad = Vs

#ifdef QMP_COMMS

  QMP_msgmem_t mm_gauge_send_fwd;
  QMP_msgmem_t mm_gauge_from_back;
  QMP_msghandle_t mh_gauge_send_fwd;
  QMP_msghandle_t mh_gauge_from_back;

  g = (void *) ((char *) gauge + offset);
  mm_gauge_send_fwd = QMP_declare_strided_msgmem(g, blocksize, nblocks, stride);
  if (!mm_gauge_send_fwd) {
    errorQuda("Unable to allocate gauge message mem");
  }

  mm_gauge_from_back = QMP_declare_strided_msgmem(gauge_face, blocksize, nblocks, stride);
  if (!mm_gauge_from_back) { 
    errorQuda("Unable to allocate gauge face message mem");
  }

  mh_gauge_send_fwd = QMP_declare_send_relative(mm_gauge_send_fwd, 3, +1, 0);
  if (!mh_gauge_send_fwd) {
    errorQuda("Unable to allocate gauge message handle");
  }
  mh_gauge_from_back = QMP_declare_receive_relative(mm_gauge_from_back, 3, -1, 0);
  if (!mh_gauge_from_back) {
    errorQuda("Unable to allocate gauge face message handle");
  }

  QMP_start(mh_gauge_send_fwd);
  QMP_start(mh_gauge_from_back);
  
  QMP_wait(mh_gauge_send_fwd);
  QMP_wait(mh_gauge_from_back);

  QMP_free_msghandle(mh_gauge_send_fwd);
  QMP_free_msghandle(mh_gauge_from_back);
  QMP_free_msgmem(mm_gauge_send_fwd);
  QMP_free_msgmem(mm_gauge_from_back);

#else 

  void *gf;

  for (int i=0; i<nblocks; i++) {
    g = (void *) ((char *) gauge + offset + i*stride);
    gf = (void *) ((char *) gauge_face + i*stride);
    cudaMemcpy(gf, g, blocksize, cudaMemcpyHostToHost);
  }

#endif // QMP_COMMS
}

void reduceMaxDouble(double &max) {

#ifdef QMP_COMMS
  QMP_max_double(&max);
#endif

}

void reduceDouble(double &sum) {

#ifdef QMP_COMMS
  if (globalReduce) QMP_sum_double(&sum);
#endif

}

void reduceDoubleArray(double *sum, const int len) {

#ifdef QMP_COMMS
  if (globalReduce) QMP_sum_double_array(sum,len);
#endif

}

#ifdef QMP_COMMS
static int manual_set_partition[4] ={0, 0, 0, 0};
int commDim(int dir) { return QMP_get_logical_dimensions()[dir]; }
int commCoords(int dir) { return QMP_get_logical_coordinates()[dir]; }
int commDimPartitioned(int dir){ return (manual_set_partition[dir] || ((commDim(dir) > 1)));}
void commDimPartitionedSet(int dir){ manual_set_partition[dir] = 1; }
void commBarrier() { QMP_barrier(); }
#else
int commDim(int dir) { return 1; }
int commCoords(int dir) { return 0; }
int commDimPartitioned(int dir){ return 0; }
void commDimPartitionedSet(int dir){ ; }
void commBarrier() { ; }
#endif
