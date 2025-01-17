
## How To

### Setup

```ps1
vcpkg install --x-install-root "externals" --triplet "x64-windows"
```
```bash
meson setup "build" --cross-file meson-x64-osx.ini -Dtests=true
```

```ps1
meson setup --backend vs2022 --vsenv `
    --cross-file "meson-x64-windows.ini" `
    --pkg-config-path "$(Get-Location)/externals/x64-windows/debug/lib/pkgconfig" `
    --buildtype debug `
    "build"
```

```ps1
meson setup --backend ninja `
    --cross-file "meson-x64-windows.ini" `
    --buildtype release `
    "build"
```

### Build

```ps1
meson compile -C "build"
```

```ps1
meson compile -C "build" "RUN_cmake_copy_bins"
meson compile -C "build" "RUN_cmake_copy_bins_debug"
```

### Test

```ps1
meson test -C "build"
```
