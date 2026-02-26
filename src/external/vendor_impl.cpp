// // --- STB ---
// #define STB_IMAGE_IMPLEMENTATION
// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #include <stb_image.h>
// #include <stb_image_write.h>
//
// // --- TinyOBJ ---
// #define TINYOBJLOADER_IMPLEMENTATION
// #include <tiny_obj_loader.h>
//
// // --- TinyGLTF ---
// // Tell TinyGLTF to use the STB implementations we already defined above
// #define TINYGLTF_IMPLEMENTATION
// #define TINYGLTF_NO_STB_IMAGE
// #define TINYGLTF_NO_STB_IMAGE_WRITE
// #include <tiny_gltf.h>

/**
 * VENDOR IMPLEMENTATION FILE
 * * This file is the ONLY place in the entire project where the IMPLEMENTATION
 * macros should be defined. This compiles the actual logic of the libraries
 * into vendor_impl.cpp.o.
 */

/**
 * VENDOR IMPLEMENTATION FILE
 */

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION

// We define these so TinyGLTF doesn't try to include stb again internally
// which causes the macro errors you saw earlier
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include <stb_image.h>
#include <stb_image_write.h>
#include <tiny_obj_loader.h>
#include <tiny_gltf.h>