from conan import ConanFile
from conan.tools.gnu import PkgConfigDeps
from conan.tools.meson import MesonToolchain, Meson
from conan.tools.cmake import CMakeDeps
from conan.tools.system.package_manager import Apt

class MLInferenceRecipe(ConanFile):
    name = "academy"
    version = "1.0.0"
    settings = "arch", "build_type", "compiler", "os"
    exports_sources = "meson*", "src/*", "include/*", "proto/*", "python/*", "tests/*", "conanfile.py"
    
    def system_requirements(self):
        apt = Apt(self)
        apt.install(["graphviz"])

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
        self.requires("asio/1.28.0")
        self.requires("flac/1.4.3", override=True)
        self.requires("aws-sdk-cpp/1.11.692")
        self.requires("amqp-cpp/4.3.27")
        self.requires("jsbsim/1.1.11")
        self.requires("onnxruntime/1.17.3")
        # self.requires("onnxruntime-gpu/1.17.3)
        # inclui suporte CUDA
        self.requires("opus/1.5.2", override=True)
        self.requires("opencv/4.9.0") 
        # self.requires("qt/6.7.3")
        
    def build_requirements(self):
        self.tool_requires("protobuf/3.21.12") # Cross-Compilation
        self.tool_requires("flex/2.6.4")
        self.tool_requires("bison/3.8.2")

        # Documentação — gera HTML/PDF a partir de comentários no código
        self.tool_requires("doxygen/1.9.4")

        # Build systems alternativos — úteis ao integrar dependências que não usam Meson
        # self.tool_requires("cmake/3.27.0")
        # self.tool_requires("meson/1.2.0")
        # self.tool_requires("ninja/1.13.2")
        
    def configure(self):
        self.settings.compiler.cppstd = "20"   # ← força C++20 no projeto
        self.options["soci"].with_sqlite3    = True
        self.options["soci"].with_postgresql = True 
        
        self.options["aws-sdk-cpp"].s3       = True
        
        self.options["opencv"].with_ffmpeg    = False
        self.options["opencv"].with_wayland   = False
        self.options["opencv"].with_v4l       = True
        self.options["opencv"].with_gtk       = True
        self.options["opencv"].dnn            = False  # não precisa — usamos ONNX Runtime
        self.options["opencv"].with_jpeg2000  = False
        self.options["opencv"].with_tiff      = False
        self.options["opencv"].with_webp      = False

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