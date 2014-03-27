# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
./src/main.c \
./src/kivfs_operations.c \
./src/kivfs_remote_operations.c \
./src/kivfs_open_handling.c \
./src/config.c \
./src/cache.c \
./src/prepared_stmts.c \
./src/connection.c \
./src/tools.c \
./src/cleanup.c \
./src/stats.c \
./src/heap.c

OBJS += \
$(OBJDIR)/main.o \
$(OBJDIR)/kivfs_operations.o \
$(OBJDIR)/kivfs_remote_operations.o \
$(OBJDIR)/kivfs_open_handling.o \
$(OBJDIR)/config.o \
$(OBJDIR)/cache.o \
$(OBJDIR)/prepared_stmts.o \
$(OBJDIR)/connection.o \
$(OBJDIR)/tools.o \
$(OBJDIR)/cleanup.o \
$(OBJDIR)/stats.o \
$(OBJDIR)/heap.o

C_DEPS += \
main.d \
kivfs_operations.d \
kivfs_remote_operations.d \
kivfs_open_handling.d \
config.d \
cache.d \
prepared_stmts.d \
connection.d \
tools.d \
cleanup.d \
stats.d \
heap.d


# Each subdirectory must supply rules for building sources it contributes
./obj/%.o: ./src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: C Comoiler'
	$(CC) -O0 -ggdb3 -Wall -pedantic -std=gnu11 -c -fmessage-length=0 `pkg-config fuse sqlite3 libssl --cflags` -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '
	
