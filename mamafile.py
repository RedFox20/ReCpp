import mama
import os

class ReCpp(mama.BuildTarget):

    def dependencies(self):
        pass

    def configure(self):
        if self.oclea:
            self.add_cmake_options('BUILD_WITH_LIBDW=FALSE')

    def package(self):
        self.export_libs('.', ['ReCpp.lib', 'ReCpp.a'])
        self.export_includes('.')  # So: #include <rpp/strview.h>
        if self.linux:
            self.export_syslib('dl')
            self.export_syslib('dw', 'libdw-dev')
            self.export_syslib('rt')
        if self.android:
            self.export_syslib('android')
            self.export_syslib('log')
        if self.macos or self.ios:
            self.export_syslib('-framework Foundation')

    def test(self, args):
        if 'nogdb' in args:
            args = args.replace('nogdb', '')
            command = f'cd {self.source_dir("bin")} && ./RppTests {args}'
            # self.run_program(self.source_dir('bin'), self.source_dir(f'bin/RppTests {args}'))
        else:
            gdb =  f'gdb -batch -ex=r -ex=bt -ex=q --args {self.source_dir("bin/RppTests")}'
            command = f'cd {self.source_dir("bin")} && {gdb} {args}'
            # self.run_program(self.source_dir('bin'), gdb)
            # self.gdb(f'bin/RppTests {args}', src_dir=True)
        # TODO: fix subprocess execution (currently broken)
        retcode = os.system(command)
        if retcode != 0: raise Exception(f'{command} failed with return code {retcode}')
