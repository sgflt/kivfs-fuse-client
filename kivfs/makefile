
-include ../makefile.init

RM := rm -rf
CC = clang
OBJDIR = ./obj
BINDIR = ./bin

# All of the sources participating in the build are defined here
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: kivfs

kivfs: $(OBJDIR) $(BINDIR) ./bin/kivfs

$(OBJDIR):		
	mkdir $(OBJDIR)
	
$(BINDIR):	
	mkdir $(BINDIR)
	
# Tool invocations
./bin/kivfs: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: C Linker'
	$(CC) -o "./bin/kivfs" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJDIR) $(BINDIR)
	-@echo ' '

libinstall:
	@sudo apt-get install e2fslibs-dev libssl-dev libkrb5 heimdal-dev libsqlite3-dev
	

.PHONY: all clean dependents kivfs libinstall
.SECONDARY:

-include ../makefile.targets