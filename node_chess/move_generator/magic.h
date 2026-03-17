#ifndef __MAGIC_BITS_HPP__
#define __MAGIC_BITS_HPP__

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <vector>


namespace magic_bits {

class Attacks {
public:
  uint64_t Rook(const uint64_t occupancy_bitboard, const int index) const {
    return AttackBitboard(occupancy_bitboard, rook_, index);
  }
  uint64_t Bishop(const uint64_t occupancy_bitboard, const int index) const {
    return AttackBitboard(occupancy_bitboard, bishop_, index);
  }

  uint64_t Queen(const uint64_t occupancy_bitboard, const int index) const {
    return Rook(occupancy_bitboard, index) | Bishop(occupancy_bitboard, index);
  }

  enum class PieceType { BISHOP, ROOK };

  class Direction {
  public:
    enum D { NORTH, SOUTH, EAST, WEST, NORTH_EAST, NORTH_WEST, SOUTH_EAST, SOUTH_WEST };

    Direction(D direction) : direction_(direction) {}

    int NextIndex(int index) const {
      int row = Row(static_cast<unsigned>(index));
      int col = Col(static_cast<unsigned>(index));

      switch (direction_) {
        case NORTH:      ++row;        break;
        case SOUTH:      --row;        break;
        case EAST:       ++col;        break;
        case WEST:       --col;        break;
        case NORTH_EAST: ++row; ++col; break;
        case NORTH_WEST: ++row; --col; break;
        case SOUTH_EAST: --row; ++col; break;
        case SOUTH_WEST: --row; --col; break;
      }
      return static_cast<int>((row > 7 || col > 7 || row < 0 || col < 0) ? -1 : static_cast<int>(Indx(static_cast<unsigned>(row), static_cast<unsigned>(col))));
    }

    int EdgeDistance(int index) const {
      using std::min;
      int row = Row(static_cast<unsigned>(index));
      int col = Col(static_cast<unsigned>(index));

      auto inv = [](int x) -> int { return 7 - x; };

      int d = -1;
      switch (direction_) {
        case NORTH:      d = inv(row);                break;
        case SOUTH:      d = row;                     break;
        case EAST:       d = inv(col);                break;
        case WEST:       d = col;                     break;
        case NORTH_EAST: d = min(inv(row), inv(col)); break;
        case NORTH_WEST: d = min(inv(row), col);      break;
        case SOUTH_EAST: d = min(row, inv(col));      break;
        case SOUTH_WEST: d = min(row, col);           break;
      }
      assert(d >= 0 && d <= 7);
      return d;
    }

    static unsigned Indx(unsigned row, unsigned col) { return row * 8 + col; }
    static int Row(unsigned index) { return index / 8; }
    static int Col(unsigned index) { return index % 8; }

    D direction_;
  };

  static constexpr int kSquares = 64;

  template <PieceType piece_type>
  class Generator {
  public:
    Generator() {
      std::vector<Direction> directions;
      if constexpr (piece_type == PieceType::BISHOP) {
        shifts_ = {
            6, 5, 5, 5, 5, 5, 5, 6,
            5, 5, 5, 5, 5, 5, 5, 5,
            5, 5, 7, 7, 7, 7, 5, 5,
            5, 5, 7, 9, 9, 7, 5, 5,
            5, 5, 7, 9, 9, 7, 5, 5,
            5, 5, 7, 7, 7, 7, 5, 5,
            5, 5, 5, 5, 5, 5, 5, 5,
            6, 5, 5, 5, 5, 5, 5, 6,
        };
        directions = {Direction(Direction::NORTH_EAST), Direction(Direction::NORTH_WEST),
                      Direction(Direction::SOUTH_EAST), Direction(Direction::SOUTH_WEST)};
      } else {
        shifts_ = {
            12, 11, 11, 11, 11, 11, 11, 12,
            11, 10, 10, 10, 10, 10, 10, 11,
            11, 10, 10, 10, 10, 10, 10, 11,
            11, 10, 10, 10, 10, 10, 10, 11,
            11, 10, 10, 10, 10, 10, 10, 11,
            11, 10, 10, 10, 10, 10, 10, 11,
            11, 10, 10, 10, 10, 10, 10, 11,
            12, 11, 11, 11, 11, 11, 11, 12,
        };
        directions = {Direction(Direction::NORTH), Direction(Direction::SOUTH),
                      Direction(Direction::EAST), Direction(Direction::WEST)};
      }

      #ifdef MAGIC_BITS_REGENERATE_MAGICS
            std::function<uint64_t()> rand_gen = ZeroBitBiasedRandom;
      #else
            static const std::array<uint64_t, kSquares> precomputed_magics = [] {
              if constexpr (piece_type == PieceType::BISHOP) {
                return std::array<uint64_t, kSquares>{

            0x4008010404044200ULL, 0x20880204504129ULL, 0x1628020400300010ULL, 0x14440182002000ULL, 0x40a1104002110021ULL, 0x40c42020012000ULL, 0x4040d0802509002ULL, 0x10019080203a020ULL, 0x221020820c00c0ULL, 0x400020848009080ULL, 0x89102408803002ULL, 0x220a040414800101ULL, 0x41044010048ULL, 0x4000010406400400ULL, 0x2880274424244004ULL, 0x200820802022cULL, 0x4028212008834824ULL, 0x10a0000d24042040ULL, 0x43b003802040014ULL, 0x14000804204932ULL, 0xd04012080a00100ULL, 0x1080400a01100d00ULL, 0x40800048084802ULL, 0xc000406908421000ULL, 0x20900020820200ULL, 0x4840ca022380800ULL, 0x50002800300088a4ULL, 0x2008080000806100ULL, 0x100100c411004008ULL, 0x8108008088420ULL, 0x510c001081848ULL, 0x8011204012020081ULL, 0x40242400a301002ULL, 0x808080800071205ULL, 0x840405000080260ULL, 0x2200801290104ULL, 0x4004010090140040ULL, 0x10208103008e00a1ULL, 0x242220400c46c00ULL, 0x20400c0810860ULL, 0x12120240402000ULL, 0x480230484830ULL, 0x45400c0402004c01ULL, 0x412013000800ULL, 0x422080100400400ULL, 0x1010502040900ULL, 0x8420404180040ULL, 0x2c10008216900042ULL, 0x20c0a0a0a200108ULL, 0x401040241040c00ULL, 0x28010098110400ULL, 0x400120880000ULL, 0x2b005002020000ULL, 0x9848220212120044ULL, 0x5010200800808000ULL, 0x1028850808890054ULL, 0x41805410010801ULL, 0x701002108021005ULL, 0x200008c044040480ULL, 0x8000008020218810ULL, 0x440200022020cb00ULL, 0x4304180410020e01ULL, 0x140020189000b080ULL, 0x4001820c0100e1ULL

            };

              } else {
                return std::array<uint64_t, kSquares>{
            0x4280018020400410ULL, 0x440005000200048ULL, 0x1080100220011880ULL, 0x100100020280500ULL, 0x4080140088000280ULL, 0x5900240008320100ULL, 0x2000d0082000804ULL, 0x1000380c1210002ULL, 0x2000800026c00081ULL, 0x20802000400080ULL, 0x4200801000a00080ULL, 0x5001910032100ULL, 0x8000800802800c00ULL, 0x8802004200444830ULL, 0x42000854020041ULL, 0x80164b000080ULL, 0x20480800060401aULL, 0x540002001300800ULL, 0x1a09010040502006ULL, 0x410808010001802ULL, 0x4050030080100ULL, 0x280802200cc00ULL, 0x4803340010080502ULL, 0x84902000c441081ULL, 0x10400080088820ULL, 0x402400480200480ULL, 0x8020408200220010ULL, 0x20032004021c8ULL, 0x98080080800400ULL, 0x4062004200040910ULL, 0x220420400018810ULL, 0x4000904600090084ULL, 0x4000248c800040ULL, 0x91008021004000ULL, 0x10d8401101002001ULL, 0x80c0082101001004ULL, 0x401002411000800ULL, 0x1001209000400ULL, 0x8460080d04009022ULL, 0x3069040082000041ULL, 0x304002802b8000ULL, 0xa020084000808020ULL, 0x10002000888010ULL, 0x4230002411010008ULL, 0xa8008004028008ULL, 0x3094104004080120ULL, 0x38025810040021ULL, 0x802008400420001ULL, 0x42c1088208406200ULL, 0xc01820008080ULL, 0x1004406000110100ULL, 0x9012100080580080ULL, 0x2442510005080100ULL, 0x80060400802a0080ULL, 0x20008002005d0080ULL, 0x4011084204600ULL, 0x108300102081c202ULL, 0x2810020904001ULL, 0x2003024008200091ULL, 0x5202409001001ULL, 0x8012002804102002ULL, 0x46000410081502ULL, 0xa0181200911024ULL, 0x1228041040022ULL

            };

              }
            }();

            int magics_index = 0;
            std::function<uint64_t()> rand_gen = [&magics_index]() -> uint64_t {
              return precomputed_magics.at(static_cast<size_t>(magics_index++));
            };
      #endif

      for (int i = 0; i < kSquares; ++i) {
        masks_[static_cast<size_t>(i)] = 0ULL;
        for (const Direction & d : directions) {
          masks_[static_cast<size_t>(i)] |= MaskBits(d, i);
        }
        std::vector<uint64_t> tmp_attack_table;
        GenerateMagic(rand_gen, directions, i, shifts_[static_cast<size_t>(i)], &magics_[static_cast<size_t>(i)], &tmp_attack_table);
        offsets_[static_cast<size_t>(i)] = static_cast<int>(attack_table_.size());
        attack_table_.insert(attack_table_.end(), tmp_attack_table.begin(), tmp_attack_table.end());
      }
    }

    const std::array<int, kSquares> & shifts() const { return shifts_; }
    const std::array<uint64_t, kSquares> & masks() const { return masks_; }
    const std::array<uint64_t, kSquares> & magics() const { return magics_; }
    const std::array<int, kSquares> & offsets() const { return offsets_; }
    const std::vector<uint64_t> & attack_table() const { return attack_table_; }

    // Masks all the bits from the given index, and along the given direction to 1, excluding the
    // square given by the index and the edge of the board along given direction.
    static uint64_t MaskBits(const Direction& direction, const int index) {
      uint64_t bitboard = 0ULL;
      int next_index = index;
      while ((next_index = direction.NextIndex(next_index)) >= 0 &&
             direction.NextIndex(next_index) >= 0) {
        bitboard |= (1ULL << next_index);
      }
      return bitboard;
    }

    // Generate an attack bitboard from a given square in the given direction for a specific
    // occupancy of pieces.
    static uint64_t GenerateAttack(const Direction& direction, const int index,
                                   const uint64_t occupancy) {
      uint64_t attack_bb = 0ULL;
      for (int i = index; (i = direction.NextIndex(i)) != -1;) {
        attack_bb |= (1ULL << i);
        if (occupancy & (1ULL << i)) {
          break;
        }
      }
      return attack_bb;
    }

    // Generate all piece occupancies along a rank, file or diagonal, in the given direction, with
    // index as the reference point. The square given by the index and the edge of the board in the
    // given direction are not covered. For example, direction = NORTH_WEST, index = 29 (marked by
    // X) will generate all combinations of occupancies for squares marked by # (there are 8
    // possible occupancies):
    // 8 | 0 0 0 0 0 0 0 0
    // 7 | 0 0 # 0 0 0 0 0
    // 6 | 0 0 0 # 0 0 0 0
    // 5 | 0 0 0 0 # 0 0 0
    // 4 | 0 0 0 0 0 X 0 0
    // 3 | 0 0 0 0 0 0 0 0
    // 2 | 0 0 0 0 0 0 0 0
    // 1 | 0 0 0 0 0 0 0 0
    // -------------------
    //   | A B C D E F G H
    static std::vector<uint64_t> GenerateOccupancies(const int index, const Direction& direction) {
      std::vector<uint64_t> bbv;

      // Number of squares in this direction excluding current square and edge of the board.
      const int num_squares = direction.EdgeDistance(index) - 1;
      if (num_squares <= 0) {
        return bbv;
      }

      // Number of possible piece occupancies in these squares along the given direction.
      const unsigned num_occupancies = (1U << num_squares);

      // Create bitboard for each occupancy with the index next to given index as starting point,
      // along the given direction.
      for (unsigned occupancy = 0U; occupancy < num_occupancies; ++occupancy) {
        uint64_t bitboard = 0ULL;
        int next_index = index;
        for (unsigned bit_mask = 1U; bit_mask <= occupancy; bit_mask <<= 1) {
          next_index = direction.NextIndex(next_index);
          assert(next_index != -1);
          bitboard |= (uint64_t(!!(occupancy & bit_mask)) << next_index);
        }
        bbv.push_back(bitboard);
      }
      return bbv;
    }

    // Generate all possible piece occupancies along the given directions from the reference square.
    static std::vector<uint64_t> GenerateOccupancies(const int index,
                                                     const std::vector<Direction>& directions) {
      std::vector<uint64_t> occupancies;
      for (const auto& direction : directions) {
        const auto bbv = GenerateOccupancies(index, direction);
        if (bbv.empty()) {
          continue;
        }
        if (occupancies.empty()) {
          occupancies.insert(occupancies.end(), bbv.begin(), bbv.end());
          continue;
        }
        std::vector<uint64_t> tmp;
        for (const uint64_t bb : bbv) {
          for (const uint64_t occupancy : occupancies) {
            tmp.push_back(bb | occupancy);
          }
        }
        occupancies.swap(tmp);
      }
      return occupancies;
    }

    static void GenerateMagic(const std::function<uint64_t()>& rand_gen,
                              const std::vector<Direction>& directions, const int index,
                              const int shift_bits, uint64_t* magic,
                              std::vector<uint64_t>* attack_table) {
      std::vector<uint64_t> occupancies = GenerateOccupancies(index, directions);

      // Generate attacks.
      std::vector<uint64_t> attacks;
      for (const uint64_t occupancy : occupancies) {
        uint64_t attack = 0ULL;
        for (const Direction& direction : directions) {
          attack |= GenerateAttack(direction, index, occupancy);
        }
        attacks.push_back(attack);
      }

      // No bishop or rook attack can cover all squares of the board.
      static const uint64_t kInvalidAttack = ~0ULL;

      // Trial and error approach to generate magics.
      while (true) {
        std::vector<uint64_t> table(1U << shift_bits, kInvalidAttack);
        uint64_t candidate_magic = rand_gen();
        bool collision = false;
        for (size_t k = 0; k < occupancies.size(); ++k) {
          const uint64_t occupancy = occupancies.at(k);
          const uint64_t attack = attacks.at(k);
          const int offset = static_cast<int>((occupancy * candidate_magic) >> (64 - shift_bits));
          if (table.at(static_cast<size_t>(offset)) == kInvalidAttack || table.at(static_cast<size_t>(offset)) == attack) {
            table.at(static_cast<size_t>(offset)) = attack;
          } else {
            collision = true;
            break;
          }
        }
        if (!collision) {
          *magic = candidate_magic;
          attack_table->swap(table);
          break;
        }
      }
    }

    // Returns an unsigned 64 bit random number.
    static uint64_t U64Rand() {
      return (uint64_t(0xFFFF & rand()) << 48) | (uint64_t(0xFFFF & rand()) << 32) |
             (uint64_t(0xFFFF & rand()) << 16) | uint64_t(0xFFFF & rand());
    }

    // Bias the random number to contain more 0 bits.
    static uint64_t ZeroBitBiasedRandom() { return U64Rand() & U64Rand() & U64Rand(); }

    std::array<int, kSquares> shifts_;
    std::array<uint64_t, kSquares> masks_;
    std::array<uint64_t, kSquares> magics_;
    std::array<int, kSquares> offsets_;
    std::vector<uint64_t> attack_table_;
  };

  template <PieceType piece_type>
  static uint64_t AttackBitboard(const uint64_t bitboard, const Generator<piece_type>& gen,
                                 const int index) {
    return gen.attack_table()[AttackTableIndex(bitboard, gen.masks()[static_cast<size_t>(index)], gen.magics()[static_cast<size_t>(index)],
                                               gen.shifts()[static_cast<size_t>(index)], gen.offsets()[static_cast<size_t>(index)])];
  }

  static size_t AttackTableIndex(const uint64_t bitboard, uint64_t mask, uint64_t magic, int shift,
                                 int offset) {
    uint64_t occupancy = bitboard & mask;
    size_t toReturn = static_cast<size_t>(((occupancy * magic) >> (kSquares - shift)) + static_cast<uint64_t>(offset));
    return toReturn;
  }

  const Generator<PieceType::BISHOP> bishop_;
  const Generator<PieceType::ROOK> rook_;
};

} // namespace magic_bits

#endif