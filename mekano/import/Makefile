# $Id: Makefile 797 2007-06-06 13:36:20Z stuart $ make
# Stuart Pook, Genescope, 2007
.PHONY : clean all fast full vigne rx hg cloaca pichia databases tz
tmp=tmp
o=${tmp}/
target = ${o}mekano_load
program_names =  sctg_check run_clonelinks
programs = ${addprefix ${o}, ${program_names}} ${target} 
#testit:
all : ${programs}
include ../Makefile.inc

db = mekano
schema = ../schema.odl
eyedb_version = eyedb-2.7.13
eyedb = ${eyedb_root}${eyedb_version}
db_owner = eyedb
test_db = test
u := '--user=${USER}' '--host=localhost'

include ${eyedb}/share/eyedb/Makefile.eyedb

march := ${shell test ! -r /proc/cpuinfo || sed -ne 's/.*Intel(R) Pentium(R) 4 CPU.*/-march=pentium4/p' -e '/AMD Opteron/{s/.*/-pipe -march=opteron -O1/p;q;}' /proc/cpuinfo} 
CXXFLAGS += -g -Wall ${march} $O
CXXFLAGS += ${EYEDB_CXXFLAGS}
utils = ..
CPPFLAGS += ${EYEDB_CPPFLAGS} -I${utils} -I$o
LDFLAGS += ${EYEDB_LDFLAGS} -Wl,-R${EYEDB_LIBDIR}
LDLIBS += ${EYEDB_LDLIBS}
CC=${CXX}
b=./${tmp}
core_directory = /var/tmp/stuart

${programs} : ${addprefix $o,${project}.o common.o sctg_data.o}
${o}debug : ${addprefix $o,${project}.o}
objs = ${addprefix $o, ${addsuffix .o, clonelinks joins clones_reads banks contigs stage_types metacontigs etape change_group group newstage mekanob utilities sctg_data eyedb matches ${project} common}}

chondrus = /env/cns/proj/projet_UA/Assemblages/arachne/Chondrus/1X/Mekano/chondrus_mek.init
rx = /env/cns/proj/projet_RX/A/Assemblages/RXA_C/MEKANO/RX.mek.init
vigne=/env/cns/proj/projet_ML/Assemblage/arachne/Vigne8X/run1/MEKANO/ML.mek.init
vigne_db = vigne.${USER}
testit_env = ulimit -c 4194304; time
testit: ${o}run_clonelinks
	${o}run_clonelinks vigne 0 0

databases:  tz pichia chondrus cloaca rx vigne hg

${o}schema.h: ${schema}
	cpp -P ${schema} | sed -e 's/^[ 	]*//' -e '/^$$/d' -e 's/.*/"&\\n"/'  > $@.new
	@mv $@.new $@
${o}mekanob.o : ${o}schema.h

hg: ${target}
	-eyedbdbdelete hg.stuart
	ulimit -c 5242880 && \
	${target} --database=hg.stuart /env/cns/proj/projet_HG/Assemblages/arachne/Oikopleura1400/run1/MEKANO/NEW/HG.mek.init

chondrus: ${target}
	-eyedbdbdelete chondrus.stuart
	ulimit -c 5242880 && ${target} --database=chondrus.stuart ${chondrus}

rx: ${target}
	-eyedbdbdelete RX.stuart
	ulimit -c 5242880 && ${target} --database=RX.stuart ${rx}

vigne: ${target}
	-eyedbdbdelete ${vigne_db}
	ulimit -c 5242880 && ${target} --database=${vigne_db} ${vigne}

cloaca: ${target}
	-eyedbdbdelete $@.stuart
	ulimit -c 5242880 && ${target} --database=$@.stuart /env/cns/proj/BioComputing/Cloaca/Arachne/CloacaMaxima/Francois/Cloaca_1/Resultats3/mekano/cloaca_mek.init

pichia: ${target}
	-eyedbdbdelete $@.stuart
	ulimit -c 5242880 && ${target} --database=$@.stuart /env/cns/proj/projet_NG/Assemblages/arachne/GAELLE/PICHIA/run2/MEKANO/pichia_mek.init 

test: ${target}
	-eyedbdbdelete $@.stuart
	ulimit -c 5242880 && ${target} --database=$@.stuart pichia/pichia_mek.init

tz: ${target}
	-eyedbdbdelete $@.stuart
	ulimit -c 5242880 && ${target} --database=$@.stuart /env/cns/proj/projet_TZ/A/Assemblages/Arachne/Clostridium_VectMask/run1/MEKANO/Clostridium_VectMask_mek.init

check_args=  -C $o hg

check: ${o}sctg_check
#	$b/sctg_check ${check_args} ${local_data}/scaffolding.sctg.ctg.verif.mek ${local_data}/A.fusions1.sctg.ctg.verif.mek ${local_data}/fusions2.sctg.ctg.verif.mek
	$b/sctg_check -a ${check_args} ${local_data}/cuts1.news.mek
	:
	$b/sctg_check ${check_args} ${local_data}/fusions2.sctg.ctg.verif.mek
	$b/sctg_check -a ${check_args} ${local_data}/fusions2.sctg.cseq.mek > tmp/log
	:
	$b/sctg_check ${check_args} ${local_data}/A.fusions1.sctg.ctg.verif.mek > tmp/log
	$b/sctg_check -A ${check_args} ${local_data}/A.fusions1.sctg.ctg.verif.mek > tmp/log
	$b/sctg_check -a ${check_args} ${local_data}/A.fusions1.sctg.cseq.mek > tmp/log
	$b/sctg_check -Aa ${check_args} ${local_data}/A.fusions1.sctg.cseq.mek > tmp/log
	:
	$b/sctg_check ${check_args} ${local_data}/scaffolding.sctg.ctg.verif.mek > tmp/log
	$b/sctg_check -a ${check_args} ${local_data}/scaffolding.sctg.cseq.mek > tmp/log

${objs} : ${o}${project}.h

${o}stamp:
	mkdir -p ${o}
	touch $@

$o${project}.o : CXXFLAGS += -w
${o}clonelinks.o : CXXFLAGS += -O3

${o}run_clonelinks: ${o}run_clonelinks.o ${o}clonelinks.o ${o}utilities.o  ${o}eyedb.o 
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

.DELETE_ON_ERROR:

includes=${objs:.o=.d}

clean:
	${RM} ${objs} ${programs} ${o}${project}.h ${o}${project}.cc ${includes}

# svn propset svn:keywords Id $%
$o${project}.o : $o${project}.cc ${o}stamp
	$(CXX) -MMD -MF ${@:.o=.d} -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@
$o%.o : %.cc ${o}stamp
	$(CXX) -MMD -MF ${@:.o=.d} -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@
$o%.o : ../utils/%.cc ${o}stamp
	$(CXX) -I. -MMD -MF ${@:.o=.d} -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

${o}%: ${o}%.o
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@
	@rm -f ${core_directory}/core.*

${target}: ${objs}
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@
	@rm -f ${core_directory}/core.*

ifneq (clean,$(filter clean,${MAKECMDGOALS}))
-include ${includes}
endif
