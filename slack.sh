#!/bin/sh
# pre cleanup
rm -rf build

# config vars
ARCH=i486
ETC=etc/X11/imwheel

# configure
CFLAGS="-march=$ARCH -Os" ./configure --prefix=/usr
# safe build
make clean
make
# install to build dir
make prefix="build/usr" ETCDIR="build/$ETC" install

# get name and version
eval `egrep "^VERSION =" Makefile | sed -e 's/ //g'`
eval `egrep "^PACKAGE =" Makefile | sed -e 's/ //g'`
PKG=$PACKAGE-$VERSION-$ARCH-${1-1}

cd build || exit 1

# fix perms
chown -R root .
chgrp -R bin .
# alter config file names
mv $ETC/imwheelrc $ETC/imwheelrc.new
# strip execs
echo stripping...
find -perm 755 -print \
| while read f
do
	file "$f" | grep -q "not stripped" && {
		echo "$f"
		strip "$f"
	}
done
# gzip manpages
echo gzipping...
find -name '*.[123456789]' -exec gzip -v {} \;

# make description
mkdir install
cat >install/slack-desc << EOF
$PACKAGE: IMWheel-$VERSION (mouse wheel handler for X11)
$PACKAGE:
$PACKAGE: imwheel is an X11 application that allows a user
$PACKAGE: to alter the actions resulting from a mouse wheel
$PACKAGE: or thumb button press.  It is able to have individual
$PACKAGE: configurations for each application by name.
$PACKAGE: imwheel is particularly useful for old applications
$PACKAGE: that don't know about the new mouse wheel and buttons.
$PACKAGE:
$PACKAGE: You can find the latest release of imwheel at:
$PACKAGE: http://jonatkins.org/iwheel/
$PACKAGE: 
$PACKAGE: 
EOF

# make post install script
cat >install/doinst.sh <<EOF
#!/bin/sh
#
# postinstall script, created by hand
#
if [ ! -f /$ETC/imwheelrc ]
then
	mv /$ETC/imwheelrc.new /$ETC/imwheelrc
	echo new default config created at /$ETC/imwheelrc
else
	echo new default config created at /$ETC/imwheelrc.new
	echo merge with /$ETC/imwheelrc yourself...
fi
EOF

# make package
makepkg -c n -l y "../$PKG.tgz"

# cleanup
cd ..
rm -rf build
