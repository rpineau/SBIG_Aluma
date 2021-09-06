#!/bin/zsh
install_name_tool -change  libftd2xx.dylib /usr/local/lib/libftd2xx.dylib /Applications/Astronomy/TheSkyX\ Professional\ Edition.app/Contents/PlugIns/CameraPlugIns/libSBIG_Aluma.dylib
install_name_tool -change  libftd3xx.dylib /usr/local/lib/libftd3xx.dylib /Applications/Astronomy/TheSkyX\ Professional\ Edition.app/Contents/PlugIns/CameraPlugIns/libSBIG_Aluma.dylib
codesign -f -s "$app_id_signature" --verbose /Applications/Astronomy/TheSkyX\ Professional\ Edition.app/Contents/PlugIns/CameraPlugIns/libSBIG_Aluma.dylib

