set_project("graphics_course")
set_version("0.1.0")
set_config("builddir", path.join(os.projectdir(), "build"))
set_languages("c++20")
set_warnings("all", "extra", "pedantic")
set_policy("build.warning", true)
if is_plat("windows") then
    add_cxxflags("/utf-8", "/permissive-", "/volatile:iso", "/Zc:inline", "/Zc:wchar_t", "/EHsc", "/Zc:__cplusplus", "/Zc:preprocessor")
end

add_rules("mode.debug", "mode.release")

add_requires("glfw 3.4", {configs = {shared = false}})
add_requires("imgui v1.91.8", {
    configs = {
        shared = false,
        glfw = true,
        vulkan_no_proto = true
    }
})
add_requires("glm")
add_requires("tinygltf 2.9.2")
add_requires("function2 4.2.4")
add_requires("vulkan-memory-allocator", "spirv-reflect", "fmt 12.2.0", "tracy 0.11.1")
add_requires("spdlog 1.15.3", {configs = {header_only = false, fmt_external = true, shared = false}})
add_requireconfs("spdlog.fmt", {version = "12.2.0", override = true})

add_requires("vulkan-headers", "vulkan-loader")
add_requires("glslang", {configs = {binaryonly = true}})
add_packages("vulkan-headers", "vulkan-loader", "glslang")

add_defines(
    'GRAPHICS_COURSE_RESOURCES_ROOT="' .. path.join(os.projectdir(), "resources"):gsub("\\", "/") .. '"',
    'GRAPHICS_COURSE_ROOT="' .. os.projectdir():gsub("\\", "/") .. '"'
)

local etna_root = path.join(os.projectdir(), "build", "_deps", "etna")
if not os.isdir(etna_root) then
    os.mkdir(path.directory(etna_root))
    os.execv("git", {"clone", "--depth", "1", "--branch", "v1.10.1",
        "https://github.com/AlexandrShcherbakov/etna.git", etna_root})
end

target("etna")
    set_kind("static")
    add_files(path.join(etna_root, "etna", "source", "*.cpp"))
    add_includedirs(path.join(os.projectdir(), "xmake"), {public = true})
    add_includedirs(path.join(etna_root, "etna", "include"), {public = true})
    add_includedirs(path.join(etna_root, "etna", "source"))
    add_packages("vulkan-memory-allocator", "spirv-reflect", "fmt", "spdlog", "tracy", {public = true})
    add_defines("VULKAN_HPP_NO_STRUCT_CONSTRUCTORS", "VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1",
        "VULKAN_HPP_NO_EXCEPTIONS", "TRACY_VK_USE_SYMBOL_TABLE",
        "VMA_STATIC_VULKAN_FUNCTIONS=0", "VMA_DYNAMIC_VULKAN_FUNCTIONS=1", {public = true})
    if is_mode("debug") then
        add_defines("ETNA_DEBUG=1", {public = true})
        add_defines("ETNA_SET_VULKAN_DEBUG_NAMES")
    end
target_end()

function set_target_layout(layout)
    local target_root = path.join(os.projectdir(), "build", layout)
    set_targetdir(target_root)
    set_objectdir(path.join(os.projectdir(), "build", ".objs", layout))
    set_values("output_dir", target_root)
end

rule("compile.glsl")
    on_load(function (target)
        local shader_output = path.join(target:values("output_dir"), "shaders")
        target:add("defines", string.upper(target:name()) .. "_SHADERS_ROOT=\"" .. shader_output:gsub("\\", "/") .. "/\"")
    end)
    before_build(function (target)
        import("lib.detect.find_tool")
        local validator = assert(find_tool("glslangValidator", {envs = target:pkgenvs()})
            or find_tool("glslang", {envs = target:pkgenvs()}), "glslang compiler not found")
        local target_name = target:name()
        local shader_directory = target:values("shader_directory")
        local shader_files = {}
        for _, extension in ipairs({"vert", "frag", "comp", "geom", "tesc", "tese"}) do
            local matches = os.files(path.join(target:scriptdir(), shader_directory, "*." .. extension))
            for _, shader in ipairs(matches) do
                table.insert(shader_files, shader)
            end
        end
        local shader_output = path.join(target:values("output_dir"), "shaders")
        assert(#shader_files > 0, "No shaders found for " .. target_name .. " in " .. shader_directory)
        os.mkdir(shader_output)
        for _, shader in ipairs(shader_files) do
            local output = path.join(shader_output, path.filename(shader) .. ".spv")
            local args = {"-V", shader, "-o", output,
                "-I" .. path.join(os.projectdir(), "common", "render_utils", "shaders")}
            if is_mode("debug") then
                table.insert(args, 1, "-g")
            end
            os.execv(validator.program, args)
            if not os.isfile(output) then
                raise("glslangValidator did not create " .. output)
            end
        end
    end)
rule_end()

function compile_shaders(shader_files)
    set_values("shader_directory", shader_files)
    add_rules("compile.glsl")
end

includes("common")
includes("samples")
includes("tasks")
