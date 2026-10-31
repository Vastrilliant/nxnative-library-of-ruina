# Texture sub-module unit tests (seconds): make run
CFLAGS = -O2 -g -Wall -Wextra
# SAN: extra compiler flags, O: the output folder (with a trailing slash) of a sanitizer or coverage build (make asan and make coverage below set both); both empty otherwise
SAN ?=
O ?=
CLANG = clang $(SAN)
$(O)t_tex: t_tex.c ../nvn_deswizzle.c ../nvn_deswizzle.h ../../nvn/nvn_tex.c ../../nvn/nvn_tex.h
	$(CLANG) $(CFLAGS) -o $@ t_tex.c ../nvn_deswizzle.c ../../nvn/nvn_tex.c -lm
$(O)t_fmt_mtl: t_fmt_mtl.m ../nvn_fmt_mtl.c ../nvn_fmt_mtl.h
	$(CLANG) $(CFLAGS) -fobjc-arc -o $@ t_fmt_mtl.m ../nvn_fmt_mtl.c -framework Metal -framework Foundation
$(O)t_bcn: t_bcn.c ../bcn_decode.c ../bcn_decode.h
	$(CLANG) $(CFLAGS) -o $@ t_bcn.c ../bcn_decode.c
$(O)t_fmt_bc: t_fmt_bc.c ../nvn_fmt_mtl.c ../nvn_fmt_mtl.h ../bcn_decode.h
	$(CLANG) $(CFLAGS) -o $@ t_fmt_bc.c ../nvn_fmt_mtl.c
# t_astc_cache: the on-disk cache, alone and through nvn_bc_astc_image with a fake encoder (no astcenc needed)
$(O)t_astc_cache: t_astc_cache.c fake_astc_enc.c ../astc_cache.c ../astc_cache.h ../nvn_bc_astc.c ../nvn_bc_astc.h ../nvn_fmt_mtl.c ../bcn_decode.c
	$(CLANG) $(CFLAGS) -DDM_HAVE_ASTCENC=1 -o $@ t_astc_cache.c fake_astc_enc.c ../astc_cache.c ../nvn_bc_astc.c ../nvn_fmt_mtl.c ../bcn_decode.c -lpthread
# t_bc_astc needs astcenc: make run ASTCENC_DIR=<dir with include/astcenc.h and lib/libastcenc.a built for this machine> (CXXLIB=-lstdc++ off macOS)
ASTCENC_DIR ?=
CXXLIB ?= -lc++
ASTC_RUN =
ifneq ($(strip $(ASTCENC_DIR)),)
ASTC_RUN = run_bc_astc
$(O)t_bc_astc: t_bc_astc.c ../nvn_bc_astc.c ../nvn_bc_astc.h ../nvn_fmt_mtl.c ../bcn_decode.c ../../astc/astc_enc.c ../../astc/astc_enc.h
	$(CLANG) $(CFLAGS) -DDM_HAVE_ASTCENC=1 -I$(ASTCENC_DIR)/include -o $@ t_bc_astc.c ../nvn_bc_astc.c ../astc_cache.c ../nvn_fmt_mtl.c ../bcn_decode.c ../../astc/astc_enc.c $(ASTCENC_DIR)/lib/libastcenc.a $(CXXLIB) -lm -lpthread
run_bc_astc: t_bc_astc
	./t_bc_astc
.PHONY: run_bc_astc
endif
run: t_tex t_fmt_mtl t_bcn t_fmt_bc t_astc_cache $(ASTC_RUN)
	./t_fmt_mtl && ./t_tex && ./t_bcn && ./t_fmt_bc && ./t_astc_cache
.PHONY: run
# ASan and UBSan (../../../sanitizers/sanitizers.mk): the CPU test t_tex, built into asan/ (t_fmt_mtl creates Metal textures and stays local)
include ../../../sanitizers/sanitizers.mk
.PHONY: asan
asan:
	@$(call san_fresh,asan,$(ASAN_FLAGS))
	$(MAKE) O=asan/ SAN="$(ASAN_FLAGS)" asan/t_tex asan/t_bcn asan/t_fmt_bc asan/t_astc_cache
	$(ASAN_ENV) ./asan/t_tex
	$(ASAN_ENV) ./asan/t_bcn
	$(ASAN_ENV) ./asan/t_fmt_bc
	$(ASAN_ENV) ./asan/t_astc_cache
# Line coverage (../../../sanitizers/coverage.mk): t_tex built into cov/ with COV_FLAGS and run; writes cov/coverage.lcov
include ../../../sanitizers/coverage.mk
.PHONY: coverage
coverage:
	@$(call san_fresh,cov,$(COV_FLAGS))
	rm -rf cov/prof
	$(MAKE) O=cov/ SAN="$(COV_FLAGS)" cov/t_tex
	-LLVM_PROFILE_FILE="$(call cov_profile,cov)" ./cov/t_tex
	@$(call cov_export,cov)
