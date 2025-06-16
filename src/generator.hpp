#pragma once

#include "utils/types.hpp"

namespace generator {

void genCubemap(
  const fspath& path0,
  const fspath& path1,
  const GLenum& internalFormat,
  const GLenum& readFormat,
  const GLenum& readDataType
);

void genCubemapOnPlane(const fspath& path);

} // generator

