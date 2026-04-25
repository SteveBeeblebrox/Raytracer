TARGET = raytracer
CFLAGS = -Xcompiler '-Wall -std=c++17'
LFLAGS = -Xcompiler '-Wall'
INCLUDE = 
CXX=nvcc

include ./Makefile.base