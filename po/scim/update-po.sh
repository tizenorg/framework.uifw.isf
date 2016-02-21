#!/bin/sh

PACKAGE=scim
SRCROOT=../..
POTFILES=POTFILES.in

ALL_LINGUAS="ar as az be_BY bg bn bn_BD ca cs da de el_GR en en_PH en_US es_ES es_MX es_US et eu fa fi fr fr_CA ga gl gu he hi hr hu hy id is it_IT ja_JP ka kk km kn ko_KR ky_KG lo lt lv mg mk ml mn_MN mr ms my_MM my_ZG nb ne nl or pa pl pl_SP pt_BR pt_PT ro ru_RU si sk sl sq sr sv sw ta te tg_TJ th tk_TM tl tr_TR uk ur uz vi xh zh_CN zh_HK zh_TW zh_TW zu"

XGETTEXT=/usr/bin/xgettext
MSGMERGE=/usr/bin/msgmerge

echo -n "Make ${PACKAGE}.pot  "
if [ ! -e $POTFILES ] ; then
	echo "$POTFILES not found"
	exit 1
fi

$XGETTEXT --default-domain=${PACKAGE} --directory=${SRCROOT} \
		--add-comments --keyword=_ --keyword=N_ --files-from=$POTFILES \
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

