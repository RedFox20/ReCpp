import mama
import os

class ReCpp(mama.BuildTarget):

    def dependencies(self):
        pass

    def configure(self):
        if self.windows and os.getenv('APPVEYOR') != None:
            self.add_cl_flags('/DAPPVEYOR')
        if os.getenv('BUILD_WITH_MEM_SAFETY') != None:
            self.add_cmake_options('BUILD_WITH_MEM_SAFETY=ON')
        if os.getenv('BUILD_WITH_THREAD_SAFETY') != None:
            self.add_cmake_options('BUILD_WITH_THREAD_SAFETY=ON')
        
        if os.getenv('CXX17') or self.is_enabled_cxx17():
            self.add_cmake_options('CXX17=TRUE')
        if os.getenv('CXX20') or self.is_enabled_cxx20():
            self.add_cmake_options('CXX20=TRUE')

    def package(self):
        if self.dep.from_artifactory:
            # copy headers to build dir
            os.makedirs(self.build_dir('deploy/ReCpp/include/rpp/'), exist_ok=True)
            self.copy(self.source_dir('src/rpp/'),
                    self.build_dir('deploy/ReCpp/include/rpp/'),
                    filter=['.h','.natvis'])
            self.export_includes('deploy/ReCpp/include', build_dir=True)  # So: #include <rpp/strview.h>
        else:
            self.export_includes('src', build_dir=False)

        self.export_libs('.', ['ReCpp.lib', 'ReCpp.a'])

        if self.raspi or self.oclea:
            self.export_syslib('dl')
            self.export_syslib('rt')
        elif self.mips:
            self.export_syslib('dl')
            self.export_syslib('rt')
            self.export_syslib('atomic')
        elif self.linux:
            self.export_syslib('dl')
            self.export_syslib('dw', 'libdw-dev')
            self.export_syslib('rt')
        elif self.android:
            self.export_syslib('android')
            self.export_syslib('log')
        elif self.macos or self.ios:
            self.export_syslib('-framework Foundation')

    def test(self, args):
        if 'nogdb' in args:
            args = args.replace('nogdb', '')
            self.run_program(self.source_dir('bin'), self.source_dir(f'bin/RppTests {args}'))
        else:
            self.gdb(f"bin/RppTests {args}", src_dir=True)
