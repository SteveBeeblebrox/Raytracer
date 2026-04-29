TARGET = raytracer
CFLAGS = -std=c++17 -Xcompiler '-Wall -MD -MP'
LFLAGS = -Xcompiler '-Wall'
DEPFLAGS =
INCLUDE =
CXX = nvcc

cpu:
	$(MAKE) $(TARGET) CXX=g++ CFLAGS="-Wall -std=c++17 -MD -MP -x c++" LFLAGS="-Wall"

include ./Makefile.base