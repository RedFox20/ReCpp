import mama

class RppConsumer(mama.BuildTarget):
    """Builds ReCpp the way another project builds it: as a dependency, not as the root.

    ReCpp's own CI always builds ReCpp as the root target, so it never sees what a
    consumer sees. A dependency build picks its own generator and never sets the
    env vars the root build sets, and that difference has broken a downstream build.
    """

    def dependencies(self):
        self.add_local('ReCpp', '../..')

    def package(self):
        pass

    def test(self, args):
        self.run_program(self.source_dir('bin'), self.source_dir('bin/RppConsumer'))
