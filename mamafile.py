import mama
from mama.utils.system import error
import os, sys, shlex, subprocess

class ReCpp(mama.BuildTarget):

    def settings(self):
        # if no preference, prefer gcc since its support is better in 2023
        self.prefer_gcc()
        if os.getenv('NO_NINJA'):
            self.enable_ninja_build = False


    def dependencies(self):
        # for a few specific target we use source-built elfutils for libdw support
        if self.mips:
            self.add_git('elfutils', 'https://github.com/RedFox20/elfutils-package.git')


    def configure(self):
        # follow mama's clang stdlib choice; getattr keeps this working on older mamabuild
        if getattr(self.config, 'clang_stdlib', 'libc++') != 'libc++':
            self.add_cmake_options('RPP_USE_LIBCXX=OFF')

        # enable CMAKE opts if env vars are enabled
        self.enable_from_env('BUILD_TESTS', force=self.config.test != '')
        self.enable_from_env('BUILD_WITH_MEM_SAFETY')
        self.enable_from_env('CXX17', force=self.is_enabled_cxx17())
        self.enable_from_env('CXX20', force=self.is_enabled_cxx20())
        self.enable_from_env('CXX23', force=self.is_enabled_cxx23())
        self.enable_from_env('CXX26', force=self.is_enabled_cxx26())
        self.enable_from_env('BUILD_WITH_MODULES')


    def package(self):
        self.link_compile_commands()
        self.export_include('src/rpp', build_dir=False,
                            includes_filter=['.h','.natvis'], as_includes_root=True)
        if self.windows:
            self.export_lib(f'{self.cmake_build_type}/ReCpp.lib')
        else:
            self.export_lib('libReCpp.a')

        if self.raspi or self.oclea:
            self.export_syslib('dl')
            self.export_syslib('rt')
        elif self.yocto_linux:
            self.export_syslib('dw')
            self.export_syslib('elf')
            self.export_syslib('z')
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


    def link_compile_commands(self):
        """Points packages/ReCpp/compile_commands.json at packages/ReCpp/<build_dir>/compile_commands.json"""
        target = self.build_dir('compile_commands.json') # the actual thing
        if os.path.exists(target):
            try:
                link = self.build_dir('../compile_commands.json')
                if os.path.islink(link) or os.path.exists(link): os.remove(link)
                os.symlink(f'{self.dep.build_dir_name}/compile_commands.json', link)
            except OSError:
                pass # an IDE convenience never fails the build


    def deploy(self):
        # a subdir of the build dir, because the module strip cannot edit the build dir in place
        self.papa_deploy('ReCpp', src_dir=False)


    # The whole suite runs in ~5 seconds, so this limit only ever triggers on a deadlock.
    # It turns a hung CI job into a clear failure instead of a job that runs until the
    # platform kills it, which gives no output at all.
    TEST_TIMEOUT_SECONDS = 30

    def test(self, args):
        if 'nogdb' in args:
            args = args.replace('nogdb', '')
            self.run_tests_with_timeout(args)
        else:
            self.gdb(f"bin/RppTests {args}", src_dir=True) # GDB drives the timing, do not interrupt it

    def run_tests_with_timeout(self, args):
        """Runs RppTests with a deadlock timeout unless repeat mode is active."""
        bin_dir = self.source_dir('bin')
        test_args = shlex.split(args)
        command = [self.source_dir('bin/RppTests')] + test_args
        # Repeat mode runs until failure, so a fixed timeout would stop a healthy run.
        is_repeat = '-r' in test_args or '--repeat' in test_args
        timeout = None if is_repeat else self.TEST_TIMEOUT_SECONDS
        try:
            result = subprocess.run(command, cwd=bin_dir, timeout=timeout)
        except subprocess.TimeoutExpired:
            error(f'RppTests timed out after {self.TEST_TIMEOUT_SECONDS}s: a test is hung. '
                   'The last test name printed before this is the one that hung. '
                   'Run the same test without `nogdb` to attach GDB and get the stack.')
            sys.exit(-1)
        if result.returncode != 0:
            error(f'RppTests failed with exit code {result.returncode}')
            sys.exit(result.returncode)
