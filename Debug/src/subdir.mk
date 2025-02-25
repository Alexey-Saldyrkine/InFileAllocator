################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/launchTests.cpp 

CPP_DEPS += \
./src/launchTests.d 

OBJS += \
./src/launchTests.o 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp src/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -I/home/alexey/googletest-1.15.2/googletest/include -O0 -g3 -Wall -c -fmessage-length=0 -std=c++1z -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-src

clean-src:
	-$(RM) ./src/launchTests.d ./src/launchTests.o

.PHONY: clean-src

