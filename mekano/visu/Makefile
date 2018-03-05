# Stuart Pook, Genoscope 2007 (enscript -E $%) (Indent ON)
# $Id: Makefile 802 2007-06-06 22:17:08Z stuart $ make
.PHONY: run clean install commited all
all:
target = mekano
fltk := /env/cns/opt/fltk/${GSC_ARCH}/fltk-2.0.x-r5777
fltk_local = ${fltk}
eyedb = ${eyedb_root}eyedb-2.7.13
eyedb_local = ${eyedb}
nurbs = /env/cns/opt/nurbs++/nurbs++-3.0.11
db = hg
include ../Makefile.inc
include ${eyedb}/share/eyedb/Makefile.eyedb
utils = ..
rpath = -L${1} -Wl,-R${1}
EYEDB_INCLUDEDIR = ${eyedb}/include
CPPFLAGS +=-isystem${fltk}/include -I${utils} -I.
CPPFLAGS += ${subst -I,-isystem,${EYEDB_CPPFLAGS}}
#CPPFLAGS += -I/env/export/nfs1/home/stuart/install/boost_1_34_0
LDFLAGS += -L${fltk}/lib -Wl,-rpath ${fltk_local}/lib ${LDXXFLAGS} -Wl,-R${eyedb_local}/lib
LDFLAGS += ${EYEDB_LDFLAGS}
LDFLAGS += ${call rpath, ${nurbs}/lib}
LDLIBS += ${LDXXLIBS} -lfltk2 -lfltk2_gl -lGL -lglut
LDLIBS += ${EYEDB_LDLIBS}
LDLIBS += -lnurbsf -lmatrixN -lmatrixI -lmatrix
march := ${shell test ! -r /proc/cpuinfo || sed -ne 's/.*Intel(R) Pentium(R) 4 CPU.*/-march=pentium4/p' -e '/AMD Opteron/{s/.*/-march=opteron -pipe/p;q;}' /proc/cpuinfo} 
CXXFLAGS += -g -Wall -Wextra -Wno-deprecated ${march}
Werror = -Werror
CXXFLAGS += ${EYEDB_CXXFLAGS} ${Werror}
CXX=g++
CC=gcc

norbs_objs = ${addsuffix .o, graphics}
objs = ${addsuffix .o, visu ${project} value_input matches sctg_data eyedb utilities selector graph bcm circle} ${norbs_objs}

all: ${target}

${norbs_objs} : CPPFLAGS += -isystem${nurbs}/include
# --user=stuart --host=localhost

cloaca = -d cloaca\#0 sctg1_2
RX = -d RX.stuart SC2 SC1
hg = -d oikopleura sctg2_3 sctg2_26 sctg2_35 sctg1_34439 sctg2_44 sctg1_1
vigne = -d vigne sctg1_1145 sctg1_162 sctg1_625 sctg1_884 sctg1_1034 sctg1_1253 sctg1_303 sctg1_576 sctg1_820 sctg1_925 # sctg1_0
debug = -d debug sctg2_3
pichia = -d pichia.stuart

run: ${target}
	cd tmp && ulimit -c hard && time ${PWD}/${target} ${debug}

hg: ${target}
	./${target} ${hg}

${objs}: ${project}.h ${eyedb}/include/eyedb/eyedb.h ${fltk}/include/fltk/Widget.h
${norbs_objs}: ${nurbs}/include/nurbs++/nurbs.h

install: ${install_dir}/${target}

ifeq (install,$(filter install,${MAKECMDGOALS}))
CXXFLAGS += -O2
endif

${target} : ${objs}
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@.tmp
	@mv $@.tmp $@
	@rm -f tmp/core.[1-9]*

#${project}.cc ${project}.h : ${schema} ${EYEDBODL} ${edit_api}
#	${EYEDBODL} ${EYEDB_ODL} --attr-style=implicit --namespace=mekano --gencode=C++ --packa#ge=${project} ${schema}
#	@rm ${project}stubsfe.cc ${project}stubsbe.cc ${project}mthfe-skel.cc ${project}mthbe-skel.cc Makefile.${project}  template_${project}.cc
#	${edit_api} ${project}.h > ${project}.h_
#	mv ${project}.h_ ${project}.h

${project}.o : CXXFLAGS += -O2 -w

.DELETE_ON_ERROR:

includes=${objs:.o=.d}

clean:
	${RM} ${objs} ${target} ${project}.cc ${project}.h ${includes}

# svn propset svn:keywords Id $%

%.o : %.cc
	$(CXX) -MMD -c $(CPPFLAGS) $(CXXFLAGS) $<
%.o : ../utils/%.cc
	$(CXX) -I. -MMD -c $(CPPFLAGS) $(CXXFLAGS) $<
ifneq (clean,$(filter clean,${MAKECMDGOALS}))
-include ${includes}
endif
