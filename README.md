# VGrender

VGrender is an experimental DOS graphics demo written in C and 16-bit x86 assembly. It switches to VGA mode `0x12`, reads a BMP file through DOS file services, converts its pixels to the mode's 16-color palette, and plots them through BIOS video services. The repository also contains experimental helpers for drawing pixels, axis-aligned lines, rectangle outlines, and palette entries.

The project is primarily useful for learning about real-mode DOS, BIOS interrupts, C/assembly interfaces, and the constraints of a small graphics program. It is not a portable desktop graphics library.

## Current demo

The entry point is `main.c`. When `GUI.COM` runs, it:

1. Initializes a fixed-size heap.
2. Switches to 640×480, 16-color VGA mode `0x12`.
3. Opens `output.bmp` from the current DOS directory.
4. Prints image metadata and draws the image with its top-left corner at `(50, 50)`.

The filename is currently hardcoded. The loader appends `.bmp`, so its API receives a base name such as `"output"`. There are no command-line options or interactive file picker.

## Capabilities

- Plot pixels using BIOS video services.
- Draw horizontal and vertical lines and rectangle outlines.
- Read BMP files with DOS file open, seek, and buffered read calls.
- Decode uncompressed indexed and true-color BMP pixels, including row padding and top-down images.
- Convert source colors to the 16 colors available in VGA mode `0x12`.
- Use a small fixed-size heap and minimal string and printing helpers.

## BMP compatibility

The reader supports Windows BMP files with a DIB header of at least 40 bytes and uncompressed (`BI_RGB`) pixel data at these depths:

| Bits per pixel | Interpretation |
| --- | --- |
| 1, 4, 8 | Indexed pixels using the BMP color table |
| 16 | Uncompressed RGB 5:5:5 |
| 24 | BGR pixels |
| 32 | BGR pixels; the fourth byte is ignored |

BMP row padding and both bottom-up and top-down row order are handled. Decoded colors are mapped to the nearest color in the 16-color VGA palette, so photos and gradients lose detail. Images are not scaled; pixels outside the 640×480 screen are clipped. The reader rejects compressed BMPs, bitfield-compressed images, OS/2 core headers, unsupported bit depths, and invalid or oversized dimensions.

## Requirements

To rebuild the DOS executable, use a Linux-like environment with:

- GNU Make
- GCC with `-m16` support
- GNU binutils with `elf_i386` support
- NASM

To run it, use a DOS environment or emulator with BIOS video support, such as DOSBox or DOSBox-X. It is not a native Linux or Windows console program.

## Build

From the repository root:

```sh
make
```

The Makefile compiles the C modules, assembles `xgfx.asm`, and links the result into `gui.com`. It then removes intermediate object files. The main build workflow is Make; `Cargo.toml` does not provide a Rust implementation or Cargo build.

## Run in DOSBox

Mount the project directory and launch the executable:

```text
mount c /home/ihitplayzyt/VGrender
c:
gui.com
```

The demo looks for `output.bmp` in the mounted directory. The current `output.bmp` is a 50×50, 24-bit BMP. To use another image without changing source, replace `output.bmp` with it or rename it to `output.bmp`. To use another filename or change the draw position, edit the `parse_bmp` call in `main.c`, rebuild with `make`, and run the new `gui.com`.

https://github.com/user-attachments/assets/8d27ecab-cdd6-4642-b7f8-e6ea5bbf8024

## API examples

The shape interfaces are declared in `include/shapes.h`. For example, the intended flow for drawing a horizontal line is:

```c
init_heap();
videomode(x640_480_16);

Point *start = init_point(20, 30, 4);
Point *end = init_point(120, 30, 4);
Line *line = init_line(start, end, 4, 1);
draw_line(line);

dealloc_all();
```

The BMP API is declared in `include/bmp.h`:

```c
Bitmap *image = parse_bmp((i8 *)"output", 50, 50); /* opens output.bmp */
if (image) {
    bmp_show(image);
    draw_bmp(image);
}
```

## Strengths and limitations

### Strengths

- The DOS and BIOS operations are visible in the source rather than hidden behind a large framework.
- The program has a small runtime and demonstrates C calling assembly routines directly.
- BMP pixel data is streamed through a small buffer instead of loading the entire image into memory.

### Limitations

- The program depends on 16-bit x86 real-mode DOS and BIOS services.
- BMP support is limited to the formats listed above; RLE and bitfield compression are not implemented.
- Every visible pixel is plotted through a BIOS interrupt, so large images can render slowly.
- The 16-color output substantially reduces image detail.
- `draw_line` supports only horizontal and vertical lines. `draw_rectangle` draws an outline; solid fill is not implemented.
- The custom bump allocator has no per-object free operation. `dealloc_all()` resets the entire heap.
- Error handling and the custom formatted-print helper are intentionally minimal.

## Source layout

| Path | Purpose |
| --- | --- |
| `main.c` | Demo entry point, heap, DOS file helpers, and basic runtime utilities |
| `bmp.c`, `include/bmp.h` | BMP validation, palette handling, pixel decoding, and drawing |
| `shapes.c`, `include/shapes.h` | Pixel, line, rectangle, and palette helpers |
| `xgfx.asm`, `include/assembly.h`, `include/asm/xgfx.h` | BIOS/DOS interrupt wrappers and fixed heap storage |
| `omnistd.c`, `include/omnistd.h` | Minimal memory, string, and formatted-output helpers |
| `gui.ld` | Linker script for the DOS binary |
| `Makefile` | Build recipe for `gui.com` |
| `GUI_IMAGES/`, `*.bmp` | Development screenshots and image assets |

## License

VGrender is distributed under the GNU General Public License v3.0 only. See [`LICENSE`](LICENSE).
