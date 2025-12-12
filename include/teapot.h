#pragma once

#include "shape.h"

namespace Boidsish {
class Teapot : public Shape {
 public:
  Teapot(int id, float x, float y, float z);
  void render() const override;

 private:
  void create_buffers() const;

  mutable bool initialized_ = false;
  mutable unsigned int vao_ = 0;
  mutable unsigned int vbo_ = 0;
  mutable int num_vertices_ = 0;
};
}
