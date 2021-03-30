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

#Esctructura de directorios del proyecto
SRCDIR = src
OBJDIR = obj
INCDIR = include
TESTSDIR = tests
BINDIR = bin

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

#Especificar aqui donde esta el sysroot de la Jetson (de preferencia camino absoluto)
TARGET_ROOTFS := /home/rafael/jetson-sysroot

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
INCLUDE = \
    -I"$(INCDIR)" \
	-I"$(TARGET_ROOTFS)/usr/include/$(TEGRA_ARMABI)" 

# Opciones del compilador
CFLAGS += \
			-std=gnu99 \
		    -Wall \
			-Wextra \
			-Wno-unused-variable \
			-Wno-unused-function \
			-Wno-unused-parameter\
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

SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.c=.o)))

all: $(OBJS)

clean:
	@rm -f $(OBJDIR)/*.o
	@rm -f $(BINDIR)/*.arm64

#Programas de prueba
axis: $(OBJS) 
	@echo "Compiling Axis_test.c"
	$(CC) $(CFLAGS) $(INCLUDE) -c $(TESTSDIR)/Axis_test.c -o $(OBJDIR)/Axis_test.o
	@echo "Linking axis_test.arm64"
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) $(OBJS) $(OBJDIR)/Axis_test.o $(LDFLAGS) -o $(BINDIR)/axis_test.arm64	

gpio: $(OBJS)
	@echo "Compiling GPIO_test.c"
	$(CC) $(CFLAGS) $(INCLUDE) -c $(TESTSDIR)/GPIO_test.c -o $(OBJDIR)/GPIO_test.o
	@echo "Linking gpio_test.arm64"
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) $(OBJS) $(OBJDIR)/GPIO_test.o $(LDFLAGS) -o $(BINDIR)/gpio_test.arm64

stepper: $(OBJS)
	@echo "Compiling Stepper_test.c"
	$(CC) $(CFLAGS) $(INCLUDE) -c $(TESTSDIR)/Stepper_test.c -o $(OBJDIR)/Stepper_test.o
	@echo "Linking stepper_test.arm64"
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) $(OBJS) $(OBJDIR)/Stepper_test.o $(LDFLAGS) -o $(BINDIR)/stepper_test.arm64

time: $(OBJS)
	@echo "Compiling Time_test.c"
	$(CC) $(CFLAGS) $(INCLUDE) -c $(TESTSDIR)/Time_test.c -o $(OBJDIR)/Time_test.o
	@echo "Linking time_test.arm64"
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) $(OBJS) $(OBJDIR)/Time_test.o $(LDFLAGS) -o $(BINDIR)/time_test.arm64

#Comandos principales de compilacion

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo "Compiling: $<"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@
