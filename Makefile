#########################################################################
# Makefile de compilación cruzada para la Jetson TX2.
# Para compilar exitosamente, se requiere una copia del sistema
# de archivos de la Jetson (/usr y /lib), y la toolchain para aarch64.
# Makefile basado en el de nvidia.
#
# Se compila lo que se encuentra en ./src, los binarios
# se generan en ./obj, y los ejecutables se guardan en ./bin. 
# Headers de clases y librerias hechas deben encontrarse en ./include.
########################################################################

#Nombre del ejecutable
APP = control

#Esctructura de directorios del proyecto
BASEDIR  = control
COREDIR  = core
SRCDIR   = src
OBJDIR   = obj
INCDIR   = include
BINDIR   = bin
TESTSDIR = tests

#Especificar aqui donde esta el sysroot de la Jetson (de preferencia camino absoluto)
TARGET_ROOTFS := /home/rafael/jetson-sysroot

# Clear the flags from env
CFLAGS :=
LDFLAGS :=

# Verbose flag
ifeq ($(VERBOSE), 1)
AT =
else
AT = @
endif

# ARM ABI of the target platform
ifeq ($(TEGRA_ARMABI),)
TEGRA_ARMABI ?= aarch64-linux-gnu
endif

# Location of the target rootfs
ifeq ($(shell uname -m), aarch64)
TARGET_ROOTFS :=
else
ifeq ($(TARGET_ROOTFS),)
$(error Please specify the target rootfs path if you are cross-compiling)
endif
endif

ifeq ($(shell uname -m), aarch64)
CROSS_COMPILE :=
else
#Especificar aqui la ubicación de la toolchain
CROSS_COMPILE ?= /usr/bin/aarch64-linux-gnu-
endif

#Comandos
AS             = $(AT) $(CROSS_COMPILE)as
LD             = $(AT) $(CROSS_COMPILE)ld
CC             = $(AT) $(CROSS_COMPILE)gcc
CPP            = $(AT) $(CROSS_COMPILE)g++
AR             = $(AT) $(CROSS_COMPILE)ar
NM             = $(AT) $(CROSS_COMPILE)nm
STRIP          = $(AT) $(CROSS_COMPILE)strip
OBJCOPY        = $(AT) $(CROSS_COMPILE)objcopy
OBJDUMP        = $(AT) $(CROSS_COMPILE)objdump

# Specify the logical root directory for headers and libraries.
ifneq ($(TARGET_ROOTFS),)
CFLAGS += --sysroot=$(TARGET_ROOTFS)
LDFLAGS += \
	-Wl,-rpath-link=$(TARGET_ROOTFS)/lib/$(TEGRA_ARMABI) \
	-Wl,-rpath-link=$(TARGET_ROOTFS)/usr/lib/$(TEGRA_ARMABI) \
	-Wl,-rpath-link=$(TARGET_ROOTFS)/usr/lib/$(TEGRA_ARMABI)/tegra 
endif

# Directorios con headers
INCLUDE = -I"$(BASEDIR)/$(INCDIR)" \
	      -I"$(COREDIR)/$(INCDIR)" \
	      -I"$(TARGET_ROOTFS)/usr/include/$(TEGRA_ARMABI)" 

# Opciones del compilador
CFLAGS += -std=gnu99 \
          -Wall \
		  -Wextra \
          -Wno-unused \
          -Werror  

# Directorios con librerias utilizadas
LIBDIRS = \
	-L"$(TARGET_ROOTFS)/usr/lib/$(TEGRA_ARMABI)" \
	-L"$(TARGET_ROOTFS)/usr/lib/$(TEGRA_ARMABI)/tegra" \
	-L"$(TARGET_ROOTFS)/lib/$(TEGRA_ARMABI)" 

#Librerias utilizadas
LIBS = \
	-lpthread -lgpiod

LDFLAGS += $(LIBDIRS) $(LIBS)

BASESRCS := $(wildcard $(BASEDIR)/$(SRCDIR)/*.c)
BASEOBJS := $(addprefix $(BASEDIR)/$(OBJDIR)/,$(notdir $(BASESRCS:.c=.o)))

CORESRCS := $(wildcard $(COREDIR)/$(SRCDIR)/*.c)
COREOBJS := $(addprefix $(COREDIR)/$(OBJDIR)/,$(notdir $(CORESRCS:.c=.o)))

all: $(APP).arm64

clean:
	@echo "Cleaning $(BASEDIR)/$(OBJDIR)"
	@rm -f $(BASEDIR)/$(OBJDIR)/*
	@echo "Cleaning $(BASEDIR)/$(BINDIR)"
	@rm -f $(BASEDIR)/$(BINDIR)/*
	@echo "Cleaning $(COREDIR)/$(OBJDIR)"
	@rm -f $(COREDIR)/$(OBJDIR)/*
	@echo "Cleaning $(COREDIR)/$(BINDIR)"
	@rm -f $(COREDIR)/$(BINDIR)/*
	@echo "Deleting main binary"
	@rm -f *.arm64

# Programas de prueba

config_test: $(BASEOBJS) $(COREOBJS)
	@echo "Compiling config_test.c"
	$(CC) $(CFLAGS) $(INCLUDE) -c $(BASEDIR)/$(TESTSDIR)/config_test.c -o $(BASEDIR)/$(OBJDIR)/config_test.o
	@echo "Linking config_test.arm64"
	@mkdir -p  $(BASEDIR)/$(BINDIR)
	$(CC) $(CFLAGS) $(BASEOBJS) $(COREOBJS) $(BASEDIR)/$(OBJDIR)/config_test.o $(LDFLAGS) -o $(BASEDIR)/$(BINDIR)/config_test.arm64	

ipc_test: $(BASEOBJS) $(COREOBJS)
	@echo "Compiling ipc_test.c"
	$(CC) $(CFLAGS) $(INCLUDE) -c $(BASEDIR)/$(TESTSDIR)/ipc_test.c -o $(BASEDIR)/$(OBJDIR)/ipc_test.o
	@echo "Linking ipc_test.arm64"
	@mkdir -p  $(BASEDIR)/$(BINDIR)
	$(CC) $(CFLAGS) $(BASEOBJS) $(COREOBJS) $(BASEDIR)/$(OBJDIR)/ipc_test.o $(LDFLAGS) -o $(BASEDIR)/$(BINDIR)/ipc_test.arm64	

# Compilacion principal

$(APP).arm64: $(BASEOBJS) $(COREOBJS)
	@echo "Linking $@"
	$(CC) $(CFLAGS) $(INCLUDE) $(BASEOBJS) $(COREOBJS) $(LDFLAGS) -o $@

$(BASEDIR)/$(OBJDIR)/%.o: $(BASEDIR)/$(SRCDIR)/%.c
	@echo "Compiling: $<"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(COREDIR)/$(OBJDIR)/%.o: $(COREDIR)/$(SRCDIR)/%.c
	@echo "Compiling: $<"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@
