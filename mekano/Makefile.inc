# $Id: Makefile.inc 755 2007-05-24 16:20:01Z stuart $
# Stuart Pook, Genescope, 2007

project = mekano
schema = ../schema.odl
edit_api = ../scripts/edit_api
#install_dir = /env/cns/ARCH/${GSC_ARCH}/stow/mekano/bin
install_dir = /usr/local/stow/mekano/bin
install_mode = 2755
install_group = g_eyedb
install: ${install_dir}/${notdir ${target}}
${install_dir}/%: commited ${target}
	cp ${target} $@.new
	chgrp ${install_group} $@.new
	chmod ${install_mode} $@.new
	@python -c 'import socket;socket.socket(socket.AF_INET,socket.SOCK_STREAM).bind(("enta",0))' 2> /dev/null || { set -x; scp -Bq ${target} etna:$@ && ssh -n etna chgrp ${install_group} $@ '&&' chmod ${install_mode} $@ ; }
	mv --reply=yes $@.new $@

commited:
	if svn status ../utils . ../schema.odl | grep -v '^?'; then false; else true; fi
	@set -x; svn update .. && svn -m "install `date`" copy -r `svn info .. | sed -ne '/Revision: /{s///;p;x;p;G;s@/trunk\n@/tags/installed-@;p}' -e '/URL: /{s///;x}'`

eyedbctl := ${shell which eyedbctl}
eyedb_root := ${dir ${eyedbctl}}${dir ${patsubst %/, %, ${dir ${patsubst %/, %, ${dir ${shell readlink ${eyedbctl}}}}}}}

# svn propset svn:keywords Id $%

${o}${project}.cc : ${schema} ${EYEDB_ODL} ${edit_api}
	${EYEDB_ODL} --output-dir=${dir $@} --attr-style=implicit --namespace=${project} --gencode=C++ --package=${project} ${schema}
	rm ${dir $@}${project}stubsfe.cc ${dir $@}${project}stubsbe.cc ${dir $@}${project}mthfe-skel.cc ${dir $@}${project}mthbe-skel.cc ${dir $@}Makefile.${project} ${dir $@}template_${project}.cc
	${edit_api} ${dir $@}${project}.h > ${dir $@}${project}.h_
	mv ${dir $@}${project}.h_ ${dir $@}${project}.h

${o}${project}.h : ${o}${project}.cc
	touch $@
