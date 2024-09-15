#pragma once

#include <optional>

#include <tiny_gltf.h>

#include <etna/Buffer.hpp>
#include <etna/BlockingTransferHelper.hpp>

#include "scene/light/FinitePointLight.h"
#include "scene/light/DirectionalLight.h"
#include "scene/light/InfinitePointLight.h"
#include "scene/light/AmbientLight.h"


template <typename T>
struct GltfLightTryPack;

template <>
struct GltfLightTryPack<FinitePointLight>
{
  static std::optional<FinitePointLight> tryPack(
    glm::mat4 transform, const tinygltf::Light& gltf_light);
};

template <>
struct GltfLightTryPack<InfinitePointLight>
{
  static std::optional<InfinitePointLight> tryPack(
    glm::mat4 transform, const tinygltf::Light& gltf_light);
};

template <>
struct GltfLightTryPack<DirectionalLight>
{
  static std::optional<DirectionalLight> tryPack(
    glm::mat4 transform, const tinygltf::Light& gltf_light);
};

template <>
struct GltfLightTryPack<AmbientLight>
{
  static std::optional<AmbientLight> tryPack(
    glm::mat4 transform, const tinygltf::Light& gltf_light);
};

using KnownLightTypes =
  std::tuple<FinitePointLight, InfinitePointLight, DirectionalLight, AmbientLight>;

namespace detail
{

template <typename F, std::size_t... Indices>
void for_each_known_light_type_impl(F&& func, std::index_sequence<Indices...>)
{
  (func.template operator()<Indices, std::tuple_element_t<Indices, KnownLightTypes>>(), ...);
}

template <template <std::size_t, typename> typename F, typename I, typename K>
struct ForEachKnownLightTypeImpl;

template <template <std::size_t, typename> typename F, std::size_t... Indices, typename... L>
struct ForEachKnownLightTypeImpl<F, std::index_sequence<Indices...>, std::tuple<L...>>
{
  using Type = std::tuple<typename F<Indices, L>::Type...>;
};

} // namespace detail

template <typename F>
void for_each_known_light_type(F&& func)
{
  return detail::for_each_known_light_type_impl(
    std::forward<F>(func), std::make_index_sequence<std::tuple_size_v<KnownLightTypes>>{});
}

template <template <std::size_t, typename> typename F>
using ForEachKnownLightType = detail::ForEachKnownLightTypeImpl<
  F,
  std::make_index_sequence<std::tuple_size_v<KnownLightTypes>>,
  KnownLightTypes>;

class SceneLights
{
private:
  template <std::size_t, typename L>
  struct LightBufferData
  {
    using Type = std::vector<L>;
  };

public:
  struct HomogenousLightBuffer
  {
    etna::Buffer buffer;
    std::size_t count{0};
  };

  void load(
    std::span<const glm::mat4> instance_matrices,
    const tinygltf::Model& model,
    etna::BlockingTransferHelper& transfer_helper,
    etna::OneShotCmdMgr& one_shot_cmd_mgr);

  template <typename F>
  void forEachKnownLightType(F&& func) const
  {
    for_each_known_light_type(
      [this, &func]<std::size_t I, typename L>() { func.template operator()<I, L>(buffers[I]); });
  }

private:
  std::array<HomogenousLightBuffer, std::tuple_size_v<KnownLightTypes>> buffers;
};
