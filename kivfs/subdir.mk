# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
./src/main.c \
./lib/sqlite3.c \
./src/kivfs_operations.c

OBJS += \
$(OBJDIR)/main.o \
$(OBJDIR)/sqlite3.o \
$(OBJDIR)/kivfs_operations.o

C_DEPS += \
main.d \
sqlite3.d \
kivfs_operations.d


# Each subdirectory must supply rules for building sources it contributes
./obj/%.o: ./src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: C Compiler'
	$(CC) -O0 -ggdb3 -Wall -pedantic -c -fmessage-length=0 `pkg-config fuse --cflags` -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '
	
./obj/%.o: ./lib/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: C Compiler'
	$(CC) -O2 -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '