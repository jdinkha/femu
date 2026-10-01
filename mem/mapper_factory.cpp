#include "mapper_factory.h"
#include "mapper000.h"
#include "mapper001.h"
#include "mapper002.h"
#include "mapper003.h"
#include "mapper004.h"
#include "mapper005.h"
#include "mapper007.h"
#include "mapper009.h"
#include "mapper011.h"
#include "mapper013.h"
#include "mapper016.h"
#include "mapper018.h"
#include "mapper019.h"
#include "mapper021.h"
#include "mapper024.h"
#include "mapper032.h"
#include "mapper033.h"
#include "mapper034.h"
#include "mapper037.h"
#include "mapper038.h"
#include "mapper041.h"
#include "mapper047.h"
#include "mapper064.h"
#include "mapper065.h"
#include "mapper066.h"
#include "mapper067.h"
#include "mapper068.h"
#include "mapper069.h"
#include "mapper070.h"
#include "mapper071.h"
#include "mapper072.h"
#include "mapper073.h"
#include "mapper075.h"
#include "mapper077.h"
#include "mapper078.h"
#include "mapper079.h"
#include "mapper080.h"
#include "mapper082.h"
#include "mapper085.h"
#include "mapper086.h"
#include "mapper087.h"
#include "mapper089.h"

std::unique_ptr<Mapper> CreateMapper(RomInfo& info) {
    const uint8_t prg = info.prgBanks, chr = info.chrBanks, sub = info.submapper;
    switch (info.mapper) {
        case 0:   return std::make_unique<Mapper_000>(prg, chr);
        case 1:   return std::make_unique<Mapper_001>(prg, chr);
        case 2:   return std::make_unique<Mapper_002>(prg, chr, sub);
        case 3:   return std::make_unique<Mapper_003>(prg, chr, 3, sub);
        case 4:   return std::make_unique<Mapper_004>(prg, chr);
        case 5:   return std::make_unique<Mapper_005>(prg, chr);
        case 7:   return std::make_unique<Mapper_007>(prg, chr, sub);
        case 9:   return std::make_unique<Mapper_009>(prg, chr, 9);
        case 10:  return std::make_unique<Mapper_009>(prg, chr, 10);
        case 11:  return std::make_unique<Mapper_011>(prg, chr);
        case 13:  return std::make_unique<Mapper_013>(prg, chr);
        case 16:  return std::make_unique<Mapper_016>(prg, chr, 16, sub, info.battery);
        case 18:  return std::make_unique<Mapper_018>(prg, chr);
        case 19:  return std::make_unique<Mapper_019>(prg, chr);
        case 21:  return std::make_unique<Mapper_021>(prg, chr, 21, sub);
        case 22:  return std::make_unique<Mapper_021>(prg, chr, 22, sub);
        case 23:  return std::make_unique<Mapper_021>(prg, chr, 23, sub);
        case 24:  return std::make_unique<Mapper_024>(prg, chr, 24);
        case 25:  return std::make_unique<Mapper_021>(prg, chr, 25, sub);
        case 26:  return std::make_unique<Mapper_024>(prg, chr, 26);
        case 32:  return std::make_unique<Mapper_032>(prg, chr, sub);
        case 33:  return std::make_unique<Mapper_033>(prg, chr, 33);
        case 34:  return std::make_unique<Mapper_034>(prg, chr, sub);
        case 37:  return std::make_unique<Mapper_037>(prg, chr);
        case 38:  return std::make_unique<Mapper_038>(prg, chr);
        case 41:  return std::make_unique<Mapper_041>(prg, chr);
        case 47:  return std::make_unique<Mapper_047>(prg, chr);
        case 48:  return std::make_unique<Mapper_033>(prg, chr, 48);
        case 64:  return std::make_unique<Mapper_064>(prg, chr, 64);
        case 65:  return std::make_unique<Mapper_065>(prg, chr);
        case 66:  return std::make_unique<Mapper_066>(prg, chr);
        case 67:  return std::make_unique<Mapper_067>(prg, chr);
        case 68:  return std::make_unique<Mapper_068>(prg, chr);
        case 69:  return std::make_unique<Mapper_069>(prg, chr);
        case 70:  return std::make_unique<Mapper_070>(prg, chr, 70);
        case 71:  return std::make_unique<Mapper_071>(prg, chr, sub);
        case 72:  return std::make_unique<Mapper_072>(prg, chr, 72);
        case 73:  return std::make_unique<Mapper_073>(prg, chr);
        case 75:  return std::make_unique<Mapper_075>(prg, chr);
        case 77:  return std::make_unique<Mapper_077>(prg, chr);
        case 78: {
            bool holy_diver = sub == 3 || (sub != 1 && info.four_screen);
            info.four_screen = false; // that bit is the Holy Diver flag here
            return std::make_unique<Mapper_078>(prg, chr, holy_diver);
        }
        case 79:  return std::make_unique<Mapper_079>(prg, chr);
        case 80:  return std::make_unique<Mapper_080>(prg, chr, 80);
        case 82:  return std::make_unique<Mapper_082>(prg, chr);
        case 85:  return std::make_unique<Mapper_085>(prg, chr, sub);
        case 86:  return std::make_unique<Mapper_086>(prg, chr);
        case 87:  return std::make_unique<Mapper_087>(prg, chr);
        case 89:  return std::make_unique<Mapper_089>(prg, chr);
        case 92:  return std::make_unique<Mapper_072>(prg, chr, 92);
        case 146: return std::make_unique<Mapper_079>(prg, chr);
        case 152: return std::make_unique<Mapper_070>(prg, chr, 152);
        case 158: return std::make_unique<Mapper_064>(prg, chr, 158);
        case 159: return std::make_unique<Mapper_016>(prg, chr, 159, sub, info.battery);
        case 185: return std::make_unique<Mapper_003>(prg, chr, 185, sub);
        case 207: return std::make_unique<Mapper_080>(prg, chr, 207);
        default:  return nullptr;
    }
}
