#pragma once
#include <ship/resource/ResourceFactoryJson.h>

namespace S2H::Unbound {

class ResourceFactoryJsonCollisionV1 final : public Ship::ResourceFactoryJson {
  public:
    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override;
};

} // namespace S2H::Unbound
