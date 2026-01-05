VER_MAJOR=3
VER_MINOR=2
VER_RELEASE=5
VERSION=$(VER_MAJOR).$(VER_MINOR).$(VER_RELEASE)

BUILD_DIR=.build
BUILD_DIR_RELEASE=$(BUILD_DIR)_release
BUILD_DIR_DEBUG=$(BUILD_DIR)_debug

BUNDLE_NAME=sviewgl
OUT_NAME=sviewgl
COMPILE_COMMANDS_DIR=.compile_commands

PREFIX?=/usr/local

UNAME=$(shell uname -s)
ifeq ($(UNAME), Darwin)
	BUNDLE_NAME=sviewgl.app
	OUT_NAME="Simple Viewer GL.app"
endif

all:    release

help:
	@echo "Usage:"
	@echo "    make <release | debug>    - make release or debug application"
	@echo "    make install              - install application"
	@echo "    make <cppcheck>           - do static code verification"
	@echo "    make <dist>               - make tar.gz source package"
	@echo "    make <deb>                - make DEB package"
	@echo "    make <rpm>                - make RPM package"
	@echo "    make <clean>              - cleanup directory"

release:
	$(shell if [ ! -d $(BUILD_DIR_RELEASE) ]; then mkdir $(BUILD_DIR_RELEASE); fi)
	cd $(BUILD_DIR_RELEASE) && \
		cmake .. \
		-Wno-dev \
		-DCMAKE_BUILD_TYPE=Release \
		-DAPP_VERSION_MAJOR:STRING=$(VER_MAJOR) \
		-DAPP_VERSION_MINOR:STRING=$(VER_MINOR) \
		-DAPP_VERSION_RELEASE:STRING=$(VER_RELEASE) && \
		make -j
	rm -fr $(OUT_NAME) && cp -r $(BUILD_DIR_RELEASE)/$(BUNDLE_NAME) $(OUT_NAME)

debug:
	$(shell if [ ! -d $(BUILD_DIR_DEBUG) ]; then mkdir $(BUILD_DIR_DEBUG); fi)
	cd $(BUILD_DIR_DEBUG) && \
		cmake .. \
		-DCMAKE_BUILD_TYPE=Debug \
		-DAPP_VERSION_MAJOR:STRING=$(VER_MAJOR) \
		-DAPP_VERSION_MINOR:STRING=$(VER_MINOR) \
		-DAPP_VERSION_RELEASE:STRING=$(VER_RELEASE) && \
		make -j
	rm -fr $(OUT_NAME) && cp -r $(BUILD_DIR_DEBUG)/$(BUNDLE_NAME) $(OUT_NAME)

build_compile_commands:
	$(shell if [ ! -d $(COMPILE_COMMANDS_DIR) ]; then mkdir $(COMPILE_COMMANDS_DIR); fi )
	cd $(COMPILE_COMMANDS_DIR) && \
		cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
		-DAPP_VERSION_MAJOR:STRING=$(VER_MAJOR) \
		-DAPP_VERSION_MINOR:STRING=$(VER_MINOR) \
		-DAPP_VERSION_RELEASE:STRING=$(VER_RELEASE)
	rm -f compile_commands.json ; \
		ln -s $(COMPILE_COMMANDS_DIR)/compile_commands.json compile_commands.json

cppcheck:
	cppcheck \
		-DEXIF_SUPPORT=1 \
		-DLCMS2_SUPPORT=1 \
		-DJPEG2000_SUPPORT=1 \
		-DGIF_SUPPORT=1 \
		-DTIFF_SUPPORT=1 \
		-DWEBP_SUPPORT=1 \
		-DOPENEXR_SUPPORT=1 \
		-DIMLIB2_SUPPORT=1 \
		-j 1 \
		--std=c++14 \
		--enable=all \
		-f -I src src/ 2> cppcheck-output

clean:
	rm -fr $(BUILD_DIR)* $(OUT_NAME) $(BUNDLE_NAME) \
		$(COMPILE_COMMANDS_DIR) compile_commands.json \
		$(BUNDLE_NAME)-$(VERSION)* $(BUNDLE_NAME)_$(VERSION)* \
		*.log *.tasks *.sh *.xz *.list *.deb \
		cppcheck-output strace_out cov-int

install:
	install -m 755 -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BUNDLE_NAME) $(DESTDIR)$(PREFIX)/bin

dist:   clean
	install -d $(BUNDLE_NAME)-$(VERSION)
	cp -R cmake src res third-party dist/debian INSTALL README.md CMakeLists.txt Makefile sviewgl.desktop res/Icon-1024.png dist/fedora/* dist/gentoo/* $(BUNDLE_NAME)-$(VERSION)
	mv $(BUNDLE_NAME)-$(VERSION)/simpleviewer-gl-_VERSION_.ebuild $(BUNDLE_NAME)-$(VERSION)/simpleviewer-gl-$(VERSION).ebuild
	sed "s/_VERSION_/$(VERSION)/" -i $(BUNDLE_NAME)-$(VERSION)/$(BUNDLE_NAME).spec
	sed "s/_VERSION_/$(VERSION)/" -i $(BUNDLE_NAME)-$(VERSION)/debian/changelog
	tar -zf $(BUNDLE_NAME)-$(VERSION).tar.gz -c $(BUNDLE_NAME)-$(VERSION)

deb:    dist
	mv $(BUNDLE_NAME)-$(VERSION).tar.gz $(BUNDLE_NAME)_$(VERSION).orig.tar.gz
	cd $(BUNDLE_NAME)-$(VERSION) ; PREFIX=/usr dpkg-buildpackage -F -tc

rpm:    dist
	rpmbuild -ta $(BUNDLE_NAME)-$(VERSION).tar.gz
