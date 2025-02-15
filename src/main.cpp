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

#define WIDTH 1200u
#define HEIGHT 720u

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

  const char* dirNames[6]{"up", "down", "left", "right", "front", "rear"};
  const vec3 directions[6]{
    {0.f,  1.f,  0.f }, // Up
    {0.f,  -1.f, 0.f }, // Down
    {-1.f, 0.f,  0.f }, // Left
    {1.f,  0.f,  0.f }, // Right
    {0.f,  0.f,  1.f }, // Front
    {0.f,  0.f,  -1.f}, // Rear
  };
  const vec3 palette[6]{
    {0.004f, 0.745f, 0.996f},
    {1.f,    0.867f, 0.f   },
    {1.f,    0.49f,  0.f   },
    {1.f,    0.f,    0.427f},
    {0.678f, 1.f,    0.008f},
    {0.561f, 0.f,    1.f   },
  };

  Shader mainShader("main.comp");
  Shader hcubemapShader("hcubemap_w2.comp");
  Shader vcubemapShader("vcubemap_w2.comp");

  stbi_set_flip_vertically_on_load(true);
  stbi_flip_vertically_on_write(true);

  bool computeCubemap = true;

  int texWidth, texHeight, channels;
  byte* pixelsInput = stbi_load("wem2560.png", &texWidth, &texHeight, &channels, 0);
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

  if (computeCubemap) {
    // NOTE: These ones have 4 channels but whole (original) image had 1 channel,
    // so its posiible to use GL_RED for textures
    byte* pixelsDiffuse0 = stbi_load("wem21600_1.png", &texWidth, &texHeight, &channels, 0);
    byte* pixelsDiffuse1 = stbi_load("wem21600_2.png", &texWidth, &texHeight, &channels, 0);

    u32 diffuse0;
    glGenTextures(1, &diffuse0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, diffuse0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsDiffuse0);

    u32 diffuse1;
    glGenTextures(1, &diffuse1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, diffuse1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsDiffuse1);

    uvec2 faceSize(texWidth / 2);
    byte* pixels = new byte[faceSize.x * faceSize.y];
    u32 face;
    glGenTextures(1, &face);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, face);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, faceSize.x, faceSize.y, 0, GL_RED, GL_FLOAT, NULL);
    glBindImageTexture(0, face, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);

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
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
      stbi_write_png(std::format("{}.png", orderv[i]).c_str(), faceSize.x, faceSize.y, 1, pixels, faceSize.x);
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
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
      stbi_write_png(std::format("{}.png", orderh[i]).c_str(), faceSize.x, faceSize.y, 1, pixels, faceSize.x);
    }
    clearLine();
    clrp.fg = clrp::FG::LIGHT_GREEN;
    msg = std::format("\rConverting to horizontal face: {}", clrp::format("Done\0", clrp));
    printf(fmt.c_str(), msg.c_str());
    puts("");

    stbi_image_free(pixelsDiffuse0);
    stbi_image_free(pixelsDiffuse1);
    delete[] pixels;
  }

  glfwTerminate();
  puts("Done");

  return 0;
}
