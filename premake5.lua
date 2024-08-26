workspace("portal")
configurations({ "Debug", "Release" })

-- Client project
project("PortalClient")
kind("ConsoleApp")
language("C")
targetdir("bin/%{cfg.buildcfg}/client")

files({
	"src/client/**.h",
	"src/client/**.c",
	"src/client/warp/**.c",
	"src/shared/**.c",
	"src/shared/**.h",
	"deps/glad/src/glad.c",
})
includedirs({
	"src/shared",
	"/src/client/warp",
	"deps/glad/include",
	"deps/stb_image",
	"deps/stb_image_resize",
	"deps/stb_truetype",
})
links({ "glfw", "clipboard", "cglm", "m", "xcb" })
defines({ "WP_GLFW" })
optimize("Speed")
buildoptions({ "-ffast-math" })

filter("configurations:Debug")
defines({ "DEBUG" })
symbols("On")

filter("configurations:Release")
defines({ "NDEBUG" })
optimize("On")

-- Server project
project("PortalServer")
kind("ConsoleApp")
language("C")
targetdir("bin/%{cfg.buildcfg}/server")

files({ "src/server/**.h", "src/server/**.c", "src/shared/**.c", "src/shared/**.h" })
includedirs({ "src/shared" })

filter("configurations:Debug")
defines({ "DEBUG" })
symbols("On")

filter("configurations:Release")
defines({ "NDEBUG" })
optimize("On")
