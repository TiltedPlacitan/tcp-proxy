BUILD_ROOT = ..

include $(BUILD_ROOT)/build/make.defines

BINARY = bin/proxy
all : libs
	$(MAKE) $(BINARY)

CPPFLAGS	+=											\
				$(LIBTHROCKET_INCLUDE)					\

#LDFLAGS		+=

CCSOURCES	=											\
				./src/proxy.cc							\

LIBDIRS		=											\
				$(LIBTHROCKET_DIR)						\
				

SYSLIBS		+=											\

include $(BUILD_ROOT)/build/make.rules
