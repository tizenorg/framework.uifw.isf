#!/bin/sh

PACKAGE=keyboard-setting-wizard-efl
SRCROOT=../..
POTFILES=POTFILES.in

ALL_LINGUAS="de_DE el_GR en es_ES fr_FR it_IT ja_JP ko_KR nl_NL pt_PT ru_RU tr_TR zh_CN zh_HK zh_TW"

XGETTEXT=/usr/bin/xgettext
MSGMERGE=/usr/bin/msgmerge

echo -n "Make ${PACKAGE}.pot  "
if [ ! -e $POTFILES ] ; then
	echo "$POTFILES not found"
	exit 1
fi

$XGETTEXT --default-domain=${PACKAGE} --directory=${SRCROOT} \
		--add-comments --keyword=_ --keyword=T_ --files-from=$POTFILES \
&& test ! -f ${PACKAGE}.po \
	|| (rm -f ${PACKAGE}.pot && mv ${PACKAGE}.po ${PACKAGE}.pot)

if [ $? -ne 0 ]; then
	echo "error"
	exit 1
else
	echo "done"
fi

for LANG in $ALL_LINGUAS; do 
	echo "$LANG : "

	if [ ! -e $LANG.po ] ; then
		cp ${PACKAGE}.pot ${LANG}.po
		echo "${LANG}.po created"
	else
		if $MSGMERGE ${LANG}.po ${PACKAGE}.pot -o ${LANG}.new.po ; then
			if cmp ${LANG}.po ${LANG}.new.po > /dev/null 2>&1; then
				rm -f ${LANG}.new.po
			else
				if mv -f ${LANG}.new.po ${LANG}.po; then
					echo ""	
				else
					echo "msgmerge for $LANG.po failed: cannot move $LANG.new.po to $LANG.po" 1>&2
					rm -f ${LANG}.new.po
					exit 1
				fi
			fi
		else
			echo "msgmerge for $LANG failed!"
			rm -f ${LANG}.new.po
		fi
	fi
	echo ""
done

