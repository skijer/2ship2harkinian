#pragma once

#include <initializer_list>
#include <string_view>

// One lowercase GitHub login per line.
inline constexpr std::initializer_list<std::string_view> kRevokedModAuthors = {};

// One package content hash (the "content_hash" of its 2ship-signature.json) per line.
inline constexpr std::initializer_list<std::string_view> kRevokedModPackages = {};
