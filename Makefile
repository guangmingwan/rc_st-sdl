# st - simple terminal
# See LICENSE file for copyright and license details.
TARGET?=rs97
include config_$(TARGET).mk

SRC = st.c keyboard.c font.c msg_queue.c ime.c
OBJ = ${SRC:.c=.o}

ifeq ($(origin CXX),default)
CXX = $(if $(filter cc,${CC}),c++,${CC:gcc=g++})
endif
IME_SRC_DIR = .build/vendor/libgooglepinyin-0.1.2
IME_NAMES = dictbuilder dictlist dicttrie lpicache matrixsearch mystdlib ngram pinyinime searchutility spellingtable spellingtrie splparser sync userdict utf16char utf16reader
IME_OBJECTS = $(addprefix .build/${TARGET}/pinyin/,$(addsuffix .o,${IME_NAMES}))
IME_WRAPPER = .build/${TARGET}/pinyin_engine.o
IME_LIBRARY = .build/${TARGET}/libpinyin.a
IME_INCS = -I${IME_SRC_DIR}/include

all: options st libst-preload.so sdl_test

libst-preload.so: st-preload.o msg_queue.o
	${CC} -shared -o $@ $^

sdl_test: sdl_test.o
	${CC} -o $@ $< ${LDFLAGS}

options:
	@echo st build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

config.h:
	cp config.def.h config.h

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk
st.o keyboard.o font.o: font.h
st.o keyboard.o ime.o: ime.h
ime.o: pinyin_engine.h
font.o: console_font.h

${IME_SRC_DIR}/.prepared: tools/prepare_ime.py
	python3 tools/prepare_ime.py

ime-data/dict_pinyin.dat: ${IME_SRC_DIR}/.prepared
	python3 tools/prepare_ime.py

.build/${TARGET}/pinyin/%.o: ${IME_SRC_DIR}/.prepared
	mkdir -p .build/${TARGET}/pinyin
	${CXX} -O2 -fPIC -std=gnu++11 ${IME_INCS} -c ${IME_SRC_DIR}/src/$*.cpp -o $@

${IME_WRAPPER}: pinyin_engine.cpp pinyin_engine.h ${IME_SRC_DIR}/.prepared
	mkdir -p .build/${TARGET}
	${CXX} -O2 -fPIC -std=gnu++11 ${IME_INCS} -c pinyin_engine.cpp -o $@

${IME_LIBRARY}: ${IME_OBJECTS} ${IME_WRAPPER}
	$(if $(filter cc gcc,${CC}),ar,${CC:gcc=ar}) rcs $@ ${IME_OBJECTS} ${IME_WRAPPER}

.build/pc/dictbuilder: ${IME_SRC_DIR}/.prepared
	mkdir -p .build/pc
	${HOSTCXX} -O2 -std=gnu++11 ${IME_INCS} -o $@ ${IME_SRC_DIR}/tools/pinyinime_dictbuilder.cpp $(addprefix ${IME_SRC_DIR}/src/,$(addsuffix .cpp,${IME_NAMES})) -lpthread

.build/pc/dict_pinyin.dat: .build/pc/dictbuilder
	.build/pc/dictbuilder ${IME_SRC_DIR}/data/rawdict_utf16_65105_freq.txt ${IME_SRC_DIR}/data/valid_utf16.txt $@

ifeq (${TARGET},pc)
st: .build/pc/dict_pinyin.dat
endif

font:
	python3 tools/prepare_font.py

package-d8: font ime-data/dict_pinyin.dat
	sh build_d8.sh
	mkdir -p .build/d88/fonts .build/d88/ime-data
	cp st libst-preload.so .build/d88/
	cp fonts/unifont.hex fonts/OFL-1.1.txt fonts/COPYING fonts/README.upstream fonts/FONT-NOTICE.txt fonts/README .build/d88/fonts/
	cp README.zh-CN.md .build/d88/
	cp ime-data/dict_pinyin.dat ime-data/LICENSE-2.0.txt ime-data/NOTICE.txt ime-data/README .build/d88/ime-data/

IPK_VERSION ?= 0.4.0-1
ipk-d8: package-d8
	python3 tools/build_ipk.py --version "${IPK_VERSION}"

HOSTCC ?= cc
HOSTCXX ?= c++
test: config.h ${IME_SRC_DIR}/.prepared .build/pc/dict_pinyin.dat
	mkdir -p .build
	${MAKE} TARGET=pc CC=${HOSTCC} CXX=${HOSTCXX} .build/pc/libpinyin.a
	${HOSTCC} -g -Wall -DPC -DVERSION=\"${VERSION}\" -I. $$(sdl-config --cflags) ${TEST_CFLAGS} -o .build/test-chinese tests/chinese.c keyboard.c font.c msg_queue.c ime.c .build/pc/libpinyin.a $$(sdl-config --libs) -lutil -lstdc++ -lpthread -lm ${TEST_CFLAGS}
	.build/test-chinese
	${HOSTCC} -g -Wall -DPC -I. $$(sdl-config --cflags) ${TEST_CFLAGS} -o .build/test-ime tests/ime.c keyboard.c font.c ime.c .build/pc/libpinyin.a $$(sdl-config --libs) -lstdc++ -lpthread -lm ${TEST_CFLAGS}
	.build/test-ime .build/pc/dict_pinyin.dat fonts/unifont.hex
	${HOSTCC} -g -Wall -DPC -DVERSION=\"${VERSION}\" -I. $$(sdl-config --cflags) ${TEST_CFLAGS} -o .build/test-ime-pty tests/ime_pty.c keyboard.c font.c msg_queue.c ime.c .build/pc/libpinyin.a $$(sdl-config --libs) -lutil -lstdc++ -lpthread -lm ${TEST_CFLAGS}
	.build/test-ime-pty .build/pc/dict_pinyin.dat

test-target: config.h keyboard.o font.o ime.o msg_queue.o ${IME_LIBRARY}
	mkdir -p .build/${TARGET}
	${CC} ${CFLAGS} -c tests/ime.c -o .build/${TARGET}/test-ime.o
	${CXX} -o .build/${TARGET}/test-ime .build/${TARGET}/test-ime.o keyboard.o font.o ime.o ${IME_LIBRARY} ${LDFLAGS} -lpthread -lm -static-libstdc++ -static-libgcc
	${CC} ${CFLAGS} -c tests/ime_pty.c -o .build/${TARGET}/test-ime-pty.o
	${CXX} -o .build/${TARGET}/test-ime-pty .build/${TARGET}/test-ime-pty.o keyboard.o font.o ime.o msg_queue.o ${IME_LIBRARY} ${LDFLAGS} -lpthread -lm -static-libstdc++ -static-libgcc

st: ${OBJ} ${IME_LIBRARY} ime-data/dict_pinyin.dat
	@echo CC -o $@
	@${CXX} -o $@ ${OBJ} ${IME_LIBRARY} ${LDFLAGS} -lpthread -lm -static-libstdc++ -static-libgcc

clean:
	@echo cleaning
	@rm -f st ${OBJ} st-preload.o sdl_test.o libst-preload.so sdl_test st-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p st-${VERSION}
	@cp -R LICENSE Makefile README config.mk config.def.h st.info st.1 ${SRC} st-${VERSION}
	@tar -cf st-${VERSION}.tar st-${VERSION}
	@gzip st-${VERSION}.tar
	@rm -rf st-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f st ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/st
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < st.1 > ${DESTDIR}${MANPREFIX}/man1/st.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/st.1
	@echo If things do not seem to work, be sure that there is no \
		floating st terminfo in the .terminfo directory in your home dir.
	@tic -s st.info

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/st
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/st.1

.PHONY: all options clean dist install uninstall font test test-target package-d8 ipk-d8
