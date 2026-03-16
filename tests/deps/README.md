1. git clone https://github.com/quickfix/quickfix.git

2. Copy include folder here

3. For so/dll, you can use CMake:

```bash
cd quickfix
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```
4. Copy the following files to this folder:
- On Linux : libquickfix.so
- On Windows : quickfix.lib and quickfixd.lib for debug mode

5. On Linux, create a symlink for runtime:

```bash
ln -s ./libquickfix.so ./libquickfix.so.<quickfix_version>
```