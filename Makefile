#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

#
# Copyright (c) 2019, Joyent, Inc.
#

# Smartlogin Makefile

NAME=smartlogin
TOP := $(shell pwd)

ENGBLD_REQUIRE := $(shell git submodule update --init deps/eng)
include ./deps/eng/tools/mk/Makefile.defs
TOP ?= $(error Unable to access eng.git submodule Makefiles.)

# While this component doesn't require a base image, we set this so
# that validate-buildenv can determine whether we're building on
# smartos 1.6.3, currently a requirement.
BASE_IMAGE_UUID=fd2cc906-8938-11e3-beab-4359c665ac99


BASE=$(NAME)-$(STAMP)
TARBALL=$(BASE).tgz
MANIFEST=$(BASE).manifest

CC	= gcc
CCFLAGS	= -fPIC -g -Wall -Werror -I$(TOP)/hack-platform-include
LDFLAGS	= -nodefaultlibs -L/lib -L/usr/lib -lc -lnvpair

AGENT := ${NAME}
AGENT_SRC = \
	src/agent/bunyan.c 	\
	src/agent/capi.c 	\
	src/agent/config.c 	\
	src/agent/hash.c 	\
	src/agent/list.c	\
	src/agent/lru.c		\
	src/agent/nvpair_json.c	\
	src/agent/server.c	\
	src/agent/util.c	\
	src/agent/zutil.c

# ARG! Some versions of solaris have curl 3, some curl 4,
# so pick up the specific version
AGENT_LIBS = -lzdoor -lzonecfg /usr/lib/libcurl.so.4

NPM_FILES =		\
	bin		\
	etc		\
	npm-scripts

CLEAN_FILES += bin .npm core $~ smartlogin*.tgz smartlogin*.manifest ${AGENT}

.PHONY: all clean npm
all: npm

${AGENT}:
	mkdir -p bin
	${CC} ${CCFLAGS} ${LDFLAGS} -o bin/$@ $^ ${AGENT_SRC} ${AGENT_LIBS}
	if /usr/bin/elfdump -d bin/$@ | egrep 'RUNPATH|RPATH'; then \
		echo "Your compiler is inserting an errant RPATH/RUNPATH" >&2; \
		exit 1; \
	fi

lint:
	for file in ${AGENT_SRC} ; do \
		echo $$file ; \
		lint -Isrc/agent -uaxm -m64 $$file ;  \
		echo "--------------------" ; \
	done

$(NPM_FILES):
	mkdir -p $@

$(TARBALL): ${AGENT} $(NPM_FILES) package.json
	rm -fr .npm
	mkdir -p .npm/$(NAME)/
	cp -Pr $(NPM_FILES) .npm/$(NAME)/
	uuid -v4 > .npm/$(NAME)/image_uuid
	json -f package.json -e 'this.version += "-$(STAMP)"' \
	    > .npm/$(NAME)/package.json
	(cd .npm && gtar -I pigz -cvf ../$(TARBALL) $(NAME))
	cat $(TOP)/manifest.tmpl | sed \
		-e "s/UUID/$$(cat .npm/$(NAME)/image_uuid)/" \
		-e "s/NAME/$$(json name < .npm/$(NAME)/package.json)/" \
		-e "s/VERSION/$$(json version < .npm/$(NAME)/package.json)/" \
		-e "s/DESCRIPTION/$$(json description < .npm/$(NAME)/package.json)/" \
		-e "s/BUILDSTAMP/$(STAMP)/" \
		-e "s/SIZE/$$(stat --printf="%s" $(TARBALL))/" \
		-e "s/SHA/$$(openssl sha1 $(TARBALL) \
		    | cut -d ' ' -f2)/" \
		> $(MANIFEST)

npm: $(TARBALL)

publish:
	mkdir -p $(ENGBLD_BITS_DIR)/smartlogin
	cp $(TARBALL) $(ENGBLD_BITS_DIR)/smartlogin/
	cp $(MANIFEST) $(ENGBLD_BITS_DIR)/smartlogin/

clean::
	find . -name *.o | xargs rm -f

include ./deps/eng/tools/mk/Makefile.targ
