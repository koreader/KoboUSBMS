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
LIBS+=-l:libfbink.a
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

# And we need -lrt for clock_gettime, as our TC is targeting glibc 2.15, and it was in there before glibc 2.17...
# Yes, Kobo FW have since moved to glibc 2.19, but that's recent (-ish), and we want binaries that will work on earlier FW than that.
LIBS+=-lrt

# We already enforce that in FBInk & USBMS, so, follow suit everywhere
EXTRA_CPPFLAGS+=-D_GNU_SOURCE


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

# FIXME: Fix this
# FIXME: Also enforce LTO for DCE
# FIXME: Reproducible tarball via --mtime set to commit's epoch
kobo: armcheck release
	mkdir -p Kobo/usr/local/kfmon/bin Kobo/usr/bin Kobo/etc/udev/rules.d Kobo/etc/init.d
	ln -sf $(CURDIR)/scripts/99-kfmon.rules Kobo/etc/udev/rules.d/99-kfmon.rules
	ln -sf $(CURDIR)/scripts/uninstall/kfmon-uninstall.sh Kobo/usr/local/kfmon/bin/kfmon-update.sh
	ln -sf $(CURDIR)/scripts/uninstall/on-animator.sh Kobo/etc/init.d/on-animator.sh
	tar --exclude="./mnt" --exclude="KFMon-*.zip" --owner=root --group=root -cvzhf Release/KoboRoot.tgz -C Kobo .
	pushd Release && zip ../Kobo/KFMon-Uninstaller.zip KoboRoot.tgz && popd
	rm -f Release/KoboRoot.tgz
	rm -rf Kobo/usr/local/kfmon/bin Kobo/etc/udev/rules.d Kobo/etc/init.d
	mkdir -p Kobo/usr/local/kfmon/bin Kobo/mnt/onboard/.kobo Kobo/etc/udev/rules.d Kobo/etc/init.d Kobo/mnt/onboard/.adds/kfmon/config Kobo/mnt/onboard/.adds/kfmon/bin Kobo/mnt/onboard/.adds/kfmon/log Kobo/mnt/onboard/icons
	ln -f $(CURDIR)/resources/koreader.png Kobo/mnt/onboard/koreader.png
	ln -f $(CURDIR)/resources/plato.png Kobo/mnt/onboard/icons/plato.png
	ln -f $(CURDIR)/resources/kfmon.png Kobo/mnt/onboard/kfmon.png
	ln -f $(CURDIR)/Release/kfmon Kobo/usr/local/kfmon/bin/kfmon
	ln -f $(CURDIR)/Release/shim Kobo/usr/local/kfmon/bin/shim
	ln -f $(CURDIR)/Release/kfmon-ipc Kobo/usr/local/kfmon/bin/kfmon-ipc
	ln -sf /usr/local/kfmon/bin/kfmon-ipc Kobo/usr/bin/kfmon-ipc
	ln -f $(CURDIR)/FBInk/Release/fbink Kobo/usr/local/kfmon/bin/fbink
	ln -f $(CURDIR)/README.md Kobo/usr/local/kfmon/README.md
	ln -f $(CURDIR)/LICENSE Kobo/usr/local/kfmon/LICENSE
	ln -f $(CURDIR)/CREDITS Kobo/usr/local/kfmon/CREDITS
	ln -f $(CURDIR)/scripts/99-kfmon.rules Kobo/etc/udev/rules.d/99-kfmon.rules
	ln -f $(CURDIR)/scripts/kfmon-update.sh Kobo/usr/local/kfmon/bin/kfmon-update.sh
	ln -f $(CURDIR)/scripts/on-animator.sh Kobo/etc/init.d/on-animator.sh
	tar --exclude="./mnt" --exclude="KFMon-*.zip" --owner=root --group=root --hard-dereference -cvzf Release/KoboRoot.tgz -C Kobo .
	ln -sf $(CURDIR)/Release/KoboRoot.tgz Kobo/mnt/onboard/.kobo/KoboRoot.tgz
	ln -sf $(CURDIR)/config/kfmon.ini Kobo/mnt/onboard/.adds/kfmon/config/kfmon.ini
	ln -sf $(CURDIR)/config/koreader.ini Kobo/mnt/onboard/.adds/kfmon/config/koreader.ini
	ln -sf $(CURDIR)/config/plato.ini Kobo/mnt/onboard/.adds/kfmon/config/plato.ini
	ln -sf $(CURDIR)/config/kfmon-log.ini Kobo/mnt/onboard/.adds/kfmon/config/kfmon-log.ini
	ln -sf $(CURDIR)/scripts/kfmon-printlog.sh Kobo/mnt/onboard/.adds/kfmon/bin/kfmon-printlog.sh
	pushd Kobo/mnt/onboard && zip -r ../../KFMon-$(KFMON_VERSION).zip . && popd

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
	$(MAKE) strip KOBO=true MINIMAL=true IMAGE=1 OPENTYPE=1
	touch fbink.built

release: libevdev.built fbink.built
	$(MAKE) strip

debug: libevdev.built
	cd FBInk && \
	$(MAKE) debug KOBO=true MINIMAL=true IMAGE=1 OPENTYPE=1
	touch fbink.built
	$(MAKE) all DEBUG=true

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

.PHONY: default outdir all vendored usbms strip armcheck kobo debug clean release fbinkclean libevdevclean distclean
