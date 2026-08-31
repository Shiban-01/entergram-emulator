@echo off
call "C:\PROGRA~1\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d "C:\Users\francisco.q\Desktop\entergram-emulator"
"C:\PROGRA~1\CMake\bin\cmake.exe" -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_CXX_FLAGS="-DHAVE_SWRESAMPLE"
if %ERRORLEVEL% NEQ 0 (
    echo CMAKE CONFIGURE FAILED
    exit /b 1
)
cmake --build build --config Debug
if %ERRORLEVEL% NEQ 0 (
    echo BUILD FAILED
    exit /b 1
)
echo BUILD SUCCESS
echo Running test_rom_reader...
build\test_rom_reader.exe
echo Running test_snr_vm...
build\test_snr_vm.exe
echo Running test_snr0_parser...
build\test_snr0_parser.exe
echo Running test_nxa...
build\test_nxa.exe
echo All tests complete.
