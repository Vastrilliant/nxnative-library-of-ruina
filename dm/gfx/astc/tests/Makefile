ASTCENC_DIR ?=
CFLAGS = -O2 -g -Wall -Wextra
CLANG = clang
CXXLIB ?= -lc++
ifeq ($(strip $(ASTCENC_DIR)),)
$(error ASTCENC_DIR is not set: the directory with include/astcenc.h and lib/libastcenc.a built for this machine)
endif
t_astc: t_astc.c ../astc_enc.c ../astc_enc.h
	$(CLANG) $(CFLAGS) -I$(ASTCENC_DIR)/include -o $@ t_astc.c ../astc_enc.c $(ASTCENC_DIR)/lib/libastcenc.a $(CXXLIB) -lm -lpthread
run: t_astc
	./t_astc
.PHONY: run
