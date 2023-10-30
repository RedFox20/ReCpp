import mama
import os

class ReCpp(mama.BuildTarget):

    def dependencies(self):
        # if no preference, prefer gcc since its support is better in 2023
        self.prefer_gcc()
        # for a few specific target we use source-built elfutils for libdw support
        if self.mips:
            self.add_git('elfutils', 'https://github.com/RedFox20/elfutils-package.git')


    def configure(self):
        if self.windows and os.getenv('APPVEYOR') != None:
            self.add_cl_flags('/DAPPVEYOR')

        def enable_from_env(name, enabled='ON', force=False):
            env = os.getenv(name)
            if force or (env and (env == '1' or env == 'ON' or env == 'TRUE')):
                self.add_cmake_options(f'{name}={enabled}')

        # enable CMAKE opts if env vars are enabled
        enable_from_env('BUILD_TESTS', force=self.config.test != '')
        enable_from_env('BUILD_WITH_MEM_SAFETY')
        enable_from_env('BUILD_WITH_THREAD_SAFETY')
        enable_from_env('CXX17', force=self.is_enabled_cxx17())
        enable_from_env('CXX20', force=self.is_enabled_cxx20())


    def package(self):
        os.makedirs(self.build_dir('include/rpp/'), exist_ok=True)
        self.copy(self.source_dir('src/rpp/'),
                  self.build_dir('include/rpp/'), filter=['.h','.natvis'])
        self.export_libs('.', ['ReCpp.lib', 'ReCpp.a'])
        self.export_includes('include', build_dir=True)

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


    def deploy(self):
        # deploy directly to build directory
        self.papa_deploy(f'.', src_dir=False)


    def test(self, args):
        if 'nogdb' in args:
            args = args.replace('nogdb', '')
            self.run_program(self.source_dir('bin'), self.source_dir(f'bin/RppTests {args}'))
        else:
            self.gdb(f"bin/RppTests {args}", src_dir=True)

