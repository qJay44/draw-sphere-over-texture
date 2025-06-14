// https://stackoverflow.com/a/29681646/17694832

#include <cstdio>
#include <cstdlib>
#include <format>

#include "ComputeShader.hpp"
#include "utils.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "tiffio.h"

#define WIDTH 10u
#define HEIGHT 10u

s16* loadTif_R16I(
  const char* path,
  u32& width,
  u32& height,
  u16& channels,
  u16& depth,
  u16& sampleFormat
);

u16* loadTif_R16UI(
  const char* path,
  u32& width,
  u32& height,
  u16& channels,
  u16& depth,
  u16& sampleFormat
);

void saveTif_R16I(
  const char* path,
  const u32& width,
  const u32& height,
  const u16& channels,
  const u16& depth,
  const u16& sampleFormat,
  const s16* pixels
);

void saveTif_R16UI(
  const char* path,
  const u32& width,
  const u32& height,
  const u16& channels,
  const u16& depth,
  const u16& sampleFormat,
  const u16* pixels
);


int main() {
  // Assuming the executable is launching from its own directory
  SetCurrentDirectory("../../../src");

  // GLFW init
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // Window init
  GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "", NULL, NULL);
  if (!window) {
    printf("Failed to create GFLW window\n");
    glfwTerminate();
    return EXIT_FAILURE;
  }
  glfwMakeContextCurrent(window);

  // GLAD init
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    printf("Failed to initialize GLAD\n");
    return EXIT_FAILURE;
  }

  glViewport(0, 0, WIDTH, HEIGHT);

  GLint wgCountX;
  GLint wgCountY;
  GLint wgCountZ;
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &wgCountX);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &wgCountY);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &wgCountZ);
  printf("Work group count: [x: %d, y: %d, z: %d]\n", wgCountX, wgCountY, wgCountZ);

  GLint lgCountX;
  GLint lgCountY;
  GLint lgCountZ;
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &lgCountX);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &lgCountY);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &lgCountZ);
  printf("Work group local count: [x: %d, y: %d, z: %d]\n", lgCountX, lgCountY, lgCountZ);

  const vec3 directions[6]{
    {0.f,  1.f,  0.f }, // Up
    {0.f,  -1.f, 0.f }, // Down
    {-1.f, 0.f,  0.f }, // Left
    {1.f,  0.f,  0.f }, // Right
    {0.f,  0.f,  1.f }, // Front
    {0.f,  0.f,  -1.f}, // Rear
  };
  const vec3 palette[6]{
    {0.f,    0.992f, 1.f   },
    {1.f,    0.149f, 0.f   },
    {1.f,    0.251f, 0.988f},
    {0.f,    0.976f, 0.173f},
    {0.024f, 0.204f, 0.988f},
    {0.996f, 0.984f, 0.169f},
  };

  Shader mainShader("main.comp");
  Shader hcubemapShader("hcubemap_w2_r32f.comp");
  Shader vcubemapShader("vcubemap_w2_r32f.comp");

  stbi_set_flip_vertically_on_load(true);
  stbi_flip_vertically_on_write(true);

  bool computeMain = false;
  bool computeCubemap = true;

  u32 texWidth, texHeight;
  u16 channels, tifDepth, tifFormat;

  if (computeMain) {
    int w, h, c;
    byte* pixelsInput = stbi_load("wem2560.png", &w, &h, &c, 0);
    texWidth = static_cast<u32>(w);
    texHeight = static_cast<u32>(h);
    channels = static_cast<u16>(c);

    u32 tex;
    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, texWidth, texHeight, 0, GL_RED, GL_UNSIGNED_BYTE, pixelsInput);
    mainShader.setUniformTexture("diffuse0", 1);

    u32 textureOutput;
    glGenTextures(1, &textureOutput);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureOutput);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texWidth, texHeight, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindImageTexture(0, textureOutput, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

    byte* pixelsOutput = new byte[texWidth * texHeight * 4];
    for (u32 i = 0; i < 6; i++) {
      mainShader.setUniform3f("normal", directions[i]);
      mainShader.setUniform3f("color", palette[i]);
      glDispatchCompute(texWidth, texHeight, 1);
      glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsOutput);
    }
    stbi_write_png("result.png", texWidth, texHeight, 4, pixelsOutput, texWidth * 4);
    stbi_image_free(pixelsInput);
  }

  if (computeCubemap) {
    #define DO_TIF
    // #define DO_COLORED

    #ifdef DO_TIF
      const char* westFile = "distanceFieldWater21600_0_50.tif";
      const char* eastFile = "distanceFieldWater21600_1_50.tif";
      const fspath folderName = R"(faces\distanceFieldWater21600_50)";
      const bool isTif = true;
      const GLenum pixelsDataType = GL_UNSIGNED_SHORT;
      const GLenum readFormat = GL_RED_INTEGER;
      const GLint internalFormat = GL_R16UI;
      const GLint imageFormat = GL_R16UI;
      hcubemapShader = Shader("hcubemap_w2_r16ui.comp");
      vcubemapShader = Shader("vcubemap_w2_r16ui.comp");
    #else
      const bool isTif = false;
      const GLenum pixelsDataType = GL_UNSIGNED_BYTE;

      #ifdef DO_COLORED
        const char* westFile = "worldColors21600_0.png";
        const char* eastFile = "worldColors21600_1.png";
        const fspath folderName = R"(faces\worldColors21600)";
        const GLenum readFormat = GL_RGB;
        const GLenum internalFormat = GL_RGB;
        const GLint imageFormat = GL_RGBA32F;
        hcubemapShader = Shader("hcubemap_w2_rgba32f.comp");
        vcubemapShader = Shader("vcubemap_w2_rgba32f.comp");
      #else
        const char* westFile = "distanceFieldWater21600_0.png";
        const char* eastFile = "distanceFieldWater21600_1.png";
        const fspath folderName = R"(faces\distanceFieldWater21600)";
        const GLenum readFormat = GL_RED;
        const GLenum internalFormat = GL_R32F;
        const GLint imageFormat = GL_R32F;
      #endif
    #endif

    void* pixelsDiffuse0 = nullptr;
    void* pixelsDiffuse1 = nullptr;

    if (isTif) {
      switch (internalFormat) {
        case GL_R16I:
          pixelsDiffuse0 = (void*)loadTif_R16I(westFile, texWidth, texHeight, channels, tifDepth, tifFormat);
          pixelsDiffuse1 = (void*)loadTif_R16I(eastFile, texWidth, texHeight, channels, tifDepth, tifFormat);
          break;
        case GL_R16UI:
          pixelsDiffuse0 = (void*)loadTif_R16UI(westFile, texWidth, texHeight, channels, tifDepth, tifFormat);
          pixelsDiffuse1 = (void*)loadTif_R16UI(eastFile, texWidth, texHeight, channels, tifDepth, tifFormat);
          break;
        default:
          puts("Unhandled internalFormat");
          exit(1);
      }
    } else {
      int w, h, c;

      pixelsDiffuse0 = (void*)stbi_load(westFile, &w, &h, &c, 0);
      pixelsDiffuse1 = (void*)stbi_load(eastFile, &w, &h, &c, 0);

      texWidth = static_cast<u32>(w);
      texHeight = static_cast<u32>(h);
      channels = static_cast<u16>(c);

      if (pixelsDiffuse0 == nullptr) {
        printf("stbi can't open [%s]\n", westFile);
        exit(1);
      }

      if (pixelsDiffuse1 == nullptr) {
        printf("stbi can't open [%s]\n", eastFile);
        exit(1);
      }
    }

    u32 diffuse0;
    glGenTextures(1, &diffuse0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, diffuse0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, texWidth, texHeight, 0, readFormat, pixelsDataType, pixelsDiffuse0);

    u32 diffuse1;
    glGenTextures(1, &diffuse1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, diffuse1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, texWidth, texHeight, 0, readFormat, pixelsDataType, pixelsDiffuse1);

    uvec2 faceSize(texWidth / 2);
    void* pixels = isTif ? (void*)(new s16[faceSize.x * faceSize.y * channels]) : (void*)(new byte[faceSize.x * faceSize.y * channels]);
    u32 face;
    glGenTextures(1, &face);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, face);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, imageFormat, faceSize.x, faceSize.y, 0, readFormat, pixelsDataType, NULL);
    glBindImageTexture(0, face, 0, GL_FALSE, 0, GL_WRITE_ONLY, imageFormat);

    system(std::format("mkdir {}", folderName.string()).c_str());

    hcubemapShader.setUniformTexture("diffuse0", 1);
    hcubemapShader.setUniformTexture("diffuse1", 2);

    const char* orderh[4] = {"front", "right", "back", "left"};
    const char* orderv[3] = {"bottom", "front", "top"};

    clrp::clrp_t clrp;
    clrp.fg = clrp::FG::LIGHT_BLUE;
    std::string fmt = clrp::prepare(clrp);
    clrp.fg = clrp::FG::MAGENTA;
    std::string msg;
    puts("");

    // vertical
    stbi_flip_vertically_on_write(false);
    for (u8 i = 0; i < 3; i++) {
      clearLine();
      std::string msg = std::format("\rConverting to vertical face: {}", clrp::format(orderv[i], clrp));
      printf(fmt.c_str(), msg.c_str());
      vcubemapShader.setUniform2ui("offset", {0u, faceSize.x * i});
      vcubemapShader.setUniform1ui("face", i);
      glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
      glDispatchCompute(faceSize.x, faceSize.y, 1);
      glGetTexImage(GL_TEXTURE_2D, 0, readFormat, pixelsDataType, pixels);
      std::string filePathNoExt = (folderName / orderv[i]).string();

      if (isTif) {
        filePathNoExt += ".tif";
        saveTif_R16I(filePathNoExt.c_str(), faceSize.x, faceSize.y, channels, tifDepth, tifFormat, (s16*)pixels);
      } else {
        filePathNoExt += ".png";
        stbi_write_png(filePathNoExt.c_str(), faceSize.x, faceSize.y, channels, pixels, channels * faceSize.x);
      }
    }
    clearLine();
    clrp.fg = clrp::FG::LIGHT_GREEN;
    msg = std::format("\rConverting to vertical face: {}", clrp::format("Done", clrp));
    printf(fmt.c_str(), msg.c_str());
    puts("");
    clrp.fg = clrp::FG::MAGENTA;

    // horizontal
    stbi_flip_vertically_on_write(true);
    for (u8 i = 0; i < 4; i++) {
      clearLine();
      msg = std::format("\rConverting to horizontal face: {}", clrp::format(orderh[i], clrp));
      printf(fmt.c_str(), msg.c_str());
      hcubemapShader.setUniform2ui("offset", {faceSize.x * i, 0u});
      hcubemapShader.setUniform1ui("face", i);
      glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
      glDispatchCompute(faceSize.x, faceSize.y, 1);
      glGetTexImage(GL_TEXTURE_2D, 0, readFormat, pixelsDataType, pixels);
      std::string filePathNoExt = (folderName / orderh[i]).string();

      if (isTif) {
        filePathNoExt += ".tif";
        saveTif_R16I(filePathNoExt.c_str(), faceSize.x, faceSize.y, channels, tifDepth, tifFormat, (s16*)pixels);
      } else {
        filePathNoExt += ".png";
        stbi_write_png(filePathNoExt.c_str(), faceSize.x, faceSize.y, channels, pixels, channels * faceSize.x);
      }
    }
    clearLine();
    clrp.fg = clrp::FG::LIGHT_GREEN;
    msg = std::format("\rConverting to horizontal face: {}", clrp::format("Done\0", clrp));
    printf(fmt.c_str(), msg.c_str());
    puts("");

    if (isTif) {
      switch (internalFormat) {
        case GL_R16I:
          delete[] (s16*)pixelsDiffuse0;
          delete[] (s16*)pixelsDiffuse1;
          delete[] (s16*)pixels;
          break;
        case GL_R16UI:
          delete[] (u16*)pixelsDiffuse0;
          delete[] (u16*)pixelsDiffuse1;
          delete[] (u16*)pixels;
          break;
        default:
          puts("Unhandled internalFormat");
          exit(1);
      }
    } else {
      stbi_image_free(pixelsDiffuse0);
      stbi_image_free(pixelsDiffuse1);
      delete[] (byte*)pixels;
    }
  }

  glfwTerminate();
  puts("Done");

  return 0;
}

s16* loadTif_R16I(
  const char* path,
  u32& width,
  u32& height,
  u16& channels,
  u16& depth,
  u16& sampleFormat
) {
  TIFF* tif = TIFFOpen(path, "r");
  if (!tif) {
    printf("tif can't open file [%s]\n", path);
    exit(1);
  }

  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
  TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &channels);
  TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &depth);
  TIFFGetField(tif, TIFFTAG_SAMPLEFORMAT, &sampleFormat);

  s16* buf = new s16[width * height];

  for (u32 row = 0; row < height; row++) {
    s16* bufRow = buf + row * width;

    if (TIFFReadScanline(tif, bufRow, row) < 0) {
      puts("tif scanline read error");
      exit(1);
    }
  }

  // for (u32 y = 0; y < height / 2; ++y) {
  //   short* rowTop = buf + y * width;
  //   short* rowBottom = buf + (height - y - 1) * width;
  //   std::swap_ranges(rowTop, rowTop + width, rowBottom);
  // }

  return buf;

  TIFFClose(tif);
}

u16* loadTif_R16UI(
  const char* path,
  u32& width,
  u32& height,
  u16& channels,
  u16& depth,
  u16& sampleFormat
) {
  TIFF* tif = TIFFOpen(path, "r");
  if (!tif) {
    printf("tif can't open file [%s]\n", path);
    exit(1);
  }

  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
  TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &channels);
  TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &depth);
  TIFFGetField(tif, TIFFTAG_SAMPLEFORMAT, &sampleFormat);

  u16* buf = new u16[width * height];

  for (u32 row = 0; row < height; row++) {
    u16* bufRow = buf + row * width;

    if (TIFFReadScanline(tif, bufRow, row) < 0) {
      puts("tif scanline read error");
      exit(1);
    }
  }

  // for (u32 y = 0; y < height / 2; ++y) {
  //   short* rowTop = buf + y * width;
  //   short* rowBottom = buf + (height - y - 1) * width;
  //   std::swap_ranges(rowTop, rowTop + width, rowBottom);
  // }

  return buf;

  TIFFClose(tif);
}

void saveTif_R16I(
  const char* path,
  const u32& width,
  const u32& height,
  const u16& channels,
  const u16& depth,
  const u16& sampleFormat,
  const s16* pixels
) {
  TIFF* tif = TIFFOpen(path, "w");
  if (!tif) {
    printf("tif can't open file [%s]\n", path);
    exit(1);
  }

  TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
  TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
  TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, channels);
  TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, depth);
  TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, sampleFormat);
  TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);

  for (u32 row = 0; row < height; row++) {
    const s16* rowPtr = pixels + row * width;

    if (TIFFWriteScanline(tif, (void*)rowPtr, row) < 0) {
      printf("Failed to write scanline, row: [%i]\n", row);
      TIFFClose(tif);
      exit(1);
    }
  }

  TIFFClose(tif);
}

void saveTif_R16UI(
  const char* path,
  const u32& width,
  const u32& height,
  const u16& channels,
  const u16& depth,
  const u16& sampleFormat,
  const u16* pixels
) {
  TIFF* tif = TIFFOpen(path, "w");
  if (!tif) {
    printf("tif can't open file [%s]\n", path);
    exit(1);
  }

  TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
  TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
  TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, channels);
  TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, depth);
  TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, sampleFormat);
  TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);

  for (u32 row = 0; row < height; row++) {
    const u16* rowPtr = pixels + row * width;

    if (TIFFWriteScanline(tif, (void*)rowPtr, row) < 0) {
      printf("Failed to write scanline, row: [%i]\n", row);
      TIFFClose(tif);
      exit(1);
    }
  }

  TIFFClose(tif);
}


