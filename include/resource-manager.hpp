#ifndef FLUID_RESOURCE_MANAGER
#define FLUID_RESOURCE_MANAGER

#include <GL/glew.h>
#include <filesystem>
#include <span>
#include <vector>

namespace gl {

struct Texture2DCreateInfo {
  GLint width;
  GLint height;
  GLenum internalFormat;
  std::optional<GLenum> imageFormat;
  std::optional<std::span<GLubyte const>> image;
};

struct ShaderFiles {
  std::filesystem::path vertex;
  std::filesystem::path fragment;
  std::optional<std::filesystem::path> geometry;
};

struct ResourceManager {
  auto createVertexArray() -> GLuint;
  auto createBuffer(std::size_t size) -> GLuint;
  auto createVertexBuffer(std::span<GLfloat const> data) -> GLuint;
  auto createElementBuffer(std::span<GLuint const> data) -> GLuint;
  auto createTexture2D(Texture2DCreateInfo const &info) -> GLuint;
  auto createShaderProgram(ShaderFiles const &files) -> GLuint;
  auto createComputeProgram(std::filesystem::path const &computeShaderPath)
      -> GLuint;

  ~ResourceManager() noexcept;

private:
  std::vector<GLuint> mBuffers;
  std::vector<GLuint> mVertexArrays;
  std::vector<GLuint> mTextures;
  std::vector<GLuint> mPrograms;
};

}; // namespace gl

#endif