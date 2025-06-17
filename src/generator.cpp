#include "generator.hpp"
#include <format>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <map>

#include "ComputeShader.hpp"
#include "utils/utils.hpp"
#include "utils/status.hpp"
#include "utils/loadTif.hpp"
#include "utils/saveTif.hpp"

static const std::map<std::string, u8> extensionsMap{
  {".jpeg", 0},
  {".jpg" , 1},
  {".png" , 2},
  {".tga" , 3},
  {".bmp" , 4},
  {".psd" , 5},
  {".gif" , 6},
  {".hdr" , 7},
  {".pic" , 8},
  {".tif" , 9},
};

namespace generator {

void genCubemap(
  const fspath& path0,
  const fspath& path1,
  const GLenum& internalFormat,
  const GLenum& readFormat,
  const GLenum& readDataType
) {
  const std::string path0String = path0.string();
  const std::string path1String = path1.string();
  const char* path0Cstr = path0String.c_str();
  const char* path1Cstr = path1String.c_str();

  const std::string ext0 = path0.extension().string();
  const std::string ext1 = path1.extension().string();

  if (ext0 != ext1)
    error("[genCubemap] The files extension are not the same [{}] and [{}]\n", ext0, ext1);

  const auto& it = extensionsMap.find(ext0);
  if (it == extensionsMap.end())
    error("[genCubemap] Unsupported extension [{}]", ext0);

  const bool isTif = it->second == 9;

  Shader hcubemapShader, vcubemapShader;
  switch (internalFormat) {
    case GL_R8:
      hcubemapShader = Shader("hcubemap_w2_r8.comp");
      vcubemapShader = Shader("vcubemap_w2_r8.comp");
      break;
    case GL_R16I:
      hcubemapShader = Shader("hcubemap_w2_r16i.comp");
      vcubemapShader = Shader("vcubemap_w2_r16i.comp");
      break;
    case GL_R16UI:
      hcubemapShader = Shader("hcubemap_w2_r16ui.comp");
      vcubemapShader = Shader("vcubemap_w2_r16ui.comp");
      break;
    case GL_RGBA8:
      hcubemapShader = Shader("hcubemap_w2_rgba8.comp");
      vcubemapShader = Shader("vcubemap_w2_rgba8.comp");
      break;
    default:
      error("[genCubemap] Unexpected internalFormat [{}]", internalFormat);
  }
  hcubemapShader.setUniformTexture("diffuse0", 1);
  hcubemapShader.setUniformTexture("diffuse1", 2);

  void* pixelsDiffuse0 = nullptr;
  void* pixelsDiffuse1 = nullptr;

  uvec2 texSize;
  u16 channels = 0;
  u16 tifDepth = 0;
  u16 tifFormat = 0;

  if (isTif) {
    switch (internalFormat) {
      case GL_R16I:
        pixelsDiffuse0 = loadTif_R16I(path0Cstr, &texSize.x, &texSize.y, true);
        pixelsDiffuse1 = loadTif_R16I(path1Cstr, &texSize.x, &texSize.y, true);
        channels = 1;
        tifDepth = 16;
        tifFormat = SAMPLEFORMAT_INT;
        break;
      case GL_R16UI:
        pixelsDiffuse0 = loadTif_R16UI(path0Cstr, &texSize.x, &texSize.y, true);
        pixelsDiffuse1 = loadTif_R16UI(path1Cstr, &texSize.x, &texSize.y, true);
        channels = 1;
        tifDepth = 16;
        tifFormat = SAMPLEFORMAT_UINT;
        break;
      default:
        error("[genCubemap] Unexpected internalFormat");
    }
  } else {
    int w, h, c;

    pixelsDiffuse0 = stbi_load(path0Cstr, &w, &h, &c, 0);
    pixelsDiffuse1 = stbi_load(path1Cstr, &w, &h, &c, 0);

    texSize = {w, h};
    channels = c;

    if (pixelsDiffuse0 == nullptr) error("[genCubemap] stbi can't open [{}]", path0Cstr);
    if (pixelsDiffuse1 == nullptr) error("[genCubemap] stbi can't open [{}]", path1Cstr);
  }

  GLuint diffuse0;
  glGenTextures(1, &diffuse0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, diffuse0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, texSize.x, texSize.y, 0, readFormat, readDataType, pixelsDiffuse0);

  GLuint diffuse1;
  glGenTextures(1, &diffuse1);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, diffuse1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, texSize.x, texSize.y, 0, readFormat, readDataType, pixelsDiffuse1);

  uvec2 faceSize(texSize / 2u);
  size_t bufCount = faceSize.x * faceSize.y * channels;
  void* outputPixels = nullptr;
  switch (internalFormat) {
    case GL_R8:
      outputPixels = new u8[bufCount];
      break;
    case GL_R16I:
      outputPixels = new s16[bufCount];
      break;
    case GL_R16UI:
      outputPixels = new u16[bufCount];
      break;
    case GL_RGBA8:
      outputPixels = new u32[bufCount];
      break;
    default:
      error("[genCubemap][this didn't suppose to happen] Unexpected internalFormat [{}]", internalFormat);
  }

  GLuint face;
  glGenTextures(1, &face);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, face);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, faceSize.x, faceSize.y, 0, readFormat, readDataType, NULL);
  glBindImageTexture(0, face, 0, GL_FALSE, 0, GL_WRITE_ONLY, internalFormat);

  static constexpr std::string orderv[3] = {"bottom", "front", "top"};
  static constexpr std::string orderh[4] = {"front", "right", "back", "left"};

  std::string fileName = path0.stem().string();
  fileName.erase(fileName.length() - 2); // Remove split number
  const fspath folderName = "faces" / fspath(fileName);
  system(std::format("mkdir {}", folderName.string()).c_str());

  // vertical
  status::start("Converting to vertical face:  ", orderv[0]);
  for (GLuint i = 0; i < 3; i++) {
    status::update(orderv[i]);
    vcubemapShader.setUniform2ui("offset", {0u, faceSize.x * i});
    vcubemapShader.setUniform1ui("face", i);
    glDispatchCompute(faceSize.x, faceSize.y, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glGetTexImage(GL_TEXTURE_2D, 0, readFormat, readDataType, outputPixels);

    if (isTif) {
      bool flipVertically = orderv[i] == "bottom" || orderv[i] == "top";
      std::string filePath = ((folderName / orderv[i]).string() + path0.extension().string());
      saveTif(filePath.c_str(), faceSize.x, faceSize.y, channels, tifDepth, tifFormat, outputPixels, flipVertically);
    } else {
      std::string filePath = ((folderName / orderv[i]).string() + path0.extension().string());
      if (!stbi_write_png(filePath.c_str(), faceSize.x, faceSize.y, channels, outputPixels, channels * faceSize.x))
        error("[genCubemap] stbi write returned 0 [{}]", filePath.c_str());
    }
  }
  status::end(true);

  // horizontal
  status::start("Converting to horizontal face:", orderh[0]);
  for (GLuint i = 0; i < 4; i++) {
    status::update(orderh[i]);
    hcubemapShader.setUniform2ui("offset", {faceSize.x * i, 0u});
    hcubemapShader.setUniform1ui("face", i);
    glDispatchCompute(faceSize.x, faceSize.y, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glGetTexImage(GL_TEXTURE_2D, 0, readFormat, readDataType, outputPixels);
    std::string filePath = ((folderName / orderh[i]).string() + path0.extension().string());

    if (isTif)
      saveTif(filePath.c_str(), faceSize.x, faceSize.y, channels, tifDepth, tifFormat, outputPixels);
    else
      if (!stbi_write_png(filePath.c_str(), faceSize.x, faceSize.y, channels, outputPixels, channels * faceSize.x))
        error("[genCubemap] stbi write returned 0 [{}]", filePath.c_str());
  }
  status::end(true);

  if (isTif) {
    switch (internalFormat) {
      case GL_R8:
        delete[] (u8*)pixelsDiffuse0;
        delete[] (u8*)pixelsDiffuse1;
        delete[] (u8*)outputPixels;
        break;
      case GL_R16I:
        delete[] (s16*)pixelsDiffuse0;
        delete[] (s16*)pixelsDiffuse1;
        delete[] (s16*)outputPixels;
        break;
      case GL_R16UI:
        delete[] (u16*)pixelsDiffuse0;
        delete[] (u16*)pixelsDiffuse1;
        delete[] (u16*)outputPixels;
        break;
      case GL_RGBA8:
        delete[] (u32*)pixelsDiffuse0;
        delete[] (u32*)pixelsDiffuse1;
        delete[] (u32*)outputPixels;
        break;
      default:
        error("[genCubemap][this didn't suppose to happen] Unexpected internalFormat [{}]", internalFormat);
    }
  } else {
    stbi_image_free(pixelsDiffuse0);
    stbi_image_free(pixelsDiffuse1);
    delete[] (u8*)outputPixels;
  }
}

void genCubemapOnPlane(const fspath& path) {
  constexpr vec3 directions[6]{
    {0.f,  1.f,  0.f }, // Up
    {0.f,  -1.f, 0.f }, // Down
    {-1.f, 0.f,  0.f }, // Left
    {1.f,  0.f,  0.f }, // Right
    {0.f,  0.f,  1.f }, // Front
    {0.f,  0.f,  -1.f}, // Rear
  };
  constexpr vec3 palette[6]{
    {0.f,    0.992f, 1.f   },
    {1.f,    0.149f, 0.f   },
    {1.f,    0.251f, 0.988f},
    {0.f,    0.976f, 0.173f},
    {0.024f, 0.204f, 0.988f},
    {0.996f, 0.984f, 0.169f},
  };

  Shader mainShader("main.comp");

  stbi_set_flip_vertically_on_load(true);
  stbi_flip_vertically_on_write(true);

  int w, h, c;
  byte* pixelsInput = stbi_load(path.string().c_str(), &w, &h, &c, 0);

  GLuint tex;
  glGenTextures(1, &tex);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, pixelsInput);

  stbi_image_free(pixelsInput);
  mainShader.setUniformTexture("diffuse0", 1);

  GLuint textureOutput;
  glGenTextures(1, &textureOutput);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textureOutput);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
  glBindImageTexture(0, textureOutput, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);

  byte* pixelsOutput = new byte[w * h * 4];
  for (size_t i = 0; i < 6; i++) {
    const vec3& n = directions[i];
    status::start("Drawing normal", std::format("{{{:>2}, {:>2}, {:>2}}}", n.x, n.y, n.z));
    mainShader.setUniform3f("normal", n);
    mainShader.setUniform3f("color", palette[i]);
    glDispatchCompute(w, h, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsOutput);
    status::end(true);
  }

  status::start("Saving to", "cubemap_on_plane.png");
  stbi_write_png("cubemap_on_plane.png", w, h, 4, pixelsOutput, w * 4);
  delete[] pixelsOutput;
  status::end(true);
}

} // generator

