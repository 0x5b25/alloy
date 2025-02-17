downloads package from https://download.developer.apple.com/Developer_Tools/Metal_shader_converter_beta/Metal_Shader_Converter_2.0.pkg

Requires apple developer account sign in

Adapted from stackoverflow.com/questions/53818772

stackoverflow.com/questions/48701584

1. Extract the pkg, get the contents
    - command line: `pkgutil --expand-full <your-package.pkg> <output-directory>`
    - example folder layout:
        ```
        metal_shader_converter
        +- ...
        |
        +- Payload
        |  +- opt
        |  |  +- ... [doc and samples]
        |  |
        |  +- usr
        |     +- local
        |       +- bin
        |       |  +- metal_shaderconverter [executable]
        |       |
        |       +- include
        |       |  +- metal_irconverter
        |       |  |  +- ... [lib combanion headers]
        |       |  |
        |       |  +- metal_irconverter_runtime
        |       |     +- ... [runtime helper srcs]
        |       |
        |       +- lib
        |          +- libmetalirconverter.dylib [lib]
        |
        +- Resources
           +- English.lproj
              +- License.rtf
              +- ...
        ```
1. package the `libmetalirconverter.dylib` using `lipo` tool
    - command line: `lipo -create <libname>.dylib -output <libname>`
1. create folder `<libname>.framework`, put `<libname>` and `Info.plist` into it
1. If necessary, use install_name_tool to update id name
    - command line: `install_name_tool -id @rpath/<libname>.framework/<libname> <libname>`
    - This will invalidate code signatures!