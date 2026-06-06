#include "nobpp.h"

int main()
{
    clean_build_dir();
    setup_build_dir();

    StaticLibTarget math("math");
    math.SetCompiler("g++")
        .SetOutputDir("build/lib")
        .AddFlags("-Wall", "-g", "-std=c++23")
        .AddIncludePaths("/")
        .AddSources("math.cpp")
        .Build()
        .Archive();

    DynamicLibTarget physic("physic");
    physic.SetCompiler("g++")
        .SetOutputDir("build/lib")
        .AddFlags("-Wall", "-g", "-std=c++23")
        .AddIncludePaths("/")
        .AddSources("physic.cpp")
        .Build()
        .Shared();

    ExeTarget app("main");
    app.SetCompiler("g++")
        .SetOutputDir("build/bin")
        .AddFlags("-Wall", "-g", "-std=c++23")
        .AddSources("main.cpp")
        .Build()
        .AddLinkerFlags()
        .LinkAgainstStatic(math)
        .LinkAgainstShared(physic)
        .Link();

    app.Run();

    return 0;
}
