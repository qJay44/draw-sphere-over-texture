// https://stackoverflow.com/a/29681646/17694832

#include <cstdio>
#include <cstdlib>
#include <windows.h>

#include "generator.hpp"
#include "utils/clrp.hpp"

#define WIDTH 10u
#define HEIGHT 10u

void GLAPIENTRY MessageCallback(
  GLenum source,
  GLenum type,
  GLuint id,
  GLenum severity,
  GLsizei length,
  const GLchar* message,
  const void* userParam
) {
  clrp::clrp_t clrpError;
  clrpError.attr = clrp::ATTRIBUTE::BOLD;
  clrpError.fg = clrp::FG::RED;
  fprintf(
    stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
    (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, clrp::format(message, clrpError).c_str()
  );
  exit(1);
}


int main() {
  // Assuming the executable is launching from its own directory
  SetCurrentDirectory("../../../src");

  // GLFW init
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, TRUE);

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
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(MessageCallback, 0);

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

  // generator::genCubemapOnPlane("wem2560.png")
  generator::genCubemap("distanceFieldWater21600_0.tif", "distanceFieldWater21600_1.tif", GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT);

  glfwTerminate();
  puts("Done");

  return 0;
}

