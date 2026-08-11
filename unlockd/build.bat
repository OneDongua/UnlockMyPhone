cmake -S . -B build -G Ninja ^
  -DCMAKE_MAKE_PROGRAM="D:\Android\Sdk\cmake\3.22.1\bin\ninja.exe" ^
  -DCMAKE_TOOLCHAIN_FILE="%ANDROID_NDK_HOME%\build\cmake\android.toolchain.cmake" ^
  -DANDROID_ABI=arm64-v8a ^
  -DANDROID_PLATFORM=android-23

cmake --build build