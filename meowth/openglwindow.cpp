#include "openglwindow.hpp"

#include <fmt/core.h>
#include <imgui.h>
#include <tiny_obj_loader.h>

#include <cppitertools/itertools.hpp>
#include <glm/gtx/fast_trigonometry.hpp>
#include <glm/gtx/hash.hpp>
#include <unordered_map>

// Explicit specialization of std::hash for Vertex
namespace std {
template <>
struct hash<Vertex> {
  size_t operator()(Vertex const& vertex) const noexcept {
    const std::size_t h1{std::hash<glm::vec3>()(vertex.position)};
    return h1;
  }
};
}  // namespace std

void OpenGLWindow::handleEvent(SDL_Event& ev) {
  if (ev.type == SDL_KEYDOWN) {
    if (ev.key.keysym.sym == SDLK_UP || ev.key.keysym.sym == SDLK_w)
      m_dollySpeed = 1.0f;
    if (ev.key.keysym.sym == SDLK_DOWN || ev.key.keysym.sym == SDLK_s)
      m_dollySpeed = -1.0f;
    if (ev.key.keysym.sym == SDLK_LEFT || ev.key.keysym.sym == SDLK_a)
      m_panSpeed = -1.0f;
    if (ev.key.keysym.sym == SDLK_RIGHT || ev.key.keysym.sym == SDLK_d)
      m_panSpeed = 1.0f;
    if (ev.key.keysym.sym == SDLK_q) m_truckSpeed = -1.0f;
    if (ev.key.keysym.sym == SDLK_e) m_truckSpeed = 1.0f;
    if (ev.key.keysym.sym == SDLK_SPACE) {
      m_angulo = 0.0f;    
    }
  }
  if (ev.type == SDL_KEYUP) {
    if ((ev.key.keysym.sym == SDLK_UP || ev.key.keysym.sym == SDLK_w) &&
        m_dollySpeed > 0)
      m_dollySpeed = 0.0f;
    if ((ev.key.keysym.sym == SDLK_DOWN || ev.key.keysym.sym == SDLK_s) &&
        m_dollySpeed < 0)
      m_dollySpeed = 0.0f;
    if ((ev.key.keysym.sym == SDLK_LEFT || ev.key.keysym.sym == SDLK_a) &&
        m_panSpeed < 0)
      m_panSpeed = 0.0f;
    if ((ev.key.keysym.sym == SDLK_RIGHT || ev.key.keysym.sym == SDLK_d) &&
        m_panSpeed > 0)
      m_panSpeed = 0.0f;
    if (ev.key.keysym.sym == SDLK_q && m_truckSpeed < 0) m_truckSpeed = 0.0f;
    if (ev.key.keysym.sym == SDLK_e && m_truckSpeed > 0) m_truckSpeed = 0.0f;
  }
  if (ev.type == SDL_MOUSEBUTTONDOWN) {
    if (ev.button.button == SDL_BUTTON_LEFT){
      m_angulo = 1.0f;
    }
  }
  if (ev.type == SDL_MOUSEBUTTONUP) {
    if (ev.button.button == SDL_BUTTON_LEFT){
      m_angulo = 0.5f;    }
  }
  if (ev.type == SDL_MOUSEBUTTONDOWN) {
    if (ev.button.button == SDL_BUTTON_RIGHT){
      m_angulo = -1.0f;
    }
  }
  if (ev.type == SDL_MOUSEBUTTONUP) {
    if (ev.button.button == SDL_BUTTON_RIGHT){
      m_angulo = 0.5f;    }
  }
}

void OpenGLWindow::initializeGL() {
  abcg::glClearColor(0, 0, 0, 1);

  // Enable depth buffering
  abcg::glEnable(GL_DEPTH_TEST);

  // Create program
  m_program = createProgramFromFile(getAssetsPath() + "lookat.vert",
                                    getAssetsPath() + "lookat.frag");

  m_ground.initializeGL(m_program);

  // Load model
  loadModelFromFile(getAssetsPath() + "meowth.obj");

  // Generate VBO
  abcg::glGenBuffers(1, &m_VBO);
  abcg::glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
  abcg::glBufferData(GL_ARRAY_BUFFER, sizeof(m_vertices[0]) * m_vertices.size(),
                     m_vertices.data(), GL_STATIC_DRAW);
  abcg::glBindBuffer(GL_ARRAY_BUFFER, 0);

  // Generate EBO
  abcg::glGenBuffers(1, &m_EBO);
  abcg::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
  abcg::glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     sizeof(m_indices[0]) * m_indices.size(), m_indices.data(),
                     GL_STATIC_DRAW);
  abcg::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  // Create VAO
  abcg::glGenVertexArrays(1, &m_VAO);

  // Bind vertex attributes to current VAO
  abcg::glBindVertexArray(m_VAO);

  abcg::glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
  const GLint positionAttribute{
      abcg::glGetAttribLocation(m_program, "inPosition")};
  abcg::glEnableVertexAttribArray(positionAttribute);
  abcg::glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE,
                              sizeof(Vertex), nullptr);
  abcg::glBindBuffer(GL_ARRAY_BUFFER, 0);

  abcg::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);

  // End of binding to current VAO
  abcg::glBindVertexArray(0);

  resizeGL(getWindowSettings().width, getWindowSettings().height);
}

void OpenGLWindow::loadModelFromFile(std::string_view path) {
  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile(path.data())) {
    if (!reader.Error().empty()) {
      throw abcg::Exception{abcg::Exception::Runtime(
          fmt::format("Failed to load model {} ({})", path, reader.Error()))};
    }
    throw abcg::Exception{
        abcg::Exception::Runtime(fmt::format("Failed to load model {}", path))};
  }

  if (!reader.Warning().empty()) {
    fmt::print("Warning: {}\n", reader.Warning());
  }

  const auto& attrib{reader.GetAttrib()};
  const auto& shapes{reader.GetShapes()};

  m_vertices.clear();
  m_indices.clear();

  // A key:value map with key=Vertex and value=index
  std::unordered_map<Vertex, GLuint> hash{};

  // Loop over shapes
  for (const auto& shape : shapes) {
    // Loop over indices
    for (const auto offset : iter::range(shape.mesh.indices.size())) {
      // Access to vertex
      const tinyobj::index_t index{shape.mesh.indices.at(offset)};

      // Vertex position
      const int startIndex{3 * index.vertex_index};
      const float vx{attrib.vertices.at(startIndex + 0)};
      const float vy{attrib.vertices.at(startIndex + 1)};
      const float vz{attrib.vertices.at(startIndex + 2)};

      Vertex vertex{};
      vertex.position = {vx, vy, vz};

      // If hash doesn't contain this vertex
      if (hash.count(vertex) == 0) {
        // Add this index (size of m_vertices)
        hash[vertex] = m_vertices.size();
        // Add this vertex
        m_vertices.push_back(vertex);
      }

      m_indices.push_back(hash[vertex]);
    }
  }
}

void OpenGLWindow::paintGL() {
  update();

  // Clear color buffer and depth buffer
  abcg::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  abcg::glViewport(0, 0, m_viewportWidth, m_viewportHeight);

  abcg::glUseProgram(m_program);

  // Get location of uniform variables (could be precomputed)
  const GLint viewMatrixLoc{
      abcg::glGetUniformLocation(m_program, "viewMatrix")};
  const GLint projMatrixLoc{
      abcg::glGetUniformLocation(m_program, "projMatrix")};
  const GLint modelMatrixLoc{
      abcg::glGetUniformLocation(m_program, "modelMatrix")};
  const GLint colorLoc{abcg::glGetUniformLocation(m_program, "color")};

  // Set uniform variables for viewMatrix and projMatrix
  // These matrices are used for every scene object
  abcg::glUniformMatrix4fv(viewMatrixLoc, 1, GL_FALSE,
                           &m_camera.m_viewMatrix[0][0]);
  abcg::glUniformMatrix4fv(projMatrixLoc, 1, GL_FALSE,
                           &m_camera.m_projMatrix[0][0]);

  abcg::glBindVertexArray(m_VAO);

  // Draw white meowth
  glm::mat4 model{1.0f};
  model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
  model = glm::rotate(model, glm::radians(90.0f), glm::vec3(2, 0, 0));
  model = glm::scale(model, glm::vec3(0.5f));

  abcg::glUniformMatrix4fv(modelMatrixLoc, 1, GL_FALSE, &model[0][0]);
  //drawing the eyes
  abcg::glUniform4f(colorLoc, 0.94f, 0.71f, 0.427f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 580, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //drawing the body
  abcg::glUniform4f(colorLoc, 0.94f, 0.71f, 0.427f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 6398, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //drawing the curly tail and foot
  abcg::glUniform4f(colorLoc, 0.705f, 0.325f, 0.035f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 10000, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //continuing the head
  abcg::glUniform4f(colorLoc, 0.94f, 0.71f, 0.427f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 15200, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //MEDALHÃO DA TESTA
  abcg::glUniform4f(colorLoc, 0.925f, 0.592f, 0.188f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 15900, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //nails
  abcg::glUniform4f(colorLoc, 1.0, 1.0f, 1.0f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 16800, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //Creating the mask in around the eyes
  abcg::glUniform4f(colorLoc, 0.94f, 0.71f, 0.427f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 16920, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //some fix in the mask
  abcg::glUniform4f(colorLoc, 1.0, 1.0f, 1.0f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 16930, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //some fix in the mask
  abcg::glUniform4f(colorLoc, 0.94f, 0.71f, 0.427f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 16935, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //started building left eye
  abcg::glUniform4f(colorLoc, 1.0, 1.0f, 1.0f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 16945, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //some fix in the mask
  abcg::glUniform4f(colorLoc, 0.94f, 0.71f, 0.427f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 16950, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //continuing to build left eye
  abcg::glUniform4f(colorLoc, 1.0, 1.0f, 1.0f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 17000, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //keep on with the face mask
  abcg::glUniform4f(colorLoc, 0.94f, 0.71f, 0.427f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 17210, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //starting the right eye
  abcg::glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 17218, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //fixing some misplaced triangles issues
  abcg::glUniform4f(colorLoc, 0.94f, 0.71f, 0.427f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 17225, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //fixing some misplaced triangles issues
  abcg::glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 17233, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //fixing some misplaced triangles issues
  abcg::glUniform4f(colorLoc, 0.94f, 0.71f, 0.427f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 17240, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  //finishing the right eye
  abcg::glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
  abcg::glDrawElements(GL_TRIANGLES, 17288, GL_UNSIGNED_INT, nullptr); //pintando do vertice zero (nullptr), até o vertice index_

  abcg::glBindVertexArray(0);

  // Draw ground
  m_ground.paintGL();

  abcg::glUseProgram(0);
}

void OpenGLWindow::paintUI() { abcg::OpenGLWindow::paintUI(); }

void OpenGLWindow::resizeGL(int width, int height) {
  m_viewportWidth = width;
  m_viewportHeight = height;

  m_camera.computeProjectionMatrix(width, height);
}

void OpenGLWindow::terminateGL() {
  m_ground.terminateGL();

  abcg::glDeleteProgram(m_program);
  abcg::glDeleteBuffers(1, &m_EBO);
  abcg::glDeleteBuffers(1, &m_VBO);
  abcg::glDeleteVertexArrays(1, &m_VAO);
}

void OpenGLWindow::update() {
  const float deltaTime{static_cast<float>(getDeltaTime())};

  // Update LookAt camera
  m_camera.angulo = m_angulo; 
  m_camera.dolly(m_dollySpeed * deltaTime);
  m_camera.truck(m_truckSpeed * deltaTime);
  m_camera.pan(m_panSpeed * deltaTime);
}