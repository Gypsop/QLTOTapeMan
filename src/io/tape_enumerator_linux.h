#pragma once

#include "tape_enumerator.h"

namespace qlto {

class TapeEnumeratorLinux : public TapeEnumerator {
public:
    std::vector<BlockDevice> list(std::string &err) override;
};

} // namespace qlto
