################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/Parser/parser.c 

OBJS += \
./src/Parser/parser.o 

C_DEPS += \
./src/Parser/parser.d 


# Each subdirectory must supply rules for building sources it contributes
src/Parser/%.o: ../src/Parser/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I../../S-AFA/src/S-AFA.h -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


