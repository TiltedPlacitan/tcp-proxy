BUILD_ROOT = .

include $(BUILD_ROOT)/build/make.defines

all : submodules libs sos binaries

SUBMODULEDIRS		=											\

OPENSOURCEDIRS		=											\

LIBDIRS				= 											\
						$(LIBK_DIR)								\

SODIRS				=											\
						
BINARYDIRS			=											\
						$(PROXY_DIR)							\

installtar :
	$(MAKE)
	mkdir -p JACK-install
	rm -f JACK-install/*.tar.gz
	
	$(foreach sodir,$(SODIRS),$(MAKE) -C $(sodir) installtar && ) true
	$(foreach sodir,$(SODIRS),mv $(sodir)/JACK-install/*.tar.gz JACK-install && ) true
	$(foreach binarydir,$(BINARYDIRS),$(MAKE) -C $(binarydir) installtar && ) true
	$(foreach binarydir,$(BINARYDIRS),mv $(binarydir)/JACK-install/*.tar.gz JACK-install && ) true

install :
	$(MAKE)

packages :
	sudo apt install binutils-dev libssl-dev libogg-dev libtheora-dev libjpeg-dev binutils-dev libssl-dev

include $(BUILD_ROOT)/build/make.rules
