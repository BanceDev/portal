workspace "portal"
configurations { "Debug", "Release" }

-- Client project
project "client"
kind "ConsoleApp"
language "C"
targetdir "bin/%{cfg.buildcfg}/client"

files { "src/client/**.h", "src/client/**.c", "src/shared/**.c", "src/shared/**.h" }
includedirs { "src/shared" }

filter "configurations:Debug"
defines { "DEBUG" }
symbols "On"

filter "configurations:Release"
defines { "NDEBUG" }
optimize "On"

-- Server project
project "server"
kind "ConsoleApp"
language "C"
targetdir "bin/%{cfg.buildcfg}/server"

files { "src/server/**.h", "src/server/**.c", "src/shared/**.c", "src/shared/**.h" }
includedirs { "src/shared" }

filter "configurations:Debug"
defines { "DEBUG" }
symbols "On"

filter "configurations:Release"
defines { "NDEBUG" }
optimize "On"
