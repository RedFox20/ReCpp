import mama

class ReCpp(mama.BuildTarget):
    local_workspace = 'build'
    def dependencies(self):
        pass

    def package(self):
        self.export_libs('.', ['ReCpp.lib', 'ReCpp.a'])
        self.export_includes('.')  # So: #include <rpp/strview.h>
        if self.linux:
            self.export_syslib('dl')
            self.export_syslib('dw')
            self.export_syslib('rt')
        if self.android:
            self.export_syslib('android')
            self.export_syslib('log')

    def test(self, args):
        self.gdb('RppTests', src_dir=False)
