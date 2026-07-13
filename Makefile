# PSForcer OpenOrbis paket bilgileri - v0.29
TITLE       := PSForcer
VERSION     := 0.29
TITLE_ID    := PSFC00001
CONTENT_ID  := IV0000-PSFC00001_00-PSFORCERCLIENT00

# BGFT istemci oturumu için Remote Package Installer ile aynı SELF yetki
# yetenekleri. create-fself ilk 8 baytı atıp PAID değerini ayrıca yazar.
AUTH_INFO   := 000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000

LIBS        := -lc -lkernel -lc++ -lSceUserService -lSceVideoOut -lSceAudioOut -lScePad -lSceSysmodule -lSDL2 -lSceNet -lSceSsl -lSceHttp -lSceAppInstUtil -lSceBgft
EXTRAFLAGS  := -std=gnu++11 -D_GNU_SOURCE -DPSFORCER_ORBIS=1

TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
PROJDIR     := src
INTDIR      := x64/Debug
ROOT_ASSETS := assets/catalog.json assets/hf_token.txt assets/katalog_adresi.txt
PACKAGE_MEDIA := assets/pt-cover.jpg assets/pt-header.jpg assets/pt-screenshot-1.jpg assets/pt-screenshot-2.jpg assets/pt-screenshot-3.jpg
ASSETS      := $(ROOT_ASSETS) $(PACKAGE_MEDIA)
LIBMODULES  := $(wildcard sce_module/*)
CPPFILES    := $(shell find $(PROJDIR) -name '*.cpp')
OBJS        := $(patsubst $(PROJDIR)/%.cpp,$(INTDIR)/%.o,$(CPPFILES))

CXXFLAGS    := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c $(EXTRAFLAGS) -I$(PROJDIR) -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CCX  := clang++
    LD   := ld.lld
    CDIR := linux
endif
ifeq ($(UNAME_S),Darwin)
    CCX  := /usr/local/opt/llvm/bin/clang++
    LD   := /usr/local/opt/llvm/bin/ld.lld
    CDIR := macos
endif

.PHONY: all clean bootstrap check-env assets
all: check-env assets $(CONTENT_ID).pkg

check-env:
	@test -n "$(OO_PS4_TOOLCHAIN)" || (echo "OO_PS4_TOOLCHAIN ayarlanmadı" && exit 1)
	@test -f sce_sys/about/right.sprx || (echo "Önce 'make bootstrap' çalıştırın" && exit 1)
	@test -n "$(LIBMODULES)" || (echo "Önce 'make bootstrap' çalıştırın" && exit 1)

bootstrap: check-env-only
	@mkdir -p sce_module sce_sys/about
	@cp -f "$(TOOLCHAIN)/samples/SDL2/sce_module/"* sce_module/
	@cp -f "$(TOOLCHAIN)/samples/SDL2/sce_sys/about/right.sprx" sce_sys/about/right.sprx
	@echo "OpenOrbis çalışma dosyaları kopyalandı."

check-env-only:
	@test -n "$(OO_PS4_TOOLCHAIN)" || (echo "OO_PS4_TOOLCHAIN ayarlanmadı" && exit 1)

$(CONTENT_ID).pkg: pkg.gp4
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build $< .

assets: sce_sys/icon0.png $(PACKAGE_MEDIA)

sce_sys/icon0.png: tools/generate_assets.py
	python3 tools/generate_assets.py

assets/pt-cover.jpg: assets/media/pt/cover.jpg
	cp -f $< $@

assets/pt-header.jpg: assets/media/pt/header.jpg
	cp -f $< $@

assets/pt-screenshot-1.jpg: assets/media/pt/screenshot-1.jpg
	cp -f $< $@

assets/pt-screenshot-2.jpg: assets/media/pt/screenshot-2.jpg
	cp -f $< $@

assets/pt-screenshot-3.jpg: assets/media/pt/screenshot-3.jpg
	cp -f $< $@

pkg.gp4: eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png $(LIBMODULES) $(ASSETS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

sce_sys/param.sfo: Makefile
	@mkdir -p sce_sys
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_new $@
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

eboot.bin: $(OBJS)
	$(LD) $(OBJS) -o $(INTDIR)/PSForcer.elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$(INTDIR)/PSForcer.elf -out=$(INTDIR)/PSForcer.oelf --eboot "eboot.bin" --paid 0x3800000000000011 --authinfo $(AUTH_INFO)

$(INTDIR)/%.o: $(PROJDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CCX) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf x64 eboot.bin pkg.gp4 sce_sys/param.sfo $(CONTENT_ID).pkg $(PACKAGE_MEDIA) .v023-source-ready .v025-source-ready
