#include <cstdio>
#include <cstdlib>

#include "ComputeShader.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define WIDTH 1'200u
#define HEIGHT 720u
#define TEXTURE_WIDTH 2'560u
#define TEXTURE_HEIGHT 1'280

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
    {0.004f, 0.745f, 0.996f},
    {1.f,    0.867f, 0.f   },
    {1.f,    0.49f,  0.f   },
    {1.f,    0.f,    0.427f},
    {0.678f, 1.f,    0.008f},
    {0.561f, 0.f,    1.f   },
  };

  Shader mainShader("main.comp");
  Shader clearShader("clear.comp");

  u32 texture;
  glGenTextures(1, &texture);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
  glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

  byte* bytes = new byte[TEXTURE_WIDTH * TEXTURE_HEIGHT * 4];
  byte* bytesTemp = new byte[TEXTURE_WIDTH * TEXTURE_HEIGHT * 4];

  // Clear the output image
  clearShader.use();
  glDispatchCompute(TEXTURE_WIDTH, TEXTURE_HEIGHT, 1);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytesTemp);
  stbi_flip_vertically_on_write(true);
  stbi_write_png("result.png", TEXTURE_WIDTH, TEXTURE_HEIGHT, 4, bytesTemp, TEXTURE_WIDTH * 4);

  for (u32 i = 0; i < 6; i++) {
    mainShader.setUniform3f("normal", directions[i]);
    mainShader.setUniform3f("color", palette[i]);
    glDispatchCompute(TEXTURE_WIDTH, TEXTURE_HEIGHT, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytesTemp);
    for (u32 i = 0; i < TEXTURE_WIDTH * TEXTURE_HEIGHT * 4; i++)
      bytes[i] = max(bytes[i], bytesTemp[i]);
  }
  stbi_write_png("result.png", TEXTURE_WIDTH, TEXTURE_HEIGHT, 4, bytes, TEXTURE_WIDTH * 4);

  delete[] bytes;
  delete[] bytesTemp;
  glfwTerminate();
  puts("Done");

  return 0;
}
