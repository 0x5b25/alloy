#!/bin/bash

if [ -z "$1" ]; then
    echo "No argument supplied. Using default package path"
    PKG_NAME="Metal_Shader_Converter_2.0.pkg"
else
    PKG_NAME=$1
fi

BASEDIR=$(dirname $0)

echo "PKG_NAME=$PKG_NAME"
PKG_DOWNLOAD_URL="https://download.developer.apple.com/Developer_Tools/Metal_shader_converter_beta/Metal_Shader_Converter_2.0.pkg"
TMP_FOLDER="$BASEDIR/tmp"
OUTPUT_FOLDER="$BASEDIR/bin"

EXTRACT_PATH="$TMP_FOLDER/extracted"
PAYLOAD_PATH="$EXTRACT_PATH/MetalShaderConverter.pkg/Payload/usr/local"
DYLIB_PATH="$PAYLOAD_PATH/lib/libmetalirconverter.dylib"

CODESIGN_ID="Apple Development"


if [ -d "$TMP_FOLDER" ]; then rm -Rf $TMP_FOLDER; fi
mkdir -p $TMP_FOLDER

if [ -d "$OUTPUT_FOLDER" ]; then rm -Rf $OUTPUT_FOLDER; fi
mkdir -p $OUTPUT_FOLDER

sign_package()
{
    codesign --force --sign "$CODESIGN_ID" $1
}

prep_framework_folder()
{
    echo "Preping framework folder in $1 ..."
    mkdir -p $1
    cp Info.plist $1/
    cp -a $PAYLOAD_PATH/include/. $1/Headers
}

patch_and_sign()
{
    install_name_tool -id @rpath/MetalShaderConverter.framework/MetalShaderConverter $1/MetalShaderConverter
    sign_package $1
}

#download package
#curl -o $TMP_FOLDER/$PKG_NAME.pkg $PKG_DOWNLOAD_URL

#extract the .pkg
#pkgutil --expand-full $TMP_FOLDER/$PKG_NAME.pkg $TMP_FOLDER/extracted
pkgutil --expand-full $PKG_NAME $TMP_FOLDER/extracted

#build the patcher
clang++ -std=c++20 -g $BASEDIR/MachOPatcher.cpp -o $TMP_FOLDER/patcher

FRAMEWORK_MACOS_DST=$TMP_FOLDER/frameworks/macos/MetalShaderConverter.framework
prep_framework_folder $FRAMEWORK_MACOS_DST
cp $DYLIB_PATH $FRAMEWORK_MACOS_DST/MetalShaderConverter
patch_and_sign $FRAMEWORK_MACOS_DST

FRAMEWORK_IOS_DST=$TMP_FOLDER/frameworks/ios/MetalShaderConverter.framework
prep_framework_folder $FRAMEWORK_IOS_DST
$TMP_FOLDER/patcher "$DYLIB_PATH" ios "$FRAMEWORK_IOS_DST/MetalShaderConverter"
patch_and_sign $FRAMEWORK_IOS_DST

FRAMEWORK_IOSSIM_DST=$TMP_FOLDER/frameworks/iosSim/MetalShaderConverter.framework
prep_framework_folder $FRAMEWORK_IOSSIM_DST
$TMP_FOLDER/patcher "$DYLIB_PATH" iosSim "$FRAMEWORK_IOSSIM_DST/MetalShaderConverter"
patch_and_sign $FRAMEWORK_IOSSIM_DST

#generate the xcframework
xcodebuild -create-xcframework \
    -framework $FRAMEWORK_MACOS_DST \
    -framework $FRAMEWORK_IOS_DST \
    -framework $FRAMEWORK_IOSSIM_DST \
    -output $OUTPUT_FOLDER/MetalShaderConverter.xcframework

sign_package $OUTPUT_FOLDER/MetalShaderConverter.xcframework
cp -R $PAYLOAD_PATH/include $OUTPUT_FOLDER/

rm -Rf $TMP_FOLDER
