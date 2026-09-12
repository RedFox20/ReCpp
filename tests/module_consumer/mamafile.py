import mama, os

class RppModuleConsumer(mama.BuildTarget):
    """Imports the exported rpp.strview module, or uses its header instead.

    tests/consumer proves the package exports headers and a library. This target proves
    the separate promise of export_modules: a consumer compiles the module interface unit
    itself, and a toolchain that cannot build a module still gets the header.
    """

    def dependencies(self):
        self.add_local('ReCpp', '../../')

    def settings(self):
        # a host with no libc++ links clang against libstdc++ instead
        if os.getenv('USE_GCC_STDLIB'): self.config.use_gcc_stdlib_for_clang()
        # forces the header fallback on a module-capable compiler, so one toolchain builds both
        # paths. The cmake option covers every generator, and Visual Studio scans modules too
        if os.getenv('NO_MODULES'): self.add_cmake_options('MAMA_ENABLE_MODULES=OFF')
        # drops the two rpp.std targets, which gcc-14 cannot read back on C++23, see BUGS.md B24
        if os.getenv('RPP_NO_STD_MODULE'): self.add_cmake_options('RPP_NO_STD_MODULE=ON')

    def configure(self):
        # CXX23 reaches the targets, so the C++23 consumer run compiles as C++23 and not as C++20
        if os.getenv('CXX23'): self.enable_cxx23()
        else:                  self.enable_cxx20()
