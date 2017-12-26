from conans import ConanFile, CMake, tools


class RecppConan(ConanFile):
    name = "ReCpp"
    version = "0.1"
    license = "MIT"
    url = "https://github.com/RedFox20/ReCpp"
    description = "Reusable Standalone C++ Libraries and Modules"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = "shared=False"
    generators = "cmake"

    def source(self):
        self.run("git clone https://github.com/RedFox20/ReCpp.git")
        self.run("cd ReCpp && git checkout master")

    def build(self):
        cmake = CMake(self)
        cmake.configure(source_dir="%s/ReCpp" % self.source_folder)
        cmake.build()

        # Explicit way:
        # self.run('cmake %s/hello %s' % (self.source_folder, cmake.command_line))
        # self.run("cmake --build . %s" % cmake.build_config)

    def test(self):
        self.run("bin/RppTests")

    def package(self):
        self.copy("*.h", dst="", src="ReCpp")
        self.copy("*ReCpp.lib", dst="lib", keep_path=False)
        self.copy("*.dll", dst="bin", keep_path=False)
        self.copy("*.so", dst="lib", keep_path=False)
        self.copy("*.dylib", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["ReCpp"]
