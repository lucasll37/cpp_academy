from conan import ConanFile
from conan.tools.gnu import PkgConfigDeps
from conan.tools.meson import MesonToolchain, Meson
from conan.tools.cmake import CMakeDeps
from conan.tools.system.package_manager import Apt
import os
import re

# =============================================================================
#  Dependências por subprojeto
#
#  O Conan só instala o que os subprojetos HABILITADOS usam. "Habilitado" é o
#  que tem `subdir()` descomentado em meson.build (raiz) ou em src/meson.build —
#  os mesmos arquivos que o Meson lê. Comentar um `subdir()` desliga o
#  subprojeto nos dois build systems de uma vez.
#
#  Subprojeto que não aparece aqui não precisa de nada além de `fmt`, que é
#  sempre instalado (o meson.build da raiz o declara para todos).
#  Qt, OpenMP, Boost e Python vêm do sistema, não do Conan.
# =============================================================================
ALWAYS_REQUIRES = ["fmt/10.2.1"]

SUBPROJECT_REQUIRES = {
    "proto":            ["protobuf/3.21.12"],
    "meson":            ["gtest/1.14.0"],
    "testing":          ["gtest/1.14.0"],
    "sandbox":          ["nlohmann_json/3.11.3"],
    "http-client":      ["nlohmann_json/3.11.3", "libcurl/8.6.0"],
    "database":         ["soci/4.0.3", "sqlite3/3.45.0"],
    "coroutines":       ["asio/1.28.0"],
    "network":          ["asio/1.28.0"],
    "rabbitmq":         ["amqp-cpp/4.3.27"],
    "aws/s3":           ["aws-sdk-cpp/1.11.692"],
    "calculus":         ["eigen/3.4.0"],
    "collision":        ["bullet3/3.25"],
    "sfml_bullet":      ["sfml/2.6.1", "bullet3/3.25"],
    "sfml_bullet2":     ["sfml/2.6.1", "bullet3/3.25"],
    "qt_sfml":          ["sfml/2.6.1"],
    "qt_sfml2":         ["sfml/2.6.1", "bullet3/3.25"],
    "flight-simulator": ["spdlog/1.14.1", "jsbsim/1.1.11"],
    "python":           ["pybind11/3.0.1"],
    "yolo":             ["onnxruntime/1.17.3", "opencv/4.9.0"],
    # "yolo":           ["onnxruntime-gpu/1.17.3", "opencv/4.9.0"],  # inclui suporte CUDA
}

SUBPROJECT_TOOL_REQUIRES = {
    "proto":   ["protobuf/3.21.12"],                    # protoc (cross-compilation)
    "parser":  ["flex/2.6.4", "bison/3.8.2"],
    "doxygen": ["doxygen/1.9.4"],                       # HTML/PDF a partir dos comentários
}

# `override=True` só fixa a versão SE o pacote já estiver no grafo (vindo
# transitivamente de outro); não acrescenta nada quando ninguém o pede.
OVERRIDES = ["flac/1.4.3", "opus/1.5.2"]

_SUBDIR_RE = re.compile(r"^\s*subdir\(\s*'([^']+)'\s*\)", re.MULTILINE)


def _active_subdirs(path):
    """`subdir()` não comentados de um meson.build (None se o arquivo não existe)."""
    if not os.path.isfile(path):
        return None
    with open(path, encoding="utf-8") as f:
        return _SUBDIR_RE.findall(f.read())

class MLInferenceRecipe(ConanFile):
    name = "academy"
    version = "1.0.0"
    settings = "arch", "build_type", "compiler", "os"
    exports_sources = "meson*", "src/*", "include/*", "proto/*", "python/*", "tests/*", "conanfile.py"
    # `requirements()` lê estes dois arquivos; no cache do Conan (`conan create`)
    # só o que está em `exports` fica ao lado da receita.
    exports = "meson.build", "src/meson.build"

    def _active_subprojects(self):
        """Subprojetos com `subdir()` ativo, ou None se não der para saber."""
        root = _active_subdirs(os.path.join(self.recipe_folder, "meson.build"))
        if root is None:
            return None
        active = set(root) - {"src"}
        if "src" in root:
            src = _active_subdirs(os.path.join(self.recipe_folder, "src", "meson.build"))
            if src is None:
                return None
            active |= set(src)
        return active

    def _collect(self, table, always=()):
        active = self._active_subprojects()
        if active is None:
            # Sem os meson.build não há como filtrar: instala tudo.
            active = set(table)
        refs = list(always)
        for name, deps in table.items():
            if name in active:
                refs += [d for d in deps if d not in refs]
        return refs
    
    def system_requirements(self):
        # graphviz desenha os diagramas do Doxygen
        if "doxygen/1.9.4" in self._collect(SUBPROJECT_TOOL_REQUIRES):
            apt = Apt(self)
            apt.install(["graphviz"])

    def requirements(self):
        for ref in self._collect(SUBPROJECT_REQUIRES, ALWAYS_REQUIRES):
            self.requires(ref)
        for ref in OVERRIDES:
            self.requires(ref, override=True)
        # self.requires("qt/6.7.3")

    def build_requirements(self):
        for ref in self._collect(SUBPROJECT_TOOL_REQUIRES):
            self.tool_requires(ref)

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