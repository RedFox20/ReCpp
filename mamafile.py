import mama

class ReCpp(mama.BuildTarget):
    local_workspace = 'build'
    def dependencies(self):
        pass

    def package(self):
        self.export_libs('.', ['ReCpp.lib', 'ReCpp.a'])
        self.export_includes('.') # #include <rpp/strview.h>

    def test(self):
        self.run('./RppTests')