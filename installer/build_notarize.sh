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
	xcrun notarytool submit "$PACKAGE_NAME" --keychain-profile "$AC_PROFILE" --wait
	xcrun stapler staple $PACKAGE_NAME
else
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --scripts Scripts --version 1.0 $PACKAGE_NAME
fi


rm -rf ROOT
