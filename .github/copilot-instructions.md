# ReCpp Copilot Instructions

## Testing changes
This project uses mama build tool, and a custom RppTest framework for unit tests.

1. **Compile with C++20 and run all tests**: After modifying any code, ensure that the project compiles with C++20 and that all tests pass.
```bash
# build without reconfigure (faster if you do not need to fully reconfigure)
# -vv means extra verbose, -v means medium verbose
CXX20=1 mama gcc build test="-vv"

# reconfigure CMake and build everything
CXX20=1 mama gcc rebuild test="-vv"
```
2. **Run specific unit test suite**: If you modified a specific module, you can run only the relevant unit tests to save time. For example, if you modified `concurrent_queue.h`, you can run find the matching `TestImpl(test_concurrent_queue)` in `test_concurrent_queue.cpp` and run that test file:
```bash
CXX20=1 mama gcc build test="-vv test_concurrent_queue"
```
3. **Run a specific failing test**: If you have a specific test that is failing, you can run just that test using the following command. You can find the test name in the test output, it will be something like `test_concurrent_queue::push_and_pop`:
```bash
CXX20=1 mama gcc build test="-vv test_concurrent_queue::push_and_pop"
```
4. **Run tests with sanitizers**: If you want to check for memory or threading errors, you can build and run the tests with sanitizers enabled. It usually needs a full rebuild to apply a change in sanitizer flags. Later you can use regular build command.
```bash
# run AddressSanitizer without gdb, because it can interfere with ASAN
CXX20=1 mama gcc asan build test="nogdb -vv"

# run ThreadSanitizer
CXX20=1 mama gcc tsan build test="nogdb -vv"

# run a specific test with ThreadSanitizer until failure:
CXX20=1 mama gcc tsan build test="nogdb -vv test_concurrent_queue::push_and_pop" test_until_failure=20
```
5. **You can choose between gcc or clang**: By default, the build uses gcc, but you can switch to clang by replacing `gcc` with `clang` in the above commands. Note that a full rebuild is needed when switching compilers.
```bash
# build with clang
CXX20=1 mama clang build test="-vv"
```
6. **Run tests on windows**: If you are running on Windows, then MSVC++ is the default compiler. The run command does not specify a compiler and doesn't specify nogdb. You can still enable sanitizers with asan/tsan options.
```cmd
mama build test="-vv"
```
7. **Running clang-tidy for static analysis**: You can run clang-tidy to check for common C++ issues and enforce coding standards. This is especially useful for catching potential bugs or issues in the code. There is a .clang-tidy config file in the project root.
```bash
CXX20=1 mama gcc build clang-tidy test
```

## After modifying any header file in `src/rpp/`

1. **Update line references**: Run `python3 update_doc_linerefs.py --dry-run` from the repo root to check if any README.md source line references need updating. If updates are needed, run `python3 update_doc_linerefs.py` (without `--dry-run`) to apply them.

2. **Document new public API**: If a new public function, method, struct, enum, or constant was added to a header, add a corresponding entry to the relevant section in `README.md`. Use the format:
   ```markdown
   | [`function_name(type param, type param)`](src/rpp/header.h#L123) | Brief description |
   ```
   The display text in backticks should match the actual declaration signature closely enough that `update_doc_linerefs.py` can find and track it. Use the same parameter type names and argument names as in the source.

3. **Check for undocumented API**: Run `python3 update_doc_linerefs.py --check-undocumented` to find public declarations in headers that are missing from README.md. Any reported items should be added to the appropriate section in README.md.
