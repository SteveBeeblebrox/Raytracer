TARGET := raytracer
CUDA_ARCH := 75
CFLAGS := -std=c++17 -Xcompiler '-Wall -O3' -gencode arch=compute_$(CUDA_ARCH),code=compute_$(CUDA_ARCH) -gencode arch=compute_$(CUDA_ARCH),code=sm_$(CUDA_ARCH)
LFLAGS := -Xcompiler '-Wall'
DEBUG_FLAGS := -Xcompiler '-DDEBUG -g -O0'
INCLUDE := 
CXX := nvcc

include ./Makefile.base