#pragma once

#include <memory>
#include <string>

#include "tape_device.h"
#include "tape_enumerator.h"

namespace qlto {

std::unique_ptr<TapeDevice> make_default_device(std::string &err);
std::unique_ptr<TapeEnumerator> make_default_enumerator(std::string &err);

} // namespace qlto
