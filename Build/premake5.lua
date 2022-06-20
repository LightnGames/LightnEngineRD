local build_dir="../__Intermediate\\"

location(build_dir)
workspace "LightnEngineDemo"
configurations { "Debug", "Release" }

project "Game"
   kind "WindowedApp"
   language "C++"
   buildoptions "/MP"
   links { "LightnEngine" }
   includedirs { "../LightnEngine/source/" }
   files { "../Game/source/**.h", "../Game/source/**.cpp" }
   flags { "MultiProcessorCompile" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      architecture "x86_64"
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      architecture "x86_64"
      optimize "On"

project "LightnEngine"
    kind "SharedLib"
    language "C++"
    buildoptions "/MP"
    links{"d3d12.lib", "dxgi.lib", "dxguid.lib", "d3dcompiler.lib", "Ws2_32.lib", "WinPixEventRuntime.lib"}
    includedirs { "../LightnEngine/source/" }
    libdirs { "../LightnEngine/lib/WinPixEventRuntime/" }
    files { "../LightnEngine/source/**.h", "../LightnEngine/source/**.c", "../LightnEngine/source/**.cpp" }
    flags { "MultiProcessorCompile" }
    prebuildcommands { "copy $(LTN_ROOT)\\LightnEngine\\lib\\WinPixEventRuntime\\WinPixEventRuntime.dll $(SolutionDir)bin\\$(Configuration)\\" }

    filter "configurations:Debug"
        targetdir(build_dir.."bin\\Debug")
        objdir(build_dir.."obj\\Debug")
        defines { "_DEBUG", "_CONSOLE", "LTN_DEBUG", "LTN_EXPORT" }
        architecture "x86_64"
        symbols "On"

    filter "configurations:Release"
        targetdir(build_dir.."bin\\Release")
        objdir(build_dir.."obj\\Release")
        defines { "NDEBUG", "_CONSOLE", "LTN_EXPORT" }
        architecture "x86_64"
        optimize "On"