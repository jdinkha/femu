#include "mapper_factory.h"
#include "mapper000.h"
#include "mapper001.h"
#include "mapper002.h"
#include "mapper004.h"

std::unique_ptr<Mapper> CreateMapper(RomInfo& info) {
    const uint8_t prg = info.prgBanks, chr = info.chrBanks, sub = info.submapper;
    switch (info.mapper) {
        case 0:   return std::make_unique<Mapper_000>(prg, chr);
        case 1:   return std::make_unique<Mapper_001>(prg, chr);
        case 2:   return std::make_unique<Mapper_002>(prg, chr, sub);
        case 4:   return std::make_unique<Mapper_004>(prg, chr);
        default:  return nullptr;
    }
}
