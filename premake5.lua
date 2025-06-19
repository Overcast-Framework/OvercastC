workspace "Overcast"
   configurations { "Debug", "Release" }
   platforms { "Win64" }

filter { "platforms:Win64" }
    system "Windows"
    architecture "x86_64"

project "Overcast" -- compiler project
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++17"
   
   targetdir "bin/%{cfg.buildcfg}"

   files { "src/Overcast/**.h", "src/Overcast/**.cc" }

   includedirs { "src", "vendors/llvm-project/build/include", "vendors/llvm-project/llvm/include" }

   libdirs { "vendors/llvm-project/build/lib" }

   staticruntime "off"

   function libnames(paths)
    local names = {}
    for _, path in ipairs(paths) do
        local name = path:match("([^\\/]+)%.lib$")
print(name)
        table.insert(names, name..".lib")
    end
    return names
   end

   links("ntdll")
   links(libnames(os.matchfiles("C:/Users/bitte/source/repos/OvercastC/vendors/llvm-project/build/lib/*.lib")))

   defines {
    "LLVM_STATIC",
    "__STDC_CONSTANT_MACROS",
    "__STDC_FORMAT_MACROS",
    "__STDC_LIMIT_MACROS"
   }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

project "Overclad" -- lexer gen
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++17"

   targetdir "toolset/bin/%{cfg.buildcfg}"

   files {"src/Overclad/**.h", "src/Overclad/**.cc"}

   includedirs { "src/Overclad" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"