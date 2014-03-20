
-include ../makefile.init

# Parameters necessary for creating a debian package
DEBPREFIX = kivfs-fuse
TMP = /tmp
SNDBOX = $(TMP)/$(DEBPREFIX)-$(VERSION)
ARCHIVE = $(DEBPREFIX)-$(VERSION).tar.bz2
ORIG = $(DEBPREFIX)-$(VERSION).orig.tar.bz2

# install directories
BIN = $(DESTDIR)/usr/bin
MAN = $(DESTDIR)/usr/share/man


# local build
RM = rm -rf
CC = clang #gcc -std=gnu11
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

dependents:
	@sudo apt-get install libfuse-dev libssl-dev libsqlite3-dev heimdal-dev pkg-config

#############################################################################
##
##	DEB
##

# Some environment variables are necessary for propper configuration:	
#	export DEBEMAIL - email of package maintainer
#	export DEBFULLNAME - name of maintainer
deb: $(SNDBOX) $(TMP)/$(ARCHIVE)
	@cd $(SNDBOX); \
	dh_make -c gpl3 -f ../$(ARCHIVE); \
	rm debian/*.{ex,EX}; \
	debuild

# Create archive containing source files
$(TMP)/$(ARCHIVE):
	@cp -r ./src ./makefile ./*.mk  $(SNDBOX) # copy source files
	cd $(TMP); \
	tar -cjvf $(ARCHIVE) $(DEBPREFIX)-$(VERSION) # create upstream archive from sandbox
		
# Create sandbox directory
$(SNDBOX):
	@mkdir $(SNDBOX)

# Remove all garbage
debclean:
	$(RM) $(TMP)/$(DEBPREFIX)*
	
install: all
	@cd $(SNDBOX); \
	install -d -m 644 $(BIN); \
	install -s -m 755 $(BINDIR)/kivfs $(BIN)

##
## END DEB
##
##############################################################################

.PHONY: all clean dependents kivfs install deb
.SECONDARY:

-include ../makefile.targets
