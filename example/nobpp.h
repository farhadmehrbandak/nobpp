#include <print>
#include <format>
#include <string>
#include <vector>
#include <array>
#include <string_view>
#include <filesystem>
#include <expected>
#include <ranges>
#include <set>

#if defined(__linux__) &&                                                              \
    defined(__GNUC__) && (__GNUC__ > 14 || (__GNUC__ == 14 && __GNUC_MINOR__ >= 0)) && \
    __cplusplus >= 202302L

namespace fs = std::filesystem;

std::string gBuild_path = "build";
std::string gObj_path = "obj";

constexpr const char *RESET = "\033[0m";
constexpr const char *RED = "\033[31m";
constexpr const char *GREEN = "\033[32m";
constexpr const char *YELLOW = "\033[33m";
constexpr const char *BLUE = "\033[34m";

enum class Status
{
    INFO,
    WARNING,
    ERROR,
};

void print_status(std::string_view msg, Status stat)
{
    switch (stat)
    {
    case Status::INFO:
        std::println("{}[INFO]{} {}", BLUE, msg, RESET);
        break;
    case Status::WARNING:
        std::println("{}[WARNING]{} {}", YELLOW, msg, RESET);
        break;
    case Status::ERROR:
        std::println("{}[ERROR]{} {}", RED, msg, RESET);
        break;
    }
}

std::string path_concater(std::string &first, std::string &second)
{
    std::string joined;
    joined += first;
    joined += "/";
    joined += second;
    return joined;
}

std::expected<std::filesystem::path, std::error_code> setup_directories(std::string_view path)
{
    std::error_code ec;

    if (std::filesystem::create_directories(path, ec))
    {
        return std::filesystem::path(path);
    }
    else
    {
        if (ec)
            return std::unexpected(ec);

        return std::filesystem::path(path);
    }
}

void setup_build_dir()
{
    if (auto result = setup_directories(gBuild_path); !result)
    {
        std::string msg = std::format("Failed: {}", result.error().message());
        print_status(msg, Status::ERROR);
    }
    std::string joined = path_concater(gBuild_path, gObj_path);
    if (auto result = setup_directories(joined); !result)
    {
        std::string msg = std::format("Failed: {}", result.error().message());
        print_status(msg, Status::ERROR);
    }
}

void clean_build_dir()
{
    print_status("Cleaning build directory", Status::INFO);
    std::error_code ec;
    if (fs::remove_all("build", ec))
    {
        print_status("Clean successful", Status::INFO);
    }
    else if (ec)
    {
        print_status(std::format("Clean failed: {}", ec.message()), Status::ERROR);
    }
}

std::string join(const std::vector<std::string> &vec)
{
    if (vec.empty())
        return "";
    std::string result;
    for (size_t i = 0; i < vec.size(); ++i)
    {
        result += vec[i];
        if (i != vec.size() - 1)
            result += " ";
    }
    return result;
}

std::string join(const std::set<std::string> &set, const std::string &delimiter = " ")
{
    if (set.empty())
        return "";

    std::string result;
    auto it = set.begin();

    result += *it;
    ++it;

    for (; it != set.end(); ++it)
    {
        result += delimiter;
        result += *it;
    }

    return result;
}

bool needs_rebuild(const fs::path &src, const fs::path &obj)
{
    if (!fs::exists(obj))
        return true;

    return fs::last_write_time(src) > fs::last_write_time(obj);
}

template <typename T>
concept StringVariant =
    std::convertible_to<T, std::string_view>;

enum class LibraryKind
{
    Static,
    Shared
};

template <typename T>
concept StaticLibrary =
    requires { T::kind; } &&
    (T::kind == LibraryKind::Static);

template <typename T>
concept SharedLibrary =
    requires { T::kind; } &&
    (T::kind == LibraryKind::Shared);

template <typename Derived>
class TargetBase
{
protected:
    std::string name_;
    std::string compiler_ = "g++";
    std::vector<std::string> flags_;
    std::vector<std::string> include_paths_;
    std::vector<std::string> sources_;
    std::vector<std::string> objects_;
    std::vector<std::string> defines_;
    std::vector<std::string> dependencies_;
    std::string dependency_flag_ = "-MMD -MF";

public:
    TargetBase(std::string_view name) : name_(name) {}

    template <typename... Args>
    Derived &AddSources(Args &&...args)
    {
        (sources_.emplace_back(std::forward<Args>(args)), ...);
        (objects_.emplace_back(create_full_path_file_with_postfix(args, ".o")), ...);
        (dependencies_.emplace_back(create_full_path_file_with_postfix(args, ".d")), ...);

        return static_cast<Derived &>(*this);
    }

    template <typename... Args>
    Derived &AddFlags(Args &&...args)
    {
        (flags_.emplace_back(std::forward<Args>(args)), ...);

        return static_cast<Derived &>(*this);
    }

    template <typename... Args>
    Derived &AddIncludePaths(Args &&...args)
    {
        (include_paths_.emplace_back(std::format("-I{}", args)), ...);

        return static_cast<Derived &>(*this);
    }

    template <typename... Args>
    Derived &AddDefines(Args &&...args)
    {
        (defines_.emplace_back(std::format("-D{}", args)), ...);

        return static_cast<Derived &>(*this);
    }

    Derived &SetCompiler(std::string_view compiler)
    {
        compiler_ = compiler;
        return static_cast<Derived &>(*this);
    }

    Derived &Build()
    {
        return static_cast<Derived *>(this)->BuildImpl();
    }

    const std::string &GetName() const
    {
        return name_;
    }

    std::string GetOutpuFullPath() const
    {
        return static_cast<const Derived *>(this)->GetOutputPathImpl();
    }

    std::string GetOutputDir() const
    {
        return static_cast<const Derived *>(this)->GetOutputDirImpl();
    }

protected:
    // file_postfix can be .o or .d
    std::string create_full_path_file_with_postfix(std::string_view source, std::string_view file_postfix)
    {
        std::string full_path_file;

        std::filesystem::path p(source);
        std::string trimmed_file = p.stem().string();
        trimmed_file += file_postfix;

        full_path_file += gBuild_path;
        full_path_file += "/";
        full_path_file += gObj_path;
        full_path_file += "/";
        full_path_file += name_;
        full_path_file += "/";
        full_path_file += trimmed_file;

        return full_path_file;
    }

    std::string create_object_output_dir()
    {
        std::string full_path_obj_dir;

        full_path_obj_dir += gBuild_path;
        full_path_obj_dir += "/";
        full_path_obj_dir += gObj_path;
        full_path_obj_dir += "/";
        full_path_obj_dir += name_;

        return full_path_obj_dir;
    }
};

class ExeTarget : public TargetBase<ExeTarget>
{
private:
    std::set<std::string> library_paths_;

    // [BEGIN] external libraries
    std::vector<std::string> external_libraries_;
    // [END] external libraries

    // [BEGIN] local libraries (created by the build system for the current project)
    std::vector<std::string> local_static_libraries_;
    std::vector<std::string> local_dynamic_libraries_;
    // needed for .so linux
    std::set<std::string> rpaths_;
    // [END] local libraries

    std::vector<std::string> linker_flags_;
    std::string output_dir_ = "build/bin";

public:
    ExeTarget(std::string_view name) : TargetBase(name) {}

    template <typename... Args>
    ExeTarget &AddLibraryPath(Args &&...args)
    {
        (library_paths_.insert(std::format("-L{}", args)), ...);
        return *this;
    }

    template <typename... Args>
    ExeTarget &AddLinkerFlags(Args &&...args)
    {
        (linker_flags_.emplace_back(std::forward<Args>(args)), ...);
        return *this;
    }

    // LinkAgainst for external library
    template <StringVariant... Args>
    ExeTarget &LinkAgainst(Args &&...args)
    {
        (external_libraries_.emplace_back(std::forward<Args>(args)), ...);
        return *this;
    }

    template <StaticLibrary... Libs>
    ExeTarget &LinkAgainstStatic(const Libs &...libs)
    {
        (link_static(libs), ...);
        return *this;
    }

    template <SharedLibrary... Libs>
    ExeTarget &LinkAgainstShared(const Libs &...libs)
    {
        (link_shared(libs), ...);
        return *this;
    }

    ExeTarget &SetOutputDir(std::string_view dir)
    {
        output_dir_ = dir;
        return *this;
    }

    std::string GetOutputDirImpl() const
    {
        return output_dir_;
    }

    std::string GetOutputPathImpl() const
    {
        std::string ext;
        return (fs::path(output_dir_) / (name_ + ext)).string();
    }

    ExeTarget &BuildImpl()
    {
        setup_directories(create_object_output_dir());

        print_status(std::format("Building target Executable \"{}\"", name_), Status::INFO);
        for (auto const &[src, obj, dependency] : std::views::zip(sources_, objects_, dependencies_))
        {
            std::string cmd = std::format("{} {} {} {} {} -c {} -o {}",
                                          compiler_,
                                          join(flags_),
                                          join(include_paths_),
                                          dependency_flag_,
                                          dependency, src, obj);
            std::println("{}", cmd);
            system(cmd.c_str());
        }

        return *this;
    }

    bool Link()
    {
        print_status(std::format("Linking the Executeble \"{}\"", name_), Status::INFO);

        setup_directories(output_dir_);

        // adding executeble name to the output path
        std::string joined_output = path_concater(output_dir_, name_);

        // gXX -linker_flags -Llibrary_path object -o exe_name
        std::string cmd = std::format("{} {} {} {} {} {} {} {} -o {}",
                                      compiler_,
                                      join(flags_),
                                      local_dynamic_libraries_.empty() ? "" : join(rpaths_),
                                      join(library_paths_),
                                      join(objects_),
                                      join(external_libraries_),
                                      join(local_static_libraries_),
                                      join(local_dynamic_libraries_),
                                      joined_output);

        std::println("{}", cmd);
        system(cmd.c_str());

        return true;
    }

    bool Run()
    {
        print_status(std::format("Running: {}", GetOutputPathImpl()), Status::INFO);
        return system(GetOutputPathImpl().c_str()) == 0;
    }

private:
    template <StaticLibrary Lib>
    void link_static(const Lib &lib)
    {
        library_paths_.insert(std::format("-L{}", lib.GetOutputDir()));
        local_static_libraries_.emplace_back("-l" + lib.GetName());
    }

    template <SharedLibrary Lib>
    void link_shared(const Lib &lib)
    {
        rpaths_.insert(std::format("-Wl,-rpath,{}", lib.GetOutputDir()));
        library_paths_.insert(std::format("-L{}", lib.GetOutputDir()));
        local_dynamic_libraries_.emplace_back("-l" + lib.GetName());
    }
};

class StaticLibTarget
    : public TargetBase<StaticLibTarget>
{

public:
    static constexpr LibraryKind kind = LibraryKind::Static;

private:
    std::string output_dir_ = "build/lib";

public:
    StaticLibTarget(std::string_view name) : TargetBase(name) {}

    StaticLibTarget &SetOutputDir(std::string_view dir)
    {
        output_dir_ = dir;
        return *this;
    }

    std::string GetOutputDirImpl() const
    {
        return output_dir_;
    }

    std::string GetOutputPathImpl() const
    {
        return (fs::path(output_dir_) / ("lib" + name_ + ".a")).string();
    }

    StaticLibTarget &BuildImpl()
    {
        setup_directories(create_object_output_dir());

        print_status(std::format("Building Static Library \"{}\"", name_), Status::INFO);
        for (auto const &[src, obj, dependency] : std::views::zip(sources_, objects_, dependencies_))
        {
            std::string cmd = std::format("{} {} {} {} {} -c {} -o {}",
                                          compiler_,
                                          join(flags_),
                                          join(include_paths_),
                                          dependency_flag_,
                                          dependency, src, obj);

            std::println("{}", cmd);

            system(cmd.c_str());
        }
        return *this;
    }

    bool Archive()
    {
        setup_directories(output_dir_);

        std::string cmd = std::format("ar rcs {} {}",
                                      GetOutputPathImpl(),
                                      join(objects_));
        std::println("{}", cmd);
        return system(cmd.c_str()) == 0;
    }
};

class DynamicLibTarget
    : public TargetBase<DynamicLibTarget>
{

public:
    static constexpr LibraryKind kind = LibraryKind::Shared;

private:
    std::string output_dir_ = "build/lib";

public:
    DynamicLibTarget(std::string_view name) : TargetBase(name) {}

    DynamicLibTarget &SetOutputDir(std::string_view dir)
    {
        output_dir_ = dir;
        return *this;
    }

    std::string GetOutputDirImpl() const
    {
        return output_dir_;
    }

    std::string GetOutputPathImpl() const
    {
        return (fs::path(output_dir_) / ("lib" + name_ + ".so")).string();
    }

    DynamicLibTarget &BuildImpl()
    {
        setup_directories(create_object_output_dir());

        print_status(std::format("Building Dynamic Library \"{}\"", name_), Status::INFO);
        for (auto const &[src, obj, dependency] : std::views::zip(sources_, objects_, dependencies_))
        {
            std::string cmd = std::format("{} {} {} {} {} -c {} -o {}",
                                          compiler_,
                                          join(flags_),
                                          join(include_paths_),
                                          dependency_flag_,
                                          dependency, src, obj);

            std::println("{}", cmd);

            system(cmd.c_str());
        }
        return *this;
    }

    bool Shared()
    {
        setup_directories(output_dir_);

        std::string cmd = std::format("{} -shared {} -o {}",
                                      compiler_,
                                      join(objects_),
                                      GetOutputPathImpl());
        std::println("{}", cmd);
        return system(cmd.c_str()) == 0;
    }
};

#else
#pragma message "gcc must be used with version 14>= with stdc++>23 operation on linux based systems"
#endif
