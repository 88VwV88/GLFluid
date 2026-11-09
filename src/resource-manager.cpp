#include "resource-manager.hpp"
#include <filesystem>
#include <fstream>

namespace gl {

auto ResourceManager::createBuffer(std::size_t size) -> GLuint {
  GLuint buffer;
  glCreateBuffers(1, &buffer);
  glNamedBufferStorage(buffer, size, nullptr, GL_DYNAMIC_STORAGE_BIT);
  return mBuffers.emplace_back(buffer);
}

auto ResourceManager::createVertexArray() -> GLuint {
  GLuint array;
  glCreateVertexArrays(1, &array);
  return mVertexArrays.emplace_back(array);
}

auto ResourceManager::createVertexBuffer(std::span<GLfloat const> data)
    -> GLuint {
  auto const &buffer = createBuffer(data.size_bytes());
  glNamedBufferSubData(buffer, 0, data.size_bytes(), data.data());
  return buffer;
}

auto ResourceManager::createElementBuffer(std::span<GLuint const> data)
    -> GLuint {
  auto const &buffer = createBuffer(data.size_bytes());
  glNamedBufferSubData(buffer, 0, data.size_bytes(), data.data());
  return buffer;
}

auto ResourceManager::createTexture2D(Texture2DCreateInfo const &info)
    -> GLuint {
  GLuint texture;
  glCreateTextures(GL_TEXTURE_2D, 1, &texture);
  glTextureStorage2D(texture, 1, info.internalFormat, info.width, info.height);
  if (info.image.has_value()) {
    glTextureSubImage2D(texture, 0, 0, 0, info.width, info.height,
                        info.imageFormat.value_or(GL_RGBA), GL_UNSIGNED_BYTE,
                        info.image->data());
  }
  return mTextures.emplace_back(texture);
}

static inline auto checkFilePath(std::filesystem::path const &path) -> void {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("File not found: " + path.string());
  }
  if (!std::filesystem::is_regular_file(path)) {
    throw std::runtime_error("Not a regular file: " + path.string());
  }
}
static inline auto readFileContents(std::filesystem::path const &path)
    -> std::string {
  checkFilePath(path);
  std::ifstream file{path, std::ios::binary | std::ios::ate};
  if (!file) {
    throw std::runtime_error("Failed to open file: " + path.string());
  }

  auto const size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::string contents(size, '\0');
  if (!file.read(contents.data(), size)) {
    throw std::runtime_error("Failed to read file: " + path.string());
  }
  return contents;
}
auto checkShaderCompileStatus(GLuint shader) -> void {
  GLint param;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &param);
  if (param == GL_TRUE) {
    return;
  }
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &param);
  std::string log(param, '\0');
  glGetShaderInfoLog(shader, param, nullptr, log.data());
  throw std::runtime_error("Shader compilation failed: " + log);
}
auto checkProgramLinkStatus(GLuint program) -> void {
  GLint param;
  glGetProgramiv(program, GL_LINK_STATUS, &param);
  if (param == GL_TRUE) {
    return;
  }
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &param);
  std::string log(param, '\0');
  glGetProgramInfoLog(program, param, nullptr, log.data());
  throw std::runtime_error("Program linking failed: " + log);
}
auto ResourceManager::createShaderProgram(const ShaderFiles &files) -> GLuint {
  GLuint vertexShader{0};
  GLuint fragmentShader{0};
  GLuint geometryShader{0};
  try {
    auto const vertexSource = readFileContents(files.vertex);
    auto const vertexShaderSource = vertexSource.c_str();
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    checkShaderCompileStatus(vertexShader);

    auto const fragmentSource = readFileContents(files.fragment);
    auto const fragmentShaderSource = fragmentSource.c_str();
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    checkShaderCompileStatus(fragmentShader);

    auto const program = glCreateProgram();
    glAttachShader(program, fragmentShader);
    glAttachShader(program, vertexShader);

    if (files.geometry.has_value()) {
      auto const geometrySource = readFileContents(files.geometry.value());
      auto const geometryShaderSource = geometrySource.c_str();
      geometryShader = glCreateShader(GL_GEOMETRY_SHADER);
      glShaderSource(geometryShader, 1, &geometryShaderSource, nullptr);
      glCompileShader(geometryShader);
      checkShaderCompileStatus(geometryShader);
      glAttachShader(program, geometryShader);
    }
    glLinkProgram(program);
    checkProgramLinkStatus(program);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    if (geometryShader != 0) {
      glDeleteShader(geometryShader);
    }

    return program;
  } catch (std::exception const &error) {
    if (vertexShader != 0) {
      glDeleteShader(vertexShader);
    }
    if (fragmentShader != 0) {
      glDeleteShader(fragmentShader);
    }
    if (geometryShader != 0) {
      glDeleteShader(geometryShader);
    }

    throw std::runtime_error("Failed to compile vertex shader: " +
                             std::string(error.what()));
  }
}

auto ResourceManager::createComputeProgram(
    std::filesystem::path const &computeShaderPath) -> GLuint {
  GLuint computeShader{0};
  try {
    auto const computeSource = readFileContents(computeShaderPath);
    auto const computeShaderSource = computeSource.c_str();
    computeShader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(computeShader, 1, &computeShaderSource, nullptr);
    glCompileShader(computeShader);
    checkShaderCompileStatus(computeShader);

    auto const program = glCreateProgram();
    glAttachShader(program, computeShader);
    glLinkProgram(program);
    checkProgramLinkStatus(program);

    glDeleteShader(computeShader);
    return program;
  } catch (std::exception const &error) {
    if (computeShader != 0) {
      glDeleteShader(computeShader);
    }

    throw std::runtime_error("Failed to compile compute shader: " +
                             std::string(error.what()));
  }
}

ResourceManager::~ResourceManager() noexcept {
  glDeleteTextures(mTextures.size(), mTextures.data());
  glDeleteBuffers(mBuffers.size(), mBuffers.data());
  glDeleteVertexArrays(mVertexArrays.size(), mVertexArrays.data());
  for (auto const &program : mPrograms) {
    glDeleteProgram(program);
  }
}
}; // namespace gl