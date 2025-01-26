#pragma once

class Shader {
public:
  Shader(const fspath& vert, const fspath& frag, const fspath& geom = "");
  Shader(const fspath& comp);
  void use() const;

private:
  GLuint program;
};
