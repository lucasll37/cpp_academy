from conan import ConanFile
from conan.tools.gnu import PkgConfigDeps
from conan.tools.meson import MesonToolchain, Meson
from conan.tools.cmake import CMakeDeps

class MLInferenceRecipe(ConanFile):
    name = "academy"
    version = "1.0.0"
    settings = "arch", "build_type", "compiler", "os"
    exports_sources = "meson*", "src/*", "include/*", "proto/*", "python/*", "tests/*", "conanfile.py"

    def requirements(self):
        self.requires("gtest/1.14.0")
        self.requires("protobuf/3.21.12")
        self.requires("fmt/10.2.1")
        self.requires("spdlog/1.14.1")
        self.requires("nlohmann_json/3.11.3")
        self.requires("pybind11/3.0.1")
        self.requires("eigen/3.4.0")
        self.requires("bullet3/3.25")
        self.requires("sfml/2.6.1")
        self.requires("libcurl/8.6.0")
        self.requires("soci/4.0.3")
        self.requires("sqlite3/3.45.0")
        # self.requires("qt/6.7.3")
        
    def build_requirements(self):
        self.tool_requires("protobuf/3.21.12") # Cross-Compilation
        self.tool_requires("flex/2.6.4")
        self.tool_requires("bison/3.8.2")

        # Documentação — gera HTML/PDF a partir de comentários no código
        # self.tool_requires("doxygen/1.9.4")

        # Build systems alternativos — úteis ao integrar dependências que não usam Meson
        # self.tool_requires("cmake/3.27.0")
        # self.tool_requires("meson/1.2.0")
        # self.tool_requires("ninja/1.13.2")
        
    def configure(self):
        self.options["soci"].with_sqlite3    = True
        self.options["soci"].with_postgresql = True 

    def generate(self):
        pc = PkgConfigDeps(self)
        pc.generate()
        
        tc = MesonToolchain(self)
        tc.generate()
        
        cmake = CMakeDeps(self)
        cmake.generate()

    def layout(self):
        self.folders.build = "build"
        self.folders.generators = "build"

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        meson = Meson(self)
        meson.install()

    def package_info(self):
        self.cpp_info.libs = ["academy"]
        self.cpp_info.includedirs = ["include"]