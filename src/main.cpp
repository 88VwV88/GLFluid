#include "resource-manager.hpp"
#include "utils.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <stdexcept>

static constexpr std::array<GLfloat, 4 * 4> VERTICES{
    -1.0f, -1.0f, 0.0f, 0.0f, //
    -1.0f, +1.0f, 0.0f, 1.0f, //
    +1.0f, +1.0f, 1.0f, 1.0f, //
    +1.0f, -1.0f, 1.0f, 0.0f, //
};
static constexpr std::array<GLuint, 6> INDICES{
    0, 1, 2, //
    0, 2, 3, //
};

static constexpr auto WIDTH = 1280;
static constexpr auto HEIGHT = 720;

struct VertexAttribInfo {
  GLuint index;
  GLint size;
  GLuint offset;
};
static inline auto setupVertexArrayAttrib(GLuint array,
                                          VertexAttribInfo const &info)
    -> void {
  glVertexArrayAttribFormat(array, info.index, info.size, GL_FLOAT, GL_FALSE,
                            info.offset);
  glVertexArrayAttribBinding(array, info.index, 0);
  glEnableVertexArrayAttrib(array, info.index);
}

/* Texture indices */
static constexpr std::size_t VELOCITY0 = 0;
static constexpr std::size_t VELOCITY1 = 1;
static constexpr std::size_t DENSITY0 = 2;
static constexpr std::size_t DENSITY1 = 3;
static constexpr std::size_t PRESSURE0 = 4;
static constexpr std::size_t PRESSURE1 = 5;
static constexpr std::size_t DIVERGENCE = 6;

struct Fluid {
  std::vector<GLuint> textures;
  struct {
    GLuint advectVelocity;
    GLuint advectDensity;
    GLuint divergence;
    GLuint jacobi;
    GLuint project;
    GLuint splat;
  } programs;
};

struct UserInput {
  bool isMouseDown = false;
  glm::vec2 currentMousePos = glm::vec2{0.0f};
  glm::vec2 previousMousePos = glm::vec2{0.0f};
};

static inline auto getMousePosition(GLFWwindow *window) -> glm::vec2 {
  double x, y;
  glfwGetCursorPos(window, &x, &y);
  return {static_cast<float>(x),
          static_cast<float>(HEIGHT) - static_cast<float>(y) - 1.0f};
}

static inline auto mouseButtonCallback(GLFWwindow *window, int button,
                                       int action, int) -> void {
  auto &input =
      *reinterpret_cast<UserInput *>(glfwGetWindowUserPointer(window));
  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    if (action == GLFW_PRESS) {
      input.isMouseDown = true;
      input.currentMousePos = getMousePosition(window);
      input.previousMousePos = input.currentMousePos;
    } else if (action == GLFW_RELEASE) {
      input.isMouseDown = false;
    }
  }
}

static inline auto cursorPosCallback(GLFWwindow *window, double xpos,
                                     double ypos) -> void {
  auto &input =
      *reinterpret_cast<UserInput *>(glfwGetWindowUserPointer(window));
  if (input.isMouseDown) {
    input.currentMousePos = {static_cast<float>(xpos),
                             static_cast<float>(HEIGHT) -
                                 static_cast<float>(ypos) - 1.0f};
  }
}

static inline auto stepFluidSimulation(float dt, Fluid &fluid,
                                       UserInput &input) {
  auto &textures = fluid.textures;
  auto const &programs = fluid.programs;
  auto const barrier =
      GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT;
  auto const groupsX = (WIDTH + 15) / 16;
  auto const groupsY = (HEIGHT + 15) / 16;

  // 1. Advect Velocity (ping-pong vel: read from [0], write to [1])
  glUseProgram(programs.advectVelocity);
  glUniform1f(glGetUniformLocation(programs.advectVelocity, "dt"), dt);
  glUniform2f(glGetUniformLocation(programs.advectVelocity, "texelSize"),
              1.0f / WIDTH, 1.0f / HEIGHT);
  glBindTextureUnit(0, textures[VELOCITY0]);
  glBindTextureUnit(1, textures[VELOCITY0]);
  glBindImageTexture(2, textures[VELOCITY1], 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_RG32F);
  glDispatchCompute(groupsX, groupsY, 1);
  glMemoryBarrier(barrier);
  std::swap(textures[VELOCITY0], textures[VELOCITY1]); // Swap pointers

  // 2. Advect Density/Color (read density from [0], write to [1], using
  // updated velocity)
  glUseProgram(programs.advectDensity);
  glUniform1f(glGetUniformLocation(programs.advectDensity, "dt"), dt);
  glUniform2f(glGetUniformLocation(programs.advectDensity, "texelSize"),
              1.0f / WIDTH, 1.0f / HEIGHT);
  glBindTextureUnit(0, textures[DENSITY0]);
  glBindTextureUnit(1, textures[VELOCITY0]);
  glBindImageTexture(2, textures[DENSITY1], 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_RGBA16F);
  glDispatchCompute(groupsX, groupsY, 1);
  glMemoryBarrier(barrier);
  std::swap(textures[DENSITY0], textures[DENSITY1]);

  // Bind user input shaders to add force/density at mouse positions
  if (input.isMouseDown) {
    glUseProgram(programs.splat);
    static constexpr float SPEED = 0.1f;
    auto const mouseDir =
        dt > 0.0f
            ? (input.currentMousePos - input.previousMousePos) * (SPEED / dt)
            : glm::vec2{0.0f};
    // 2. Upload Uniform Inputs
    glUniform2f(glGetUniformLocation(programs.splat, "uMousePos"),
                input.currentMousePos.x, input.currentMousePos.y);
    glUniform2f(glGetUniformLocation(programs.splat, "uMouseDir"), mouseDir.x,
                mouseDir.y);
    glUniform1f(glGetUniformLocation(programs.splat, "uRadius"),
                12.0f); // Splat thickness radius
    glUniform4f(glGetUniformLocation(programs.splat, "uColor"), 0.0f, 0.7f,
                1.0f, 1.0f); // Bright blue dye
    glUniform1f(glGetUniformLocation(programs.splat, "uDensityAmt"),
                0.5f); // Injected density volume per frame

    // 3. Bind Textures as Images with READ_WRITE access
    glBindImageTexture(0, textures[DENSITY0], 0, GL_FALSE, 0, GL_READ_WRITE,
                       GL_RGBA16F);
    glBindImageTexture(1, textures[VELOCITY0], 0, GL_FALSE, 0, GL_READ_WRITE,
                       GL_RG32F);

    // 4. Dispatch and block memory access
    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(barrier);
    input.previousMousePos = input.currentMousePos;
  }

  // 3. Calculate Velocity Divergence
  glUseProgram(programs.divergence);
  glBindTextureUnit(0, textures[VELOCITY0]);
  glBindImageTexture(1, textures[DIVERGENCE], 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_R32F);
  glDispatchCompute(groupsX, groupsY, 1);
  glMemoryBarrier(barrier);

  // 4. Solve Jacobi Pressure Loop
  glUseProgram(programs.jacobi);
  glBindTextureUnit(1, textures[DIVERGENCE]);
  for (int i = 0; i < 30; i++) {
    glBindTextureUnit(0, textures[PRESSURE0]); // Read
    glBindImageTexture(2, textures[PRESSURE1], 0, GL_FALSE, 0, GL_WRITE_ONLY,
                       GL_R32F); // Write
    glDispatchCompute(groupsX, groupsY, 1);
    glMemoryBarrier(barrier);
    std::swap(textures[PRESSURE0], textures[PRESSURE1]);
  }

  // 5. Project/Subtract Pressure Gradient
  glUseProgram(programs.project);
  glBindTextureUnit(0, textures[VELOCITY0]);
  glBindTextureUnit(1, textures[PRESSURE0]);
  glBindImageTexture(2, textures[VELOCITY1], 0, GL_FALSE, 0, GL_WRITE_ONLY,
                     GL_RG32F);
  glDispatchCompute(groupsX, groupsY, 1);
  glMemoryBarrier(barrier);
  std::swap(textures[VELOCITY0], textures[VELOCITY1]);
}

int main() {
  gl::ResourceManager resources;

  glfwSetErrorCallback([](int error, const char *description) -> void {
    std::cerr << "[ERROR] (" << error << ") " << description << '\n';
  });
  glfwInit();

  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  GLFWwindow *window =
      glfwCreateWindow(WIDTH, HEIGHT, "OpenGL Window", nullptr, nullptr);
  glfwMakeContextCurrent(window);
  glfwSwapInterval(0);

  GLenum error = glewInit();
  if (error != GLEW_OK and error != GLEW_ERROR_NO_GLX_DISPLAY) {
    throw std::runtime_error(
        std::string("Failed to initialize GLEW: ") +
        reinterpret_cast<char const *>(glewGetErrorString(error)));
  }

  std::cout << "OpenGL version: " << glGetString(GL_VERSION) << '\n';
  glViewport(0, 0, WIDTH, HEIGHT);

#ifndef NDEBUG
  gl::utils::setupOpenGLDebugContext();
#endif

  auto const &renderProgram =
      resources.createShaderProgram({.vertex = "shaders/default.vert",
                                     .fragment = "shaders/default.frag",
                                     .geometry = "shaders/default.geom"});
  auto const &vao = resources.createVertexArray();
  auto const &vbo = resources.createVertexBuffer(VERTICES);
  auto const &ebo = resources.createElementBuffer(INDICES);

  glVertexArrayVertexBuffer(vao, 0, vbo, 0, 4 * sizeof(GLfloat));
  glVertexArrayElementBuffer(vao, ebo);
  setupVertexArrayAttrib(vao, {0, 2, 0});
  setupVertexArrayAttrib(vao, {1, 2, 2 * sizeof(GLfloat)});

  Fluid fluid;

  // Textures to store fluid velocity, density, pressure states and divergence.
  fluid.textures = std::vector<GLuint>(7);
  std::vector<GLenum> fluidTextureFormats{
      GL_RG32F, GL_RG32F, GL_RGBA16F, GL_RGBA16F, GL_R32F, GL_R32F, GL_R32F};
  std::vector<GLenum> fluidTextureClearFormats{GL_RG,  GL_RG,  GL_RGBA, GL_RGBA,
                                               GL_RED, GL_RED, GL_RED};
  static constexpr std::array<GLfloat, 4> ZERO{};
  for (auto i = 0; i < fluid.textures.size(); ++i) {
    fluid.textures[i] = resources.createTexture2D({
        .width = WIDTH,
        .height = HEIGHT,
        .internalFormat = fluidTextureFormats[i],
    });
    glTextureParameteri(fluid.textures[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(fluid.textures[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(fluid.textures[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(fluid.textures[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glClearTexImage(fluid.textures[i], 0, fluidTextureClearFormats[i], GL_FLOAT,
                    ZERO.data());
  }

  // Compute shader programs to run the fluid simulation
  fluid.programs = {
      .advectVelocity =
          resources.createComputeProgram("shaders/fluid/advect_velocity.comp"),
      .advectDensity =
          resources.createComputeProgram("shaders/fluid/advect.comp"),
      .divergence =
          resources.createComputeProgram("shaders/fluid/divergence.comp"),
      .jacobi = resources.createComputeProgram("shaders/fluid/jacobi.comp"),
      .project = resources.createComputeProgram("shaders/fluid/project.comp"),
      .splat = resources.createComputeProgram("shaders/fluid/splat.comp"),
  };

  UserInput input;
  glfwSetWindowUserPointer(window, &input);
  glfwSetCursorPosCallback(window, cursorPosCallback);
  glfwSetMouseButtonCallback(window, mouseButtonCallback);

  double deltaTime = 0.0;
  double lastFrame = 0.0;
  int fCounter = 0;

  while (not glfwWindowShouldClose(window)) {
    auto const &currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    if (fCounter > 50) {
      glfwSetWindowTitle(
          window,
          std::format("OpenGL Window | {} FPS | Frametime: {:.3f}ms",
                      static_cast<int>(1.0 / deltaTime), deltaTime * 1000.0)
              .c_str());
      fCounter = 0;
    }
    fCounter++;
    stepFluidSimulation(static_cast<float>(deltaTime), fluid, input);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(renderProgram);
    glBindVertexArray(vao);

    glUniform2f(glGetUniformLocation(renderProgram, "uScale"), 1.0f, 1.0f);
    glUniform1i(glGetUniformLocation(renderProgram, "uQuadTexture"), 0);
    glBindTextureUnit(0, fluid.textures[DENSITY0]);

    glDrawElements(GL_TRIANGLES, INDICES.size(), GL_UNSIGNED_INT, nullptr);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
}