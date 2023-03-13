import mama
import os

class ReCpp(mama.BuildTarget):

    def dependencies(self):
        pass

    def configure(self):
        pass

    def package(self):
        self.export_libs('.', ['ReCpp.lib', 'ReCpp.a'])
        self.export_includes('.')  # So: #include <rpp/strview.h>
        if self.raspi or self.oclea:
            self.export_syslib('dl')
            self.export_syslib('rt')
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
