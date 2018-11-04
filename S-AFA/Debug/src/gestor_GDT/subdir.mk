################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/gestor_GDT/gestor_GDT.c 

OBJS += \
./src/gestor_GDT/gestor_GDT.o 

C_DEPS += \
./src/gestor_GDT/gestor_GDT.d 


# Each subdirectory must supply rules for building sources it contributes
src/gestor_GDT/%.o: ../src/gestor_GDT/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -Igestor_GDT/gestor_GDT.h -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


