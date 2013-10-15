# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
./src/main.c \
./src/kivfs_operations.c \
./src/config.c \
./src/cache.c \
./src/prepared_stmts.c \
./src/connection.c \
./src/tools.c

OBJS += \
$(OBJDIR)/main.o \
$(OBJDIR)/kivfs_operations.o \
$(OBJDIR)/config.o \
$(OBJDIR)/cache.o \
$(OBJDIR)/prepared_stmts.o \
$(OBJDIR)/connection.o \
$(OBJDIR)/tools.o

C_DEPS += \
main.d \
kivfs_operations.d \
config.d \
cache.d \
prepared_stmts.d \
connection.d \
tools.d


# Each subdirectory must supply rules for building sources it contributes
./obj/%.o: ./src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: C Compiler'
	$(CC) -O0 -ggdb3 -Wall -pedantic -c -fmessage-length=0 `pkg-config fuse sqlite3 libssl --cflags` -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '
	