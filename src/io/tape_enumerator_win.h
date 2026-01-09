#pragma once

#include "tape_enumerator.h"

namespace qlto {

class TapeEnumeratorWin : public TapeEnumerator {
public:
    std::vector<BlockDevice> list(std::string &err) override;
};

} // namespace qlto
