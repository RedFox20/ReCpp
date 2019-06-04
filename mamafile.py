import mama

class ReCpp(mama.BuildTarget):

    def dependencies(self):
        pass

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
        self.gdb(f'RppTests {args}', src_dir=False)
