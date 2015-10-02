#!/bin/sh

# some terminal codes ...
boldface="$(tput bold 2>/dev/null)"
normal="$(tput sgr0 2>/dev/null)"
printbold()(
	IFS=' '; printf "$boldface%s$normal" "$*"
)

printbold Running libtoolize...
libtoolize --force --copy
printbold Running glib-gettextize...
glib-gettextize --force --copy
printbold Running intltoolize...
intltoolize --force --copy --automake
printbold Running aclocal...
aclocal -I m4
printbold Running autoconf...
autoconf -Wall
printbold Running autoheader...
autoheader -Wall

if [ -f COPYING ]; then
	cp -pf COPYING COPYING.autogen_bak
fi
if [ -f INSTALL ]; then
	cp -pf INSTALL INSTALL.autogen_bak
fi

printbold Running automake...
automake -Wall --gnu --add-missing --force --copy

if [ -f COPYING.autogen_bak ]; then
	cmp COPYING COPYING.autogen_bak > /dev/null || cp -pf COPYING.autogen_bak COPYING
	rm -f COPYING.autogen_bak
fi
if [ -f INSTALL.autogen_bak ]; then
	cmp INSTALL INSTALL.autogen_bak > /dev/null || cp -pf INSTALL.autogen_bak INSTALL
	rm -f INSTALL.autogen_bak
fi

conf_flags="--enable-maintainer-mode"

[ "$((NOCONFIGURE))" -ne 0 ] || exit 0
printbold Running ./configure $conf_flags "$@" ...
./configure $conf_flags "$@" \
&& echo Now type \`make\' to compile elsa-shell
