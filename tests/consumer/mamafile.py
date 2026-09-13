import mama
from mama.utils.system import error
import glob, sys

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
        self.check_dependency_built_no_tests()
        self.run_program(self.source_dir('bin'), self.source_dir('bin/RppConsumer'))

    def check_dependency_built_no_tests(self):
        """Fails when the ReCpp dependency turned BUILD_TESTS on.

        mama shares one config across the tree. So a `test` argument aimed at this consumer
        used to reach the mamafile of ReCpp and build its whole test suite.
        """
        for cache in glob.glob(self.source_dir('packages/ReCpp/*/CMakeCache.txt')):
            if 'BUILD_TESTS:BOOL=ON' in open(cache, encoding='utf-8').read():
                error(f'ReCpp built its own tests as a dependency: {cache} sets BUILD_TESTS=ON')
                sys.exit(-1)
