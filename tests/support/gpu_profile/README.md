Compile this fake-device test without Windows or a graphics driver (from the repository root):

```sh
mkdir -p out && clang++ -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/support/gpu_profile -Isrc/core tests/test_gpu_profile.cpp -o out/test_gpu_profile
out/test_gpu_profile
```

This mock is never included in the DLL build.
