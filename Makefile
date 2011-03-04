NAME=smartlogin
TARBALL=smart-login.tgz
VERSION=$(shell git describe --tags)
VERSION=0.1
BASEDIR=/opt

ifeq ($(VERSION), "")
	@echo "Use gmake"
endif

CC	= gcc
CCFLAGS	= -fPIC -g -Wall
LDFLAGS	= -L/lib -static-libgcc

AGENT := ${NAME}
AGENT_SRC = \
	src/agent/server.c 	\
	src/agent/config.c 	\
	src/agent/zones.c
# ARG! Some versions of solaris have curl 3, some curl 4,
# so pick up the specific version
AGENT_LIBS = -lzdoor -lzonecfg /usr/lib/libcurl.so.4 -ldoor

CLIENT := notify-${NAME}
CLIENT_SRC = src/client/client.c
CLIENT_LIBS = -ldoor

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

${CLIENT}:
	mkdir -p bin
	${CC} ${CCFLAGS} ${LDFLAGS} -o bin/$@ $^ ${CLIENT_SRC} ${CLIENT_LIBS}

$(TARBALL): ${CLIENT} ${AGENT} $(NPM_FILES)
	rm -fr .npm
	mkdir -p .npm/$(NAME)/
	cp -Pr $(NPM_FILES) .npm/$(NAME)/
	cd .npm && gtar zcvf ../$(TARBALL) $(NAME)

npm: $(TARBALL)

clean:
	-rm -rf bin .npm core $~ ${TARBALL} ${AGENT}
	find . -name *.o | xargs rm -f

distclean: clean
