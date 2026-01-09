#pragma once

#include <string>
#include <vector>

#include "tape_enumerator.h"

namespace qlto {

class TapeEnumeratorMac : public TapeEnumerator {
public:
    std::vector<BlockDevice> list(std::string &err) override;
};

} // namespace qlto
