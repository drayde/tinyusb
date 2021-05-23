include ../../../tools/top.mk
include ../../make.mk

INC += \
  src \
  $(TOP)/hw \

# source
EXAMPLE_SOURCE += $(wildcard src/*.c)
SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

SRC_C += \
  $(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_dma.c \
  $(ST_HAL_DRIVER)/Src/stm32$(ST_FAMILY)xx_hal_uart.c 
  
include ../../rules.mk
