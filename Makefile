#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

#
# Copyright 2020 Joyent, Inc.
#

#
# For historical reasons*, this is dependent upon platform bits such as libcurl
# and libzdoor, but instead of being compiled in that context, it's done
# separately, using pkgsrc compilers, and exciting things like
# "hack-platform-include".  Hold onto your hat.
#

NAME=smartlogin
TOP := $(shell pwd)

ENGBLD_REQUIRE := $(shell git submodule update --init deps/eng)
include ./deps/eng/tools/mk/Makefile.defs
include ./deps/eng/tools/mk/Makefile.ctf.defs
TOP ?= $(error Unable to access eng.git submodule Makefiles.)

# While this component doesn't require a base image, we set this so
# that validate-buildenv can determine the correct build image, aka
# triton-origin-x86_64-18.4.0
BASE_IMAGE_UUID = a9368831-958e-432d-a031-f8ce6768d190


BASE=$(NAME)-$(STAMP)
TARBALL=$(BASE).tgz
MANIFEST=$(BASE).manifest

CC	= gcc -m32
CCFLAGS	= -fPIC -g -Wall -Werror -I$(TOP)/hack-platform-include

AGENT := bin/$(NAME)
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

AGENT_LIBS = /usr/lib/libcurl.so.4 -lnvpair -lzdoor -lzonecfg -lc

NPM_FILES =		\
	bin		\
	etc		\
	npm-scripts

CLEAN_FILES += bin .npm core $~ smartlogin*.tgz smartlogin*.manifest $(AGENT)

.PHONY: all clean npm
all: $(TARBALL)

#
# We're using the pkgsrc GCC; edit out the gcc libs RUNPATH entry, as
# they wouldn't apply to the resulting system anyway.
#
${AGENT}: $(AGENT_SRC) $(STAMP_CTF_TOOLS)
	mkdir -p bin
	$(CC) $(CCFLAGS) $(LDFLAGS) -o $@ $(AGENT_SRC) ${AGENT_LIBS}
	/usr/bin/elfedit -e 'dyn:delete RUNPATH' $@
	$(CTFCONVERT) $@

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

publish:
	mkdir -p $(ENGBLD_BITS_DIR)/smartlogin
	cp $(TARBALL) $(ENGBLD_BITS_DIR)/smartlogin/
	cp $(MANIFEST) $(ENGBLD_BITS_DIR)/smartlogin/

clean::
	find . -name *.o | xargs rm -f

include ./deps/eng/tools/mk/Makefile.targ
include ./deps/eng/tools/mk/Makefile.ctf.targ
