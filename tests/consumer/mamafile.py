import mama
from mama.utils.system import console, error
import os, sys

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
        must not reach the mamafile of ReCpp and build its whole test suite.
        """
        # `test all`, `update` and `with_tests` ask mama for the tests of every target
        if self.config.targets_all() or self.config.with_tests:
            console('this run asked for the tests of every target, so this check does not apply')
            return
        cache = os.path.join(self.get_dependency('ReCpp').build_dir, 'CMakeCache.txt')
        # only the build dir of this run, because a compiler switch leaves an older one beside it
        if not os.path.exists(cache):
            console(f'ReCpp built no cache, so this check found nothing to read: {cache}')
            return
        with open(cache, encoding='utf-8', errors='ignore') as f:
            if 'BUILD_TESTS:BOOL=ON' in f.read():
                error(f'ReCpp built its own tests as a dependency: {cache} sets BUILD_TESTS=ON. '
                       'A warm build dir keeps that value, because mama configures it only once. '
                       'Run `mama configure` here, or delete packages/.')
                sys.exit(-1)
        console('ReCpp built as a dependency with BUILD_TESTS=OFF')
