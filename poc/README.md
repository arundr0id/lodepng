# lodepng Adam7 Integer Overflow RCE - Proof of Concept

## Vulnerability Summary

**Type:** Integer overflow leading to heap buffer overflow (RCE)  
**Location:** `lodepng.cpp`, function `Adam7_getpassvalues()`, lines 4366-4371  
**Root Cause:** 32-bit unsigned arithmetic overflow on 64-bit systems  
**Impact:** Heap corruption via wrong buffer offsets -> arbitrary write -> Remote Code Execution  
**Trigger:** Processing a crafted interlaced PNG file with large dimensions  

## Technical Details

### The Bug

`Adam7_getpassvalues()` computes buffer offsets for each Adam7 interlace pass using `unsigned` (32-bit) arithmetic:

```c
// lodepng.cpp:4366-4367
filter_passstart[i + 1] = filter_passstart[i]
    + ((passw[i] && passh[i]) ? passh[i] * (1u + (passw[i] * bpp + 7u) / 8u) : 0);
```

However, the overflow check (`lodepng_pixel_overflow`) uses `size_t` (64-bit on 64-bit platforms), so it does **not** detect the overflow. The image passes validation but then the wrong (wrapped) offsets are used in:
- `postProcessScanlines()` (line 4762-4786) - uses `filter_passstart[]` for memcpy offsets
- `Adam7_deinterlace()` (line 4678-4716) - uses `passstart[]` for pixel copy offsets

### Triggering Conditions

A 65536x65536 interlaced RGBA (bpp=32) PNG causes overflow in passes 5-6:

| Pass | Width | Height | filter_passstart (32-bit) | filter_passstart (64-bit) | Overflow? |
|------|-------|--------|--------------------------|--------------------------|----------|
| 0    | 8192  | 8192   | 0x20008000              | 0x20008000               | No       |
| 1    | 8192  | 8192   | 0x40010000              | 0x40010000               | No       |
| 2    | 16384 | 8192   | 0x80018000              | 0x80018000               | No       |
| 3    | 16384 | 16384  | 0x00028000              | 0x100028000              | **YES**  |
| 4    | 32768 | 16384  | 0x00048000              | 0x200048000              | **YES**  |
| 5    | 32768 | 32768  | 0x00088000              | 0x400088000              | **YES**  |
| 6    | 65536 | 32768  | 0x00108000              | 0x800108000              | **YES**  |

## Files

- `generate_exploit_png.py` - Generates a crafted PNG that triggers the vulnerability
- `test_exploit.cpp` - Test harness to decode the crafted PNG and trigger the bug
- `overflow_demo.cpp` - Standalone demo showing the integer overflow without needing a PNG

## Usage

### 1. Generate the exploit PNG
```bash
python3 generate_exploit_png.py exploit.png
```

### 2. Compile and run with AddressSanitizer
```bash
g++ -fsanitize=address -g -O1 ../lodepng.cpp test_exploit.cpp -o test_exploit
./test_exploit exploit.png
```

ASan will report a heap-buffer-overflow in `postProcessScanlines` or `Adam7_deinterlace`.

### 3. Standalone overflow demo (no PNG needed)
```bash
g++ -o overflow_demo overflow_demo.cpp
./overflow_demo
```

## Fix

Cast `passh[i]` and `passw[i]` to `size_t` before multiplication in `Adam7_getpassvalues()`:

```c
filter_passstart[i + 1] = filter_passstart[i]
    + ((passw[i] && passh[i]) ? (size_t)passh[i] * (1u + (passw[i] * bpp + 7u) / 8u) : 0);
padded_passstart[i + 1] = padded_passstart[i] + (size_t)passh[i] * ((passw[i] * bpp + 7u) / 8u);
passstart[i + 1] = passstart[i] + ((size_t)passh[i] * passw[i] * bpp + 7u) / 8u;
```

Note: `filter_passstart`, `padded_passstart`, and `passstart` are already `size_t` arrays -
only the right-hand side arithmetic needs the cast.
