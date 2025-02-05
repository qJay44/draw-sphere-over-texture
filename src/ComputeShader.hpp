#pragma once

class Shader {
public:
  Shader(const fspath& vert, const fspath& frag, const fspath& geom = "");
  Shader(const fspath& comp);

  void use() const;

  void setUniform3f(const std::string& name, const vec3& v) const;
  void setUniformTexture(const std::string& name, const GLuint& unit) const;

private:
  GLuint program;
};
