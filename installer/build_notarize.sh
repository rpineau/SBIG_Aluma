#!/bin/bash

PACKAGE_NAME="SBIG_Aluma_X2.pkg"
BUNDLE_NAME="org.rti-zone.SBIGAlumaX2"

install_name_tool -change  libftd2xx.dylib /usr/local/lib/libftd2xx.dylib ../build/Release/libSBIG_Aluma.dylib
install_name_tool -change  libftd3xx.dylib /usr/local/lib/libftd3xx.dylib ../build/Release/libSBIG_Aluma.dylib

if [ ! -z "$app_id_signature" ]; then
codesign -f -s "$app_id_signature" --verbose ../build/Release/libSBIG_Aluma.dylib
codesign -f -s "$app_id_signature" --verbose ../dyn_libs/macos/libftd2xx.dylib
codesign -f -s "$app_id_signature" --verbose ../dyn_libs/macos/libftd3xx.dylib
fi


mkdir -p ROOT/tmp/SBIG_Aluma_X2/
cp "../SBIG_AlumaCamera.ui" ROOT/tmp/SBIG_Aluma_X2/
cp "../SBIG_CamSelect.ui" ROOT/tmp/SBIG_Aluma_X2/
cp "../SBIG_Aluma.png" ROOT/tmp/SBIG_Aluma_X2/
cp "../cameralist SBIG_Aluma.txt" ROOT/tmp/SBIG_Aluma_X2/
cp "../build/Release/libSBIG_Aluma.dylib" ROOT/tmp/SBIG_Aluma_X2/
cp "../dyn_libs/macos/libftd2xx.dylib" ROOT/tmp/SBIG_Aluma_X2/
cp "../dyn_libs/macos/libftd3xx.dylib" ROOT/tmp/SBIG_Aluma_X2/

if [ ! -z "$installer_signature" ]; then
	# signed package using env variable installer_signature
	pkgbuild --root ROOT --identifier "$BUNDLE_NAME" --sign "$installer_signature" --scripts Scripts --version 1.0 "$PACKAGE_NAME"
	pkgutil --check-signature "./${PACKAGE_NAME}"
	res=`xcrun altool --notarize-app --primary-bundle-id "$BUNDLE_NAME" --username "$AC_USERNAME" --password "@keychain:AC_PASSWORD" --file $PACKAGE_NAME`
	RequestUUID=`echo $res | grep RequestUUID | cut -d"=" -f 2 | tr -d [:blank:]`
	if [ -z "$RequestUUID" ]; then
		echo "Error notarizing"
		echo "res = $res"
		exit 1
	fi
	echo "Notarization RequestUUID '$RequestUUID'"
	sleep 30
	while true
	echo "Waiting for notarization"
	do
		res=`xcrun altool --notarization-info "$RequestUUID" --username "pineau@rti-zone.org" --password "@keychain:AC_PASSWORD"`
		pkg_ok=`echo $res | grep -i "Package Approved"`
		pkg_in_progress=`echo $res | grep -i "in progress"`
		if [ ! -z "$pkg_ok" ]; then
			break
		elif [ ! -z "$pkg_in_progress" ]; then
			sleep 60
		else
			echo  "Notarization timeout or error"
			echo "res = $res"
			exit 1
		fi
	done
	xcrun stapler staple $PACKAGE_NAME

else
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --scripts Scripts --version 1.0 $PACKAGE_NAME
fi

rm -rf ROOT
