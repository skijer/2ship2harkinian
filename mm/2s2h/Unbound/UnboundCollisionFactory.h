#pragma once
// Resource factory for the JSON collision document (unbound-docs/SPEC.md §4.5). It builds the same SOH::CollisionHeader
// the binary factory builds, with s32 vertices read from a bulk file.
#include <ship/resource/ResourceFactoryJson.h>

namespace S2H::Unbound {

class ResourceFactoryJsonCollisionV1 final : public Ship::ResourceFactoryJson {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

} // namespace S2H::Unbound
