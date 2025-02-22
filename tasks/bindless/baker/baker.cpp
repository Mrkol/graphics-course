#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include "tiny_gltf.h"

#include <iostream>

#include "baker.hpp"

namespace GLTF_type {
constexpr int  BYTE = 5120;
constexpr int  UBYTE = 5121;
constexpr int  SHORT = 5122;
constexpr int  USHORT = 5123;
constexpr int  UINT = 5125;
constexpr int  FLOAT = 5126;
}

namespace GLTF_buffer_view_target {
constexpr int ARRAY_BUFFER = 34962;
constexpr int ELEMENT_ARRAY_BUFFER = 34963;
}

struct BufferOffsetKey {
  int buffer;
  size_t byteOffset;

  bool operator <(const BufferOffsetKey& other) const {
    if (buffer < other.buffer) {
      return true;
    }
    return byteOffset < other.byteOffset;
  }
};


template<bool OverrideSilence = false, class... Ts>
void debugPrint([[maybe_unused]] Ts... args) {
  if constexpr (OverrideSilence || false) {
    (std::cout << ... << args);
  }
}

template<typename T>
T normalizeAsT(unsigned char* src, int componentType, int index) {
  // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#accessor-data-types
  src += index * tinygltf::GetComponentSizeInBytes(componentType);

  if (componentType == GLTF_type::BYTE) {
    return static_cast<T>(*reinterpret_cast<signed char*>(src));
  } else if (componentType == GLTF_type::UBYTE) {
    return static_cast<T>(*reinterpret_cast<unsigned char*>(src));
  } else if (componentType == GLTF_type::SHORT) {
    return static_cast<T>(*reinterpret_cast<signed short*>(src));
  } else if (componentType == GLTF_type::USHORT) {
    return static_cast<T>(*reinterpret_cast<unsigned short*>(src));
  } else if (componentType == GLTF_type::UINT) {
    return static_cast<T>(*reinterpret_cast<unsigned int*>(src));
  } else if (componentType == GLTF_type::FLOAT) {
    return static_cast<T>(*reinterpret_cast<float*>(src));
  }

  throw std::runtime_error("invalid component type");
}

std::vector<float> normalizePositionData(unsigned char* src, int componentType) {
  return std::vector<float>{normalizeAsT<float>(src, componentType, 0),
                            normalizeAsT<float>(src, componentType, 1),
                            normalizeAsT<float>(src, componentType, 2)};
}

std::vector<float> normalizeTexcoordData(unsigned char* src, int componentType) {
  return std::vector<float>{normalizeAsT<float>(src, componentType, 0),
                            normalizeAsT<float>(src, componentType, 1)};
}

signed char quantizeFloat(float coordinate) {
  return static_cast<signed char>(std::round(127.0f * (coordinate)));
}

std::vector<signed char> normalizeNormalOrTangentData(unsigned char* src, int componentType) {
  return std::vector<signed char>{quantizeFloat(normalizeAsT<float>(src, componentType, 0)),
                                  quantizeFloat(normalizeAsT<float>(src, componentType, 1)),
                                  quantizeFloat(normalizeAsT<float>(src, componentType, 2))};
}

uint32_t normalizeIndexData(unsigned char* src, int componentType) {
  return normalizeAsT<uint32_t>(src, componentType, 0);
}

void SetAccessorAndBufferViewValues(tinygltf::Accessor& accessor, tinygltf::Model& model, size_t newByteStride,
                                    size_t newByteOffset, int newComponentType, size_t elemSize, int newBuffer,
                                    bool normalized, int target = GLTF_buffer_view_target::ARRAY_BUFFER) {
  // add new buffer view to model
  tinygltf::BufferView newView;
  newView.buffer = newBuffer;
  newView.byteOffset = newByteOffset;
  size_t realStride = newByteStride > 0 ? newByteStride : elemSize;
  newView.byteLength = accessor.count > 0 ? accessor.count * realStride : elemSize;
  newView.byteStride = newByteStride;
  newView.target = target;
  model.bufferViews.push_back(newView);

  // set accessor's values
  accessor.bufferView = static_cast<int>(model.bufferViews.size()) - 1;
  accessor.byteOffset = 0;
  accessor.componentType = newComponentType;
  accessor.normalized = normalized;
  if (normalized) {
    for (auto& comp : accessor.minValues) {
      comp = std::round(comp * 127.0);
    }

    for (auto& comp : accessor.maxValues) {
      comp = std::round(comp * 127.0);
    }
  }
}

void printQuantizedVector(const std::vector<signed char>& v) {
  bool needComma = false;
  for (const auto& coord : v) {
    debugPrint((needComma ? ", " : " "), int(coord));
    needComma = true;
  }
}


void RemoveUnusedObjects(tinygltf::Model& model) {
  // I made a lot of copypasta here. I tried to generalize, but the general function looked horrendous:
  // lots of template parameters and std::functions everywhere. If you insist, I'll fix it.

  {
    // 1. remove unused accessors
    // 1.1. find used accessors
    std::vector<bool> used(model.accessors.size(), false);
    for (const auto& mesh : model.meshes) {
      for (const auto& prim : mesh.primitives) {
        for (const auto& attrib : prim.attributes) {
          used[attrib.second] = true;
          debugPrint("accessor ", attrib.second, " is used by attribute ", attrib.first, " of a primitive of a mesh\n");
        }

        if (prim.indices != -1) {
          used[prim.indices] = true;
          debugPrint("accessor ", prim.indices, " contains indices of a primitive of a mesh\n");
        }
      }
    }
    debugPrint("accessors ");
    for (size_t i = 0; i < used.size(); ++i) {
      if (used[i]) {
        debugPrint(i, " ");
      }
    }
    debugPrint("are used\n");
    // 1.2. move accessors to a new vector, and calculate new indices of accessors
    std::vector<size_t> newIndices(model.accessors.size());
    std::vector<tinygltf::Accessor> newAccessors;
    for (size_t i = 0; i < used.size(); ++i) {
      if (used[i]) {
        newAccessors.push_back(model.accessors[i]);
        newIndices[i] = newAccessors.size() - 1;
        debugPrint("accessor ", i, " gets new index: ", newIndices[i], "\n");
      }
    }
    // 1.3. change indices in attributes
    debugPrint("taking accessors ");
    for (auto& mesh : model.meshes) {
      for (auto& prim : mesh.primitives) {
        std::vector<std::pair<std::string, int>> oldAttributes;
        for (const auto& attrib : prim.attributes) {
          oldAttributes.push_back(attrib);
        }

        for (auto& attrib : oldAttributes) {
          if (used[attrib.second]) {
            prim.attributes[attrib.first] = static_cast<int>(newIndices[static_cast<size_t>(attrib.second)]);
            debugPrint(attrib.second, " (becomes ", static_cast<int>(newIndices[static_cast<size_t>(attrib.second)]), ") ");
          }
        }

        if (used[prim.indices]) {
          prim.indices = static_cast<int>(newIndices[static_cast<size_t>(prim.indices)]);
          debugPrint(prim.indices, " (becomes ", static_cast<int>(newIndices[static_cast<size_t>(prim.indices)]), ") ");
        }
      }
    }
    debugPrint("on the Noah's Ark\n");
    // 1.4. assign new buffer views to model
    model.accessors = newAccessors;
  }

  {
    // 1. remove unused bufferViews
    // 1.1. find used bufferViews
    std::vector<bool> used(model.bufferViews.size(), false);
    int indexForDebug = 0;
    for (const auto& accessor : model.accessors) {
      if (accessor.bufferView != -1) {
        used[accessor.bufferView] = true;
        debugPrint("buffer view ", accessor.bufferView, " is used by accessor ", indexForDebug, "\n");
      }
      ++indexForDebug;
    }
    debugPrint("buffer views ");
    for (size_t i = 0; i < used.size(); ++i) {
      if (used[i]) {
        debugPrint(i, " ");
      }
    }
    debugPrint("are used\n");
    // 1.2. move bufferViews to a new vector, and calculate new indices of bufferViews
    std::vector<size_t> newIndices(model.bufferViews.size());
    std::vector<tinygltf::BufferView> newBufferViews;
    for (size_t i = 0; i < used.size(); ++i) {
      if (used[i]) {
        newBufferViews.push_back(model.bufferViews[i]);
        newIndices[i] = newBufferViews.size() - 1;
        debugPrint("buffer view ", i, " gets new index: ", newIndices[i], "\n");
      }
    }
    // 1.3. change indices in accessors
    debugPrint("taking buffer views ");
    for (auto& accessor : model.accessors) {
      if (accessor.bufferView != -1) {
        debugPrint(accessor.bufferView, " (becomes ", static_cast<int>(newIndices[static_cast<size_t>(accessor.bufferView)]), ") ");
        accessor.bufferView = static_cast<int>(newIndices[static_cast<size_t>(accessor.bufferView)]);
      }
    }
    debugPrint("on the Noah's Ark\n");
    // 1.4. assign new buffer views to model
    model.bufferViews = newBufferViews;
  }

  {
    // 2. remove unused buffers
    // 2.1. find used buffers
    std::vector<bool> used(model.buffers.size(), false);
    int indexForDebug = 0;
    for (const auto& bufferView : model.bufferViews) {
      if (bufferView.buffer != -1) {
        used[bufferView.buffer] = true;
        debugPrint("Buffer ", bufferView.buffer, " is used by buffer view ", indexForDebug, "\n");
      }
      ++indexForDebug;
    }
    debugPrint("buffers ");
    for (size_t i = 0; i < used.size(); ++i) {
      if (used[i]) {
        debugPrint(i, " ");
      }
    }
    debugPrint("are used\n");
    // 2.2. move buffers to a new vector, and calculate new indices of bufferViews
    std::vector<size_t> newIndices(model.buffers.size());
    std::vector<tinygltf::Buffer> newBuffers;
    for (size_t i = 0; i < used.size(); ++i) {
      if (used[i]) {
        // Yes, I'm copying a giant vector of chars.
        // I hope you're not angry, but if you are, let me know and suffer the suffering I deserve.
        newBuffers.push_back(model.buffers[i]);
        newIndices[i] = newBuffers.size() - 1;
        debugPrint("buffer ", i, " gets new index: ", newIndices[i], "\n");
      }
    }
    // 2.3. change indices in accessors
    debugPrint("taking buffers ");
    for (auto& bufferView : model.bufferViews) {
      if (bufferView.buffer != -1) {
        bufferView.buffer = static_cast<int>(newIndices[static_cast<size_t>(bufferView.buffer)]);
        debugPrint(bufferView.buffer, " ");
      }
    }
    debugPrint("on the Noah's Ark\n");
    // 2.4. assign new buffer views to model
    model.buffers = newBuffers;
  }
}

bool bake(std::string filename_src) {
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename_src);

  if (!warn.empty()) {
    debugPrint("Warn: ", warn, "\n");
  }

  if (!err.empty()) {
    debugPrint("Err: ", err, "\n");
  }

  if (!ret) {
    debugPrint<true>("Failed to parse glTF\n");
    return false;
  }

  model.extensionsUsed.push_back("KHR_mesh_quantization");
  model.extensionsRequired.push_back("KHR_mesh_quantization");

  size_t newBufferSize = 0;  // how big will be the new buffer
  for (tinygltf::Mesh& mesh : model.meshes) {
    for (tinygltf::Primitive& prim : mesh.primitives) {
      size_t maxNewElements = 0;  // max of all attributes: how many elements are there in this attribute
      for (auto attrib : prim.attributes) {
        if (attrib.first != "POSITION" && attrib.first != "NORMAL" && attrib.first != "TANGENT"
            && attrib.first != "TEXCOORD_0") {
          continue;
        }

        if (attrib.second < 0) {
          continue;
        }

        if (model.accessors[attrib.second].bufferView < 0) {
          continue;
        }

        maxNewElements = std::max(maxNewElements, model.accessors[attrib.second].count);
      }
      newBufferSize += maxNewElements * 32;
      debugPrint("will add memory for ", maxNewElements, " new elements\n");
    }
  }
  // but also need to store indices in the buffer
  for (tinygltf::Mesh& mesh : model.meshes) {
    for (tinygltf::Primitive& prim : mesh.primitives) {
      if (prim.indices != -1) {
        newBufferSize += sizeof(uint32_t) * model.accessors[prim.indices].count;
        debugPrint("add buffer space for ", model.accessors[prim.indices].count, " indices\n");
      }
    }
  }

  tinygltf::Buffer newBuffer;
  newBuffer.data.resize(newBufferSize);

  size_t currentPosInBuffer = 0;
  for (tinygltf::Mesh& mesh : model.meshes) {
    for (tinygltf::Primitive& prim : mesh.primitives) {
      size_t maxElems = 0;
      for (auto attrib : prim.attributes) {
        if (attrib.first != "POSITION" && attrib.first != "NORMAL" && attrib.first != "TANGENT"
            && attrib.first != "TEXCOORD_0") {
          continue;
        }

        if (attrib.second < 0) {
          continue;
        }

        if (model.accessors[attrib.second].bufferView < 0) {
          continue;
        }

        tinygltf::Accessor& accessor = model.accessors[attrib.second];
        tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
        tinygltf::Buffer& buffer = model.buffers[view.buffer];
        int componentType = accessor.componentType;
        size_t stride = accessor.ByteStride(view);
        size_t offset = accessor.byteOffset + view.byteOffset;
        size_t elems = accessor.count;
        maxElems = std::max(maxElems, elems);

        if (attrib.first == "POSITION") {
          for (size_t elem = 0; elem < elems; ++elem) {
            std::vector<float> normalizedPosition = normalizePositionData(&buffer.data[offset + elem * stride], componentType);
            debugPrint("normalized position: ", normalizedPosition[0], ", ", normalizedPosition[1], ", ", normalizedPosition[2]);
            debugPrint(" goes to index ", currentPosInBuffer + elem * 32, "\n");
            memcpy(&newBuffer.data[currentPosInBuffer + elem * 32], normalizedPosition.data(), 12);
          }

          SetAccessorAndBufferViewValues(accessor, model, 32, currentPosInBuffer, GLTF_type::FLOAT, 12, static_cast<int>(model.buffers.size()), false);
        }

        if (attrib.first == "NORMAL") {
          for (size_t elem = 0; elem < elems; ++elem) {
            std::vector<signed char> normalizedNormal = normalizeNormalOrTangentData(&buffer.data[offset + elem * stride], componentType);
            debugPrint("normalized normal: ");
            printQuantizedVector(normalizedNormal);
            debugPrint(" goes to index ", currentPosInBuffer + elem * 32, "\n");
            memcpy(&newBuffer.data[currentPosInBuffer + elem * 32 + 12], normalizedNormal.data(), 3);
          }

          SetAccessorAndBufferViewValues(accessor, model, 32, currentPosInBuffer + 12, GLTF_type::BYTE, 3, static_cast<int>(model.buffers.size()), true);
        }

        if (attrib.first == "TANGENT") {
          for (size_t elem = 0; elem < elems; ++elem) {
            std::vector<signed char> normalizedTangent = normalizeNormalOrTangentData(&buffer.data[offset + elem * stride], componentType);
            debugPrint("normalized tangent: ");
            printQuantizedVector(normalizedTangent);
            debugPrint(" goes to index ", currentPosInBuffer + elem * 32, "\n");
            memcpy(&newBuffer.data[currentPosInBuffer + elem * 32 + 24], normalizedTangent.data(), 3);
            newBuffer.data[currentPosInBuffer + elem * 32 + 24 + 3] = 127;  // Tangent is actually a 4d vector, 4th component is 1 or -1.
          }

          SetAccessorAndBufferViewValues(accessor, model, 32, currentPosInBuffer + 24, GLTF_type::BYTE, 3, static_cast<int>(model.buffers.size()), true);
        }

        if (attrib.first == "TEXCOORD_0") {
          for (size_t elem = 0; elem < elems; ++elem) {
            std::vector<float> normalizedTexcoord = normalizeTexcoordData(&buffer.data[offset + elem * stride], componentType);
            debugPrint("normalized texcoord (", normalizedTexcoord[0], ", ", normalizedTexcoord[1]);
            debugPrint(" goes to index ", currentPosInBuffer + elem * 32, "\n");
            memcpy(&newBuffer.data[currentPosInBuffer + elem * 32 + 16], normalizedTexcoord.data(), 8);
          }

          SetAccessorAndBufferViewValues(accessor, model, 32, currentPosInBuffer + 16, GLTF_type::FLOAT, 8, static_cast<int>(model.buffers.size()), false);
        }
      }

      // I didn't know we need to do this.
      for (auto it = prim.attributes.begin(); it != prim.attributes.end(); ++it) {
        if (it->first == "TEXCOORD_1") {
          prim.attributes.erase(it);
          break;
        }
      }

      currentPosInBuffer += maxElems * 32;
    }
  }

  // store indices in the buffer
  for (tinygltf::Mesh& mesh : model.meshes) {
    for (tinygltf::Primitive& prim : mesh.primitives) {
      if (prim.indices != -1) {
        tinygltf::Accessor& accessor = model.accessors[prim.indices];
        tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
        tinygltf::Buffer& buffer = model.buffers[view.buffer];
        int componentType = accessor.componentType;
        size_t stride = accessor.ByteStride(view);
        size_t offset = accessor.byteOffset + view.byteOffset;
        size_t elems = accessor.count;

        for (size_t elem = 0; elem < elems; ++elem) {
          uint32_t normalizedIndex = normalizeIndexData(&buffer.data[offset + elem * stride], componentType);
          debugPrint("normalized index: ", normalizedIndex);
          debugPrint(" goes to index ", currentPosInBuffer + elem * sizeof(uint32_t), "\n");
          memcpy(&newBuffer.data[currentPosInBuffer + elem * sizeof(uint32_t)], &normalizedIndex, 4);
        }

        SetAccessorAndBufferViewValues(accessor, model, 0, currentPosInBuffer, GLTF_type::UINT,
            sizeof(uint32_t), static_cast<int>(model.buffers.size()), false, GLTF_buffer_view_target::ELEMENT_ARRAY_BUFFER);

        currentPosInBuffer += sizeof(uint32_t) * model.accessors[prim.indices].count;
      }
    }
  }

  std::string_view pathview(filename_src);
  std::string savepath(pathview.substr(0, pathview.length() - 5));  // remove ".gltf" in the end
  savepath += "_baked";

  model.buffers.push_back(newBuffer);
  size_t slash_pos = pathview.rfind("\\");
  if (slash_pos == std::string_view::npos) {
    slash_pos = pathview.rfind("/");
  }
  std::string_view uri_view = pathview.substr(slash_pos + 1);
  model.buffers.back().uri = std::string(uri_view.substr(0, uri_view.length() - 5)) + "_baked.bin";
  savepath += ".gltf";

  RemoveUnusedObjects(model);

  debugPrint("File will be saved as ", savepath, "\n");
  debugPrint("Buffer's uri will be ", model.buffers.back().uri, "\n");
  if (!loader.WriteGltfSceneToFile(&model, savepath, false, false, true, false)) {
    debugPrint<true>("FAILED SAVING THE SCENE");
  }
  // also need to save the buffer to file here

  return true;
}
