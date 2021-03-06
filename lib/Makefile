include ../make.inc

QUDA = libquda.a
QUDA_OBJS = inv_bicgstab_quda.o inv_cg_quda.o inv_multi_cg_quda.o inv_gcr_quda.o inv_mr_quda.o \
	interface_quda.o util_quda.o hw_quda.o \
	blas_cpu.o clover_field.o color_spinor_field.o	\
	cpu_color_spinor_field.o cuda_color_spinor_field.o dirac.o	     \
	lattice_field.o gauge_field.o cpu_gauge_field.o cuda_gauge_field.o \
	dirac_clover.o dirac_wilson.o dirac_staggered.o dirac_domain_wall.o  \
	dirac_twisted_mass.o tune.o fat_force_quda.o hisq_force_utils.o \
	clover_quda.o dslash_quda.o blas_quda.o \
	${NUMA_AFFINITY_OBJS} ${FACE_COMMS_OBJS} ${FATLINK_ITF_OBJS}

# header files, found in include/
QUDA_HDRS = blas_quda.h clover_field.h color_spinor_field.h convert.h	\
	dirac_quda.h dslash_quda.h enum_quda.h gauge_force_quda.h	\
	invert_quda.h llfat_quda.h quda.h quda_internal.h util_quda.h	\
	face_quda.h tune_quda.h comm_quda.h lattice_field.h		\
	gauge_field.h hisq_force_utils.h double_single.h texture.h	\
	numa_affinity.h

# These are only inlined into blas_quda.cu
BLAS_INLN = blas_core.h reduce_core.h

# These are only inlined into cuda_color_spinor_field.cu
CSF_INLN = pack_spinor.h

# files containing complex macros and other code fragments to be inlined,
# found in lib/
QUDA_INLN = check_params.h clover_def.h dslash_constants.h dslash_textures.h \
	force_common.h io_spinor.h pack_gauge.h read_clover.h  \
	read_gauge.h staggered_dslash_def.h wilson_dslash_def.h              \
	dw_dslash_def.h tm_dslash_def.h pack_face_def.h

# files generated by the scripts in lib/generate/, found in lib/dslash_core/
# (The current clover_core.h, staggered_dslash_core.h, and dw_*_core.h were
#  edited by hand.)
QUDA_CORE = clover_core.h staggered_dslash_core.h			\
	dw_dslash_dagger_core.h dw_dslash_core.h 			\
	wilson_dslash_fermi_core.h wilson_dslash_dagger_fermi_core.h 	\
	tm_dslash_fermi_core.h tm_dslash_dagger_fermi_core.h		\
	wilson_dslash_gt200_core.h wilson_dslash_dagger_gt200_core.h 	\
	tm_dslash_gt200_core.h tm_dslash_dagger_gt200_core.h		\
	wilson_dslash_g80_core.h wilson_dslash_dagger_g80_core.h 	\
	tm_dslash_g80_core.h tm_dslash_dagger_g80_core.h		\
	wilson_pack_face_core.h wilson_pack_face_dagger_core.h

INC += -I../include -Idslash_core -I.

HDRS = $(QUDA_HDRS:%=../include/%)
HDRS += $(QUDA_INLN)

CORE = $(QUDA_CORE:%=dslash_core/%)

# various parameters that characterize this build, used by the
# routines in tune.cpp to check basic compatibility of an existing
# tunecache.  In the future, we might consider including actual hashes
# of certain source files.
HASH = \"cpu_arch=$(strip $(CPU_ARCH)),gpu_arch=$(strip $(GPU_ARCH))\"

# limit maximum number of registers in BLAS routines to increase occupancy
ifneq (,$(filter $(strip $(GPU_ARCH)),sm_20 sm_21 sm_30))
  MAXREG =
else
  MAXREG = --maxrregcount=80
endif

all: $(QUDA)

$(QUDA): $(QUDA_OBJS) ../make.inc
	ar cru $@ $(QUDA_OBJS)
gen:
	$(PYTHON) generate/dslash_cuda_gen.py

clean:
	-rm -f *.o $(QUDA)

tune.o: tune.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -DQUDA_HASH=$(HASH) $< -c -o $@

blas_quda.o: blas_quda.cu $(HDRS) $(BLAS_INLN)
	$(NVCC) $(NVCCFLAGS) $(MAXREG) $< -c -o $@ -Xptxas -abi=no

cuda_color_spinor_field.o: cuda_color_spinor_field.cu $(HDRS) $(CSF_INLN)
	$(NVCC) $(NVCCFLAGS) $< -c -o $@

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) $< -c -o $@

%.o: %.cu $(HDRS) $(CORE)
	$(NVCC) $(NVCCFLAGS) $< -c -o $@

.PHONY: all gen clean
