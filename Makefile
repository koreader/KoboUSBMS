# SPDX-License-Identifier: GPL-3.0-or-later
#
# Pickup our cross-toolchains automatically...
# c.f., http://trac.ak-team.com/trac/browser/niluje/Configs/trunk/Kindle/Misc/x-compile.sh
#       https://github.com/NiLuJe/crosstool-ng
#       https://github.com/koreader/koxtoolchain
# NOTE: We want the "bare" variant of the TC env, to make sure we vendor the right stuff...
#       i.e., source ~SVN/Configs/trunk/Kindle/Misc/x-compile.sh kobo env bare
ifdef CROSS_TC
	CC:=$(CROSS_TC)-gcc
	STRIP:=$(CROSS_TC)-strip
else
	CC?=gcc
	STRIP?=strip
endif

DEBUG_CFLAGS:=-Og -fno-omit-frame-pointer -pipe -g
# Fallback CFLAGS, we honor the env first and foremost!
OPT_CFLAGS:=-O2 -fomit-frame-pointer -pipe

# We need our bundled FBInk & libdevdev.
LIBS+=-l:libfbink.a -lm
LIBS+=-l:libevdev.a

# NOTE: Remember to use gdb -ex 'set follow-fork-mode child' to debug, since we fork like wild bunnies...
ifdef DEBUG
	OUT_DIR:=Debug
	CFLAGS:=$(DEBUG_CFLAGS)
	EXTRA_CFLAGS+=-DDEBUG
else
	OUT_DIR:=Release
	CFLAGS?=$(OPT_CFLAGS)
endif

# Moar warnings!
EXTRA_CFLAGS+=-Wall
EXTRA_CFLAGS+=-Wextra -Wunused
EXTRA_CFLAGS+=-Wformat=2
EXTRA_CFLAGS+=-Wformat-signedness
# NOTE: This doesn't really play nice w/ FORTIFY, leading to an assload of false-positives, unless LTO is enabled
ifneq (,$(findstring flto,$(CFLAGS)))
	EXTRA_CFLAGS+=-Wformat-truncation=2
else
	EXTRA_CFLAGS+=-Wno-format-truncation
endif
EXTRA_CFLAGS+=-Wnull-dereference
EXTRA_CFLAGS+=-Wuninitialized
EXTRA_CFLAGS+=-Wduplicated-branches -Wduplicated-cond
EXTRA_CFLAGS+=-Wundef
EXTRA_CFLAGS+=-Wbad-function-cast
EXTRA_CFLAGS+=-Wwrite-strings
EXTRA_CFLAGS+=-Wjump-misses-init
EXTRA_CFLAGS+=-Wlogical-op
EXTRA_CFLAGS+=-Wstrict-prototypes -Wold-style-definition
EXTRA_CFLAGS+=-Wshadow
EXTRA_CFLAGS+=-Wmissing-prototypes -Wmissing-declarations
EXTRA_CFLAGS+=-Wnested-externs
EXTRA_CFLAGS+=-Winline
EXTRA_CFLAGS+=-Wcast-qual
# NOTE: GCC 8 introduces -Wcast-align=strict to warn regardless of the target architecture (i.e., like clang)
EXTRA_CFLAGS+=-Wcast-align
EXTRA_CFLAGS+=-Wconversion
# Output padding info when debugging (NOTE: Clang is slightly more verbose)
# As well as function attribute hints
ifdef DEBUG
	EXTRA_CFLAGS+=-Wpadded
	EXTRA_CFLAGS+=-Wsuggest-attribute=pure -Wsuggest-attribute=const -Wsuggest-attribute=noreturn -Wsuggest-attribute=format -Wmissing-format-attribute
endif
# And disable this, because it obviously doesn't play well with using goto to handle cleanup on error codepaths...
EXTRA_CFLAGS+=-Wno-jump-misses-init

# A version tag...
USBMS_VERSION:=$(shell git describe)
EXTRA_CFLAGS+=-DUSBMS_VERSION='"$(USBMS_VERSION)"'
# A timestamp, formatted according to ISO 8601 (latest commit)...
USBMS_TIMESTAMP:=$(shell git show -s --format=%ci)
USBMS_EPOCH:=$(shell git show -s --format=%ct)
# NOTE: We used to use __DATE__ @ __TIME__ (i.e., the build date), which we can format the same way like so:
#       date +'%Y-%m-%d %H:%M:%S %z'
#       If, instead, we'd want to emulate __TIMESTAMP__ (i.e., modification date of the file):
#       date +'%Y-%m-%d %H:%M:%S %z' -r usbms.c
# NOTE: If we ever need to tweak git's formatting:
#       git show -s --format=%cd --date=format:'%Y-%m-%d @ %H:%M:%S %z' master
EXTRA_CFLAGS+=-DUSBMS_TIMESTAMP='"$(USBMS_TIMESTAMP)"'

# NOTE: Always use as-needed to avoid unecessary DT_NEEDED entries :)
LDFLAGS?=-Wl,--as-needed

# Pick up our vendored build of libevdev
EXTRA_CPPFLAGS:=-Ilibevdev-staged/include/libevdev-1.0
EXTRA_LDFLAGS:=-Llibevdev-staged/lib

# And pick up FBInk, too.
ifdef DEBUG
	EXTRA_LDFLAGS+=-LFBInk/Debug
else
	EXTRA_LDFLAGS+=-LFBInk/Release
	EXTRA_CPPFLAGS+=-DNDEBUG
endif

# We already enforce that in FBInk & USBMS, so, follow suit everywhere
EXTRA_CPPFLAGS+=-D_GNU_SOURCE

# Enforce LTO to enjoy more efficient DCE, since we link everything statically
ifeq (,$(findstring flto,$(CFLAGS)))
	LTO_JOBS:=$(shell getconf _NPROCESSORS_ONLN 2> /dev/null || sysctl -n hw.ncpu 2> /dev/null || echo 1)
	EXTRA_CFLAGS+=-flto=$(LTO_JOBS) -fuse-linker-plugin
endif

##
# Now that we're done fiddling with flags, let's build stuff!
SRCS:=usbms.c

default: vendored

OBJS:=$(addprefix $(OUT_DIR)/, $(SRCS:.c=.o))

$(OUT_DIR)/%.o: %.c
	$(CC) $(CPPFLAGS) $(EXTRA_CPPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) $(QUIET_CFLAGS) -o $@ -c $<

outdir:
	mkdir -p $(OUT_DIR)

# Make absolutely sure we create our output directories first, even with unfortunate // timings!
# c.f., https://www.gnu.org/software/make/manual/html_node/Prerequisite-Types.html#Prerequisite-Types
$(OBJS): | outdir

all: usbms

vendored: libevdev.built fbink.built
	$(MAKE) usbms

usbms: $(OBJS) $(INIH_OBJS) $(STR5_OBJS) $(SSH_OBJS)
	$(CC) $(CPPFLAGS) $(EXTRA_CPPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) $(EXTRA_LDFLAGS) -o$(OUT_DIR)/$@$(BINEXT) $(OBJS) $(LIBS)

strip: all
	$(STRIP) --strip-unneeded $(OUT_DIR)/usbms

armcheck:
ifeq (,$(findstring arm-,$(CC)))
	$(error You forgot to setup a cross TC, you dummy!)
endif

kobo: armcheck release
	mkdir -p Kobo/scripts Kobo/resources/img Kobo/resources/fonts
	ln -sf $(CURDIR)/scripts/start-usbms.sh Kobo/scripts/start-usbms.sh
	ln -sf $(CURDIR)/scripts/end-usbms.sh Kobo/scripts/end-usbms.sh
	ln -sf $(CURDIR)/scripts/fuser-check.sh Kobo/scripts/fuser-check.sh
	ln -sf $(CURDIR)/resources/img/koreader.png Kobo/resources/img/koreader.png
	ln -sf $(CURDIR)/resources/fonts/CaskaydiaCove_NF.ttf Kobo/resources/fonts/CaskaydiaCove_NF.ttf
	ln -sf $(CURDIR)/$(OUT_DIR)/usbms Kobo/usbms
	ln -sf $(CURDIR)/l10n Kobo/l10n
	tar --mtime=@$(USBMS_EPOCH) --owner=root --group=root -cvzhf $(OUT_DIR)/KoboUSBMS.tar.gz -C Kobo .

pot:
	mkdir -p po/templates
	xgettext --from-code=utf-8 usbms.c -d usbms -p po -o templates/usbms.pot --keyword=_ --add-comments=@translators
	# Workflow example:
	#mkdir -p po/fr l10n/fr/LC_MESSAGES
	#msginit -i po/templates/usbms.pot -l fr_FR.UTF-8 -o po/fr/usbms.po
	#msgfmt po/fr/usbms.po -o l10n/fr/LC_MESSAGES/usbms.mo
	#
	#msgmerge -U po/fr/usbms.po po/templates/usbms.pot
	#rm -f po/fr/usbms.po~
	#msgfmt po/fr/usbms.po -o l10n/fr/LC_MESSAGES/usbms.mo

clean:
	rm -rf Release/*.o
	rm -rf Release/usbms
	rm -rf Release/KoboRoot.tgz
	rm -rf Debug/*.o
	rm -rf Debug/usbms
	rm -rf Kobo

libevdev.built:
	mkdir -p libevdev-staged
	cd libevdev && \
	autoreconf -fi && \
	./configure $(if $(CROSS_TC),--host=$(CROSS_TC),) \
	--prefix="$(CURDIR)/libevdev-staged" \
	--enable-static \
	--disable-shared && \
	$(MAKE) install
	touch libevdev.built

fbink.built:
	cd FBInk && \
	$(MAKE) strip KOBO=1 MINIMAL=1 IMAGE=1 OPENTYPE=1
	touch fbink.built

release: libevdev.built fbink.built
	$(MAKE) strip

debug: libevdev.built
	cd FBInk && \
	$(MAKE) debug KOBO=1 MINIMAL=1 IMAGE=1 OPENTYPE=1
	touch fbink.built
	$(MAKE) all DEBUG=1

fbinkclean:
	cd FBInk && \
	$(MAKE) clean

libevdevclean:
	cd libevdev && \
	git reset --hard && \
	git clean -fxdq

distclean: clean libevdevclean fbinkclean
	rm -rf libevdev-staged
	rm -rf libevdev.built
	rm -rf fbink.built

.PHONY: default outdir all vendored usbms strip armcheck kobo pot debug clean release fbinkclean libevdevclean distclean
