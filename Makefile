# Smartlogin Makefile
#
# Notes:
# - If "BUILDSTAMP" is defined the created package will be
#   "smartlogin-$BUILDSTAMP.tgz", else the default "smartlogin.tgz".
# - The "publish" target requires that "BITS_DIR" be defined. The
#   target will then publish to "$BITS_DIR/smartlogin/".
#

NAME=smartlogin
ifeq ($(BUILDSTAMP),"")
	TARBALL=smartlogin.tgz
else
	TARBALL=smartlogin-$(BUILDSTAMP).tgz
endif


CC	= gcc
CCFLAGS	= -fPIC -g -Wall
LDFLAGS	= -L/lib -static-libgcc

AGENT := ${NAME}
AGENT_SRC = \
	src/agent/capi.c 	\
	src/agent/config.c 	\
	src/agent/hash.c 	\
	src/agent/list.c	\
	src/agent/log.c		\
	src/agent/lru.c		\
	src/agent/server.c	\
	src/agent/util.c	\
	src/agent/zutil.c

# ARG! Some versions of solaris have curl 3, some curl 4,
# so pick up the specific version
AGENT_LIBS = -lzdoor -lzonecfg /usr/lib/libcurl.so.4

NPM_FILES =		\
	bin		\
	etc		\
	npm-scripts	\
	package.json

.PHONY: all clean distclean npm

all:: npm

${AGENT}:
	mkdir -p bin
	${CC} ${CCFLAGS} ${LDFLAGS} -o bin/$@ $^ ${AGENT_SRC} ${AGENT_LIBS}

lint:
	for file in ${AGENT_SRC} ; do \
		echo $$file ; \
		lint -Isrc/agent -uaxm -m64 $$file ;  \
		echo "--------------------" ; \
	done

$(TARBALL): ${AGENT} $(NPM_FILES)
	rm -fr .npm
	mkdir -p .npm/$(NAME)/
	cp -Pr $(NPM_FILES) .npm/$(NAME)/
	cd .npm && gtar zcvf ../$(TARBALL) $(NAME)

npm: $(TARBALL)

publish: $(BITS_DIR)
	mkdir -p $(BITS_DIR)/smartlogin
	cp $(TARBALL) $(BITS_DIR)/smartlogin/

clean:
	-rm -rf bin .npm core $~ smartlogin*.tgz ${AGENT}
	find . -name *.o | xargs rm -f

distclean: clean
