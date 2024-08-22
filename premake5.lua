-- premake5.lua
workspace "portal"
configurations { "Debug", "Release" }

project "portal"
kind "ConsoleApp"
language "C"
targetdir "bin/%{cfg.buildcfg}"

files { "**.h", "**.c" }

filter "configurations:Debug"
defines { "DEBUG" }
symbols "On"

filter "configurations:Release"
defines { "NDEBUG" }
optimize "On"
