#pragma once

#include "core/stdafx.h"

using namespace DirectX;

struct ViewProjectionConstantBuffer {
  XMFLOAT4X4 view;
  XMFLOAT4X4 projection;
  float padding[32];  // 256 bytes alignment
};

struct PrefilterConstantBuffer {
  float roughness;
  float padding[63];  // 256 bytes alignment
};

struct SceneConstantBuffer {
  XMFLOAT4X4 model;
  XMFLOAT4X4 view;
  XMFLOAT4X4 projection;
  XMFLOAT4 camPos;
};

struct LightState {
  LightState() = default;
  LightState(float posX, float posY, float posZ,
    float colorR, float colorG, float colorB) {
    position[0] = posX, position[1] = posY, position[2] = posZ;
    color[0] = colorR, color[1] = colorG, color[2] = colorB;
  }

  float position[4]{};
  float color[4]{};
};
static constexpr UINT8 kNumLights = 4;
struct LightStatesConstantBuffer {
  LightState lights[kNumLights];
};

class Model {
public:
  struct Vertex {
    Vertex() = default;
    Vertex(float posX, float posY, float posZ,
      float normalX, float normalY, float normalZ,
      float u, float v) {
      position[0] = posX; position[1] = posY; position[2] = posZ;
      normal[0] = normalX; normal[1] = normalY; normal[2] = normalZ;
      uv[0] = u; uv[1] = v;
    }

    float position[3]{};
    float normal[3]{};
    float uv[2]{};
  };  // struct Vertex

  static size_t GetVertexStride() {
    return sizeof(Vertex);
  }

  virtual ~Model() {

  }

  virtual std::unique_ptr<Vertex[]> GetVertexData() const = 0;

  virtual size_t GetVertexDataSize() const = 0;

  virtual size_t GetVertexNumber() const = 0;

  virtual std::unique_ptr<DWORD[]> GetIndexData() const = 0;

  virtual size_t GetIndexDataSize() const = 0;

  virtual size_t GetIndexNumber() const = 0;

  virtual const std::string GetTextureImageFileName() const = 0;

private:

};  // class Model

class CubeModel {
public:
  std::unique_ptr<Model::Vertex[]> GetVertexData() const {
    std::unique_ptr<Model::Vertex[]> vertices_ptr = std::make_unique<Model::Vertex[]>(36);

    // right hand
    
    float vertices[] = {
      // back face
      -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
       1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
       1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
       1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
      -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
      -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
      // front face
      -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
       1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
       1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
       1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
      -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
      -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
      // left face
      -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
      -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
      -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
      -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
      -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
      -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
      // right face
       1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
       1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
       1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
       1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
       1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
       1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
      // bottom face
      -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
       1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
       1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
       1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
      -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
      -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
      // top face
      -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
       1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
       1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
       1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
      -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
      -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
    };

    std::memcpy(vertices_ptr.get(), vertices, sizeof(vertices));

    return vertices_ptr;
  }

  size_t GetVertexDataSize() const {
    return Model::GetVertexStride() * 36;
  }
};

class SphereModel {
public:
  SphereModel(UINT x_segments, UINT y_segments) 
    : X_SEGMENTS(x_segments), Y_SEGMENTS(y_segments) {

  }

  std::unique_ptr<Model::Vertex[]> GetVertexData() {
    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT2> uv;
    std::vector<XMFLOAT3> normals;
    for (UINT x = 0; x <= X_SEGMENTS; ++x) {
      for (UINT y = 0; y <= Y_SEGMENTS; ++y) {
        float xSegment = (float)x / (float)X_SEGMENTS;
        float ySegment = (float)y / (float)Y_SEGMENTS;
        float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
        float yPos = std::cos(ySegment * PI);
        float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

        positions.push_back(XMFLOAT3(xPos, yPos, zPos));
        uv.push_back(XMFLOAT2(xSegment, ySegment));
        normals.push_back(XMFLOAT3(xPos, yPos, zPos));
      }
    }

    const size_t vertexCount = positions.size();
    std::unique_ptr<Model::Vertex[]> vertices_ptr = std::make_unique<Model::Vertex[]>(vertexCount);

    for (size_t i = 0; i < vertexCount; ++i) {
      vertices_ptr[i] = Model::Vertex(positions[i].x, positions[i].y, positions[i].z,
        normals[i].x, normals[i].y, normals[i].z,
        uv[i].x, uv[i].y);
    }

    m_vertexCount = vertexCount;

    return vertices_ptr;
  }

  std::unique_ptr<DWORD[]> GetIndexData() {
    std::vector<DWORD> indices;

    bool oddRow = false;
    for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
    {
      if (!oddRow) // even rows: y == 0, y == 2; and so on
      {
        for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
        {
          indices.push_back(y * (X_SEGMENTS + 1) + x);
          indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
        }
      }
      else
      {
        for (int x = X_SEGMENTS; x >= 0; --x)
        {
          indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
          indices.push_back(y * (X_SEGMENTS + 1) + x);
        }
      }
      oddRow = !oddRow;
    }

    size_t indexCount = indices.size();
    m_indexCount = indexCount;
    std::unique_ptr<DWORD[]> indices_ptr = std::make_unique<DWORD[]>(indexCount);
    memcpy(indices_ptr.get(), indices.data(), indexCount * sizeof(DWORD));

    return indices_ptr;
  }

  size_t GetVertexDataSize() const {
    return Model::GetVertexStride() * m_vertexCount;
  }

  size_t GetIndexDataSize() const {
    return sizeof(DWORD) * m_indexCount;
  }

private:
  static constexpr float PI = 3.14159265359f;

  const UINT X_SEGMENTS = 0;
  const UINT Y_SEGMENTS = 0;

  size_t m_vertexCount = 0;
  size_t m_indexCount = 0;
};