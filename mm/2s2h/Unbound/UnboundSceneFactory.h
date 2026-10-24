#pragma once
// Resource factory for the JSON scene and room documents (unbound-docs/SPEC.md §4). It builds the same SOH::Scene
// command list the binary factory builds, so nothing downstream of the resource changes.
#include <ship/resource/ResourceFactoryJson.h>
#include <ship/resource/ResourceLoader.h>

namespace S2H::Unbound {

class ResourceFactoryJsonSceneV1 final : public Ship::ResourceFactoryJson {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

void RegisterResourceFactories(const std::shared_ptr<Ship::ResourceLoader>& loader);

} // namespace S2H::Unbound
