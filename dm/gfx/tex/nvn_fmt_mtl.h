// NVNformat → Metal pixel format + component remapping. Pure C, no dependency on Metal headers (values taken from the SDK's MTLPixelFormat.h / MTLTexture.h, iOS 26.5 SDK).
#pragma once
#include <stdint.h>

// MTLTextureSwizzle values
enum { NVN_SWZ_ZERO = 0, NVN_SWZ_ONE = 1, NVN_SWZ_R = 2, NVN_SWZ_G = 3, NVN_SWZ_B = 4, NVN_SWZ_A = 5 };

// Returns: 1 = exact match (data uploaded as is);
//          2 = substitute (precision / bit order / read mode differs: upload needs conversion or the shader must read differently; per-format handling is in the table comments in nvn_fmt_mtl.c);
//          0 = no Metal counterpart (*mtl is set to 0; handling is in the table comments, mainly expanding 3-component formats to 4 components on upload).
// swz gives, for each of the four output components r/g/b/a, which source component to take (MTLTextureSwizzleChannels order); may be NULL.
int nvn_fmt_to_mtl(int nvn_format, uint32_t *mtl_pixel_format, uint8_t swz[4]);

// BC → ASTC 4x4 transcode mode (see dm/gfx/tex/README.md). Off by default: BC formats then map to Metal's BC formats as before.
// When on, the BC1 / BC2 / BC3 / BC7 formats (NVN 0x41–0x48, 0x4D, 0x4E) and unsigned BC4 / BC5 (0x49, 0x4B) map to ASTC_4x4_LDR (sRGB: ASTC_4x4_sRGB) and nvn_fmt_to_mtl returns 2 for them (the upload decodes and re-encodes);
// RGB_DXT1 (0x41, 0x45) keeps its "alpha reads as 1" remapping. Signed BC4 / BC5 (0x4A, 0x4C) and BC6H are not covered yet and keep mapping to the BC formats.
// Set once at start, before any texture is created. Thread safe.
void nvn_fmt_bc_to_astc(int on);
int nvn_fmt_bc_to_astc_on(void);

// For an NVN format the transcode covers (BC1 / BC2 / BC3 / BC7 any sRGB-ness, unsigned BC4 / BC5): the bcn_decode.h format (BCN_BC1 ...) and, through *srgb (may be NULL), whether it is an sRGB format. 0 for any other format.
// Independent of the mode above.
int nvn_fmt_bcn(int nvn_format, int *srgb);

// Whether the GPU reads BC textures (MTLDevice.supportsBCTextureCompression). On by default. Set it to 0 on a device without BC: every BC format the transcode above does not take over
// (BC6H always, the rest when the transcode is off) then returns 0 from nvn_fmt_to_mtl, which the callers handle as "no Metal equivalent" (texture refused, red line) rather than creating a BC texture Metal aborts on.
// Set once at start, before any texture is created. Thread safe.
void nvn_fmt_bc_native(int has_bc);
int nvn_fmt_bc_native_on(void);
