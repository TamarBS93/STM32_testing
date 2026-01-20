################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../SW/Src/adcs.c \
../SW/Src/i2cs.c \
../SW/Src/spis.c \
../SW/Src/timer_test.c \
../SW/Src/uarts.c 

OBJS += \
./SW/Src/adcs.o \
./SW/Src/i2cs.o \
./SW/Src/spis.o \
./SW/Src/timer_test.o \
./SW/Src/uarts.o 

C_DEPS += \
./SW/Src/adcs.d \
./SW/Src/i2cs.d \
./SW/Src/spis.d \
./SW/Src/timer_test.d \
./SW/Src/uarts.d 


# Each subdirectory must supply rules for building sources it contributes
SW/Src/%.o SW/Src/%.su SW/Src/%.cyclo: ../SW/Src/%.c SW/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F756xx -c -I../Core/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -I../LWIP/App -I../LWIP/Target -I../Middlewares/Third_Party/LwIP/src/include -I../Middlewares/Third_Party/LwIP/system -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1 -I../Drivers/BSP/Components/lan8742 -I../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../Middlewares/Third_Party/LwIP/src/include/lwip -I../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../Middlewares/Third_Party/LwIP/src/include/netif -I../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../Middlewares/Third_Party/LwIP/system/arch -I"D:/Tamar/ARM/projects/Final_Project/LWIP_v2/SW/Inc" -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-SW-2f-Src

clean-SW-2f-Src:
	-$(RM) ./SW/Src/adcs.cyclo ./SW/Src/adcs.d ./SW/Src/adcs.o ./SW/Src/adcs.su ./SW/Src/i2cs.cyclo ./SW/Src/i2cs.d ./SW/Src/i2cs.o ./SW/Src/i2cs.su ./SW/Src/spis.cyclo ./SW/Src/spis.d ./SW/Src/spis.o ./SW/Src/spis.su ./SW/Src/timer_test.cyclo ./SW/Src/timer_test.d ./SW/Src/timer_test.o ./SW/Src/timer_test.su ./SW/Src/uarts.cyclo ./SW/Src/uarts.d ./SW/Src/uarts.o ./SW/Src/uarts.su

.PHONY: clean-SW-2f-Src

