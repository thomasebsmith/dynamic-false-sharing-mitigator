/*
 * Copyright (C) 2004-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! @file
 *  This file contains a configurable cache class
 */

#ifndef PIN_MDCACHE_H
#define PIN_MDCACHE_H

#define KILO 1024
#define MEGA (KILO * KILO)
#define GIGA (KILO * MEGA)

typedef UINT64 CACHE_STATS; // type of cache hit/miss counters

#include "mutex.H"
#include <algorithm>
#include <sstream>
#include <vector>
using std::ostringstream;
using std::string;
/*! RMR (rodric@gmail.com)
 *   - temporary work around because decstr()
 *     casts 64 bit ints to 32 bit ones
 */
static string mydecstr(UINT64 v, UINT32 w) {
  ostringstream o;
  o.width(w);
  o << v;
  string str(o.str());
  return str;
}

/*!
 *  @brief Checks if n is a power of 2.
 *  @returns true if n is power of 2
 */
static inline bool IsPower2(UINT32 n) { return ((n & (n - 1)) == 0); }

/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
static inline INT32 FloorLog2(UINT32 n) {
  INT32 p = 0;

  if (n == 0)
    return -1;

  if (n & 0xffff0000) {
    p += 16;
    n >>= 16;
  }
  if (n & 0x0000ff00) {
    p += 8;
    n >>= 8;
  }
  if (n & 0x000000f0) {
    p += 4;
    n >>= 4;
  }
  if (n & 0x0000000c) {
    p += 2;
    n >>= 2;
  }
  if (n & 0x00000002) {
    p += 1;
  }

  return p;
}

/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
static inline INT32 CeilLog2(UINT32 n) { return FloorLog2(n - 1) + 1; }

/*!
 *  @brief Cache tag - self clearing on creation
 */
class CACHE_TAG {
private:
  ADDRINT _tag;
  bool _is_tombstone;

public:
  CACHE_TAG(ADDRINT tag = 0) {
    _tag = tag;
    _is_tombstone = false;
  }
  bool operator==(const CACHE_TAG &right) const { return _tag == right._tag; }
  operator ADDRINT() const { return _tag; }
  void kill() { _is_tombstone = true; }
  bool is_dead() { return _is_tombstone; }
};

/*!
 * Everything related to cache sets
 */
namespace CACHE_SET {
/*!
 *  @brief Cache set direct mapped
 */
class DIRECT_MAPPED {
private:
  CACHE_TAG _tag;

public:
  DIRECT_MAPPED(UINT32 associativity = 1) { ASSERTX(associativity == 1); }

  VOID SetAssociativity(UINT32 associativity) { ASSERTX(associativity == 1); }
  UINT32 GetAssociativity(UINT32 associativity) { return 1; }

  ACCESS_RESULT Find(CACHE_TAG tag) { return (_tag == tag); }
  VOID Replace(CACHE_TAG tag) { _tag = tag; }
  VOID Invalidate(CACHE_TAG tag) {}
};

/*!
 *  @brief Cache set with round robin replacement
 */
template <UINT32 MAX_ASSOCIATIVITY = 4> class ROUND_ROBIN {
private:
  CACHE_TAG _tags[MAX_ASSOCIATIVITY];
  UINT32 _tagsLastIndex;
  UINT32 _nextReplaceIndex;
  UINT32 _nextTombstoneIndex;

public:
  ROUND_ROBIN(UINT32 associativity = MAX_ASSOCIATIVITY)
      : _tagsLastIndex(associativity - 1) {
    ASSERTX(associativity <= MAX_ASSOCIATIVITY);
    _nextReplaceIndex = _tagsLastIndex;
    _nextTombstoneIndex = _nextReplaceIndex;

    for (INT32 index = _tagsLastIndex; index >= 0; index--) {
      _tags[index] = CACHE_TAG(0);
    }
  }

  VOID SetAssociativity(UINT32 associativity) {
    ASSERTX(associativity <= MAX_ASSOCIATIVITY);
    _tagsLastIndex = associativity - 1;
    _nextReplaceIndex = _tagsLastIndex;
    _nextTombstoneIndex = _tagsLastIndex;
  }
  UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }

  ACCESS_RESULT Find(CACHE_TAG tag) {
    ACCESS_RESULT result = CACHE_MISS;

    for (INT32 index = _tagsLastIndex; index >= 0; index--) {
      // this is an ugly micro-optimization, but it does cause a
      // tighter assembly loop for ARM that way ...
      if (_tags[index] == tag) {
        if (_tags[index].is_dead())
          // If we find a tombstone, it could still be in there.
          result = CACHE_TOMBSTONE;
        else
          result = CACHE_HIT;
        goto end;
      }
    }

  end:
    return result;
  }

  VOID Replace(CACHE_TAG tag) {
    // g++ -O3 too dumb to do CSE on following lines?!
    const UINT32 index = _nextReplaceIndex;

    _tags[index] = tag;
    // condition typically faster than modulo
    if (_nextTombstoneIndex == _nextReplaceIndex)
      _nextTombstoneIndex = (index == 0 ? _tagsLastIndex : index - 1);
    _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
  }

  VOID Invalidate(CACHE_TAG tag) {
    for (INT32 index = _tagsLastIndex; index >= 0; index--) {
      // If we find it and it's alive, kill it
      if (_tags[index] == tag && !_tags[index].is_dead()) {
        _tags[index].kill();
        // Put it on the remove list
        std::swap(_tags[index], _tags[_nextTombstoneIndex])
            // Increment the remove list
            _nextTombstoneIndex = (index == 0 ? _tagsLastIndex : index - 1);
      }
    }
  }
};

} // namespace CACHE_SET

namespace CACHE_ALLOC {
typedef enum { STORE_ALLOCATE, STORE_NO_ALLOCATE } STORE_ALLOCATION;
}

/*!
 *  @brief Generic cache base class; no allocate specialization, no cache set
 * specialization
 */
class CACHE_BASE {
public:
  // types, constants
  typedef enum {
    ACCESS_TYPE_LOAD,
    ACCESS_TYPE_STORE,
    ACCESS_TYPE_INVALIDATE,
    ACCESS_TYPE_NUM
  } ACCESS_TYPE;

  typedef enum {
    CACHE_MISS = 0,
    CACHE_TOMBSTONE = 1,
    CACHE_HIT = 3,
  } ACCESS_RESULT;

  typedef enum {
    CACHE_TYPE_ICACHE,
    CACHE_TYPE_DCACHE,
    CACHE_TYPE_NUM
  } CACHE_TYPE;

  UINT32 CALC_RESULT_INDEX(ACCESS_RESULT x) const {
    return __builtin_popcount(x);
  }

protected:
  static const UINT32 HIT_MISS_NUM = 3;
  CACHE_STATS _access[ACCESS_TYPE_NUM][HIT_MISS_NUM];

private: // input params
  const std::string _name;
  const UINT32 _cacheSize;
  const UINT32 _lineSize;
  const UINT32 _associativity;

  // computed params
  const UINT32 _lineShift;
  const UINT32 _setIndexMask;

  CACHE_STATS SumAccess(ACCESS_RESULT hit) const {
    CACHE_STATS sum = 0;

    for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++) {
      sum += _access[accessType][CALC_RESULT_INDEX(hit)];
    }

    return sum;
  }

protected:
  UINT32 NumSets() const { return _setIndexMask + 1; }

public:
  // constructors/destructors
  CACHE_BASE(std::string name, UINT32 cacheSize, UINT32 lineSize,
             UINT32 associativity);

  // accessors
  UINT32 CacheSize() const { return _cacheSize; }
  UINT32 LineSize() const { return _lineSize; }
  UINT32 Associativity() const { return _associativity; }
  //
  CACHE_STATS Hits(ACCESS_TYPE accessType) const {
    return _access[accessType][CALC_RESULT_INDEX(CACHE_HIT)];
  }
  CACHE_STATS Misses(ACCESS_TYPE accessType) const {
    return _access[accessType][CALC_RESULT_INDEX(CACHE_MISS)];
  }
  CACHE_STATS Tombstones(ACCESS_TYPE accessType) const {
    return _access[accessType][CALC_RESULT_INDEX(CACHE_TOMBSTONE)];
  }
  CACHE_STATS Accesses(ACCESS_TYPE accessType) const {
    return Hits(accessType) + Misses(accessType) + Tombstones(accessType);
  }
  CACHE_STATS Hits() const { return SumAccess(CACHE_HIT); }
  CACHE_STATS Misses() const { return SumAccess(CACHE_MISS); }
  CACHE_STATS Tombstones() const { return SumAccess(CACHE_TOMBSTONE); }
  CACHE_STATS Accesses() const { return Hits() + Misses() + Tombstones; }

  VOID SplitAddress(const ADDRINT addr, CACHE_TAG &tag,
                    UINT32 &setIndex) const {
    tag = addr >> _lineShift;
    setIndex = tag & _setIndexMask;
  }

  VOID SplitAddress(const ADDRINT addr, CACHE_TAG &tag, UINT32 &setIndex,
                    UINT32 &lineIndex) const {
    const UINT32 lineMask = _lineSize - 1;
    lineIndex = addr & lineMask;
    SplitAddress(addr, tag, setIndex);
  }

  string StatsLong(string prefix = "", CACHE_TYPE = CACHE_TYPE_DCACHE) const;
};

CACHE_BASE::CACHE_BASE(std::string name, UINT32 cacheSize, UINT32 lineSize,
                       UINT32 associativity)
    : _name(name), _cacheSize(cacheSize), _lineSize(lineSize),
      _associativity(associativity), _lineShift(FloorLog2(lineSize)),
      _setIndexMask((cacheSize / (associativity * lineSize)) - 1) {
  ASSERTX(IsPower2(_lineSize));
  ASSERTX(IsPower2(_setIndexMask + 1));

  for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++) {
    _access[accessType][CALC_RESULT_INDEX(CACHE_HIT)] = 0;
    _access[accessType][CALC_RESULT_INDEX(CACHE_MISS)] = 0;
    _access[accessType][CALC_RESULT_INDEX(CACHE_TOMBSTONE)] = 0;
  }
}

/*!
 *  @brief Stats output method
 */

string CACHE_BASE::StatsLong(string prefix, CACHE_TYPE cache_type) const {
  const UINT32 headerWidth = 19;
  const UINT32 numberWidth = 12;

  string out;

  out += prefix + _name + ":" + "\n";

  if (cache_type != CACHE_TYPE_ICACHE) {
    for (UINT32 i = 0; i < ACCESS_TYPE_NUM; i++) {
      const ACCESS_TYPE accessType = ACCESS_TYPE(i);

      std::string type(accessType == ACCESS_TYPE_LOAD ? "Load" : "Store");

      out += prefix + ljstr(type + "-Hits:      ", headerWidth) +
             mydecstr(Hits(accessType), numberWidth) + "  " +
             fltstr(100.0 * Hits(accessType) / Accesses(accessType), 2, 6) +
             "%\n";

      out += prefix + ljstr(type + "-Misses:    ", headerWidth) +
             mydecstr(Misses(accessType), numberWidth) + "  " +
             fltstr(100.0 * Misses(accessType) / Accesses(accessType), 2, 6) +
             "%\n";

      out +=
          prefix + ljstr(type + "-Tombstones:    ", headerWidth) +
          mydecstr(Tombstones(accessType), numberWidth) + "  " +
          fltstr(100.0 * Tombstones(accessType) / Accesses(accessType), 2, 6) +
          "%\n";

      out += prefix + ljstr(type + "-Accesses:  ", headerWidth) +
             mydecstr(Accesses(accessType), numberWidth) + "  " +
             fltstr(100.0 * Accesses(accessType) / Accesses(accessType), 2, 6) +
             "%\n";

      out += prefix + "\n";
    }
  }

  out += prefix + ljstr("Total-Hits:      ", headerWidth) +
         mydecstr(Hits(), numberWidth) + "  " +
         fltstr(100.0 * Hits() / Accesses(), 2, 6) + "%\n";

  out += prefix + ljstr("Total-Misses:    ", headerWidth) +
         mydecstr(Misses(), numberWidth) + "  " +
         fltstr(100.0 * Misses() / Accesses(), 2, 6) + "%\n";

  out += prefix + ljstr("Total-Tombstones:    ", headerWidth) +
         mydecstr(Tombstones(), numberWidth) + "  " +
         fltstr(100.0 * Tombstones() / Accesses(), 2, 6) + "%\n";

  out += prefix + ljstr("Total-Accesses:  ", headerWidth) +
         mydecstr(Accesses(), numberWidth) + "  " +
         fltstr(100.0 * Accesses() / Accesses(), 2, 6) + "%\n";
  out += "\n";

  return out;
}

/*!
 *  @brief Templated cache class with specific cache set allocation policies
 *
 *  All that remains to be done here is allocate and deallocate the right
 *  type of cache sets.
 */
template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
class CACHE : public CACHE_BASE {
private:
  SET _sets[MAX_SETS];
  mutex _mu mutex &_write_mu;
  std::vector<CACHE *> _peers;

public:
  // constructors/destructors
  CACHE(std::string name, UINT32 cacheSize, UINT32 lineSize,
        UINT32 associativity, mutex &write_mu)
      : CACHE_BASE(name, cacheSize, lineSize, associativity),
        _write_mu(write_mu) {
    ASSERTX(NumSets() <= MAX_SETS);

    for (UINT32 i = 0; i < NumSets(); i++) {
      _sets[i].SetAssociativity(associativity);
    }
  }

  // modifiers
  /// Cache access from addr to addr+size-1
  bool Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);
  /// Cache access at addr that does not span cache lines
  bool AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType);
  /// Cache invalidation from addr to addr+size-1

  // Become aware of caches for other CPUs
  void RegisterPeers(const std::vector<CACHE *> &peers);
  // Become aware of the cache for one other CPU
  void RegisterPeer(CACHE *peer);

  /// Cache invalidation from addr to addr+size-1
  void Invalidate(ADDRINT addr, UINT32 size);
  /// Cache invalidation at addr that does not span cache lines
  void InvalidateSingleLine(ADDRINT addr);
};

/*!
 *  @return true if all accessed cache lines hit
 */

template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
bool CACHE<SET, MAX_SETS, STORE_ALLOCATION>::Access(ADDRINT addr, UINT32 size,
                                                    ACCESS_TYPE accessType) {
  ptr_lock_guard lock(accessType == ACCESS_TYPE_STORE ? _write_mu : nullptr);
  lock_guard lock(_mu);
  const ADDRINT highAddr = addr + size;
  ACCESS_RESULT allHit = CACHE_HIT;

  const ADDRINT lineSize = LineSize();
  const ADDRINT notLineMask = ~(lineSize - 1);
  do {
    CACHE_TAG tag;
    UINT32 setIndex;

    SplitAddress(addr, tag, setIndex);

    SET &set = _sets[setIndex];

    ACCESS_RESULT localHit = set.Find(tag);
    allHit &= localHit;

    // on miss and tombstone, loads always allocate, stores optionally
    if ((localHit != CACHE_HIT) &&
        (accessType == ACCESS_TYPE_LOAD ||
         STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE)) {
      set.Replace(tag);
    }

    addr = (addr & notLineMask) + lineSize; // start of next cache line
  } while (addr < highAddr);

  if (accessType == ACCESS_TYPE_STORE) {
    for (auto *peer : peers) {
      peer->Invalidate(addr, size);
    }
  }

  _access[accessType][CALC_RESULT_INDEX(allHit)]++;

  return allHit == CACHE_HIT;
}

/*!
 *  @return true if accessed cache line hits
 */
template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
bool CACHE<SET, MAX_SETS, STORE_ALLOCATION>::AccessSingleLine(
    ADDRINT addr, ACCESS_TYPE accessType) {
  ptr_lock_guard lock(accessType == ACCESS_TYPE_STORE ? _write_mu : nullptr);
  lock_guard lock(_mu);
  CACHE_TAG tag;
  UINT32 setIndex;

  SplitAddress(addr, tag, setIndex);

  SET &set = _sets[setIndex];

  ACCESS_RESULT hit = set.Find(tag);

  // on miss, loads always allocate, stores optionally
  if ((hit != CACHE_HIT) && (accessType == ACCESS_TYPE_LOAD ||
                             STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE)) {
    set.Replace(tag);
  }

  _access[accessType][CALC_RESULT_INDEX(hit)]++;

  if (accessType == ACCESS_TYPE_STORE) {
    for (auto *peer : peers)
      peer->InvalidateSingleLine(addr);
  }

  return hit == CACHE_HIT;
}

/*!
 * Become aware of caches for other CPUs
 */
template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
void CACHE<SET, MAX_SETS, STORE_ALLOCATION>::RegisterPeers(
    const std::vector<CACHE *> &peers) {
  lock_guard lock(_mu);
  for (auto *peer)
    _peers.push_back(peer);
}

/*!
 * Become aware of the cache for one other CPU
 */
template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
void CACHE<SET, MAX_SETS, STORE_ALLOCATION>::RegisterPeer(CACHE *peer) {
  lock_guard lock(_mu);
  _peers.push_back(peer);
}

/*!
 * Cache invalidation from addr to addr+size-1
 */
template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
void CACHE<SET, MAX_SETS, STORE_ALLOCATION>::Invalidate(ADDRINT addr,
                                                        UINT32 size) {
  lock_guard lock(_mu);
  const ADDRINT highAddr = addr + size;
  ACCESS_RESULT allHit = CACHE_HIT;

  const ADDRINT lineSize = LineSize();
  const ADDRINT notLineMask = ~(lineSize - 1);
  do {
    CACHE_TAG tag;
    UINT32 setIndex;

    SplitAddress(addr, tag, setIndex);

    SET &set = _sets[setIndex];

    ACCESS_RESULT localHit = set.Find(tag);
    allHit &= localHit;

    // If it's in the cache, remove it
    if (localHit == CACHE_HIT) {
      set.Invalidate(tag);
    }

    addr = (addr & notLineMask) + lineSize; // start of next cache line
  } while (addr < highAddr);

  _access[ACCESS_TYPE_INVALIDATE][CALC_RESULT_INDEX(allHit)]++;
}

/*!
 * Cache invalidation at addr that does not span cache lines
 */
template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
void CACHE<SET, MAX_SETS, STORE_ALLOCATION>::InvalidateSingleLine(
    ADDRINT addr) {
  // Get it like normal. If it's a miss, ignore it. If it's a hit with a
  // tombstone, ignore it. If it's a hit, make it a tombstone and log it.
  lock_guard lock(_mu);
  CACHE_TAG tag;
  UINT32 setIndex;
  SplitAddress(addr, tag, setIndex);
  SET &set = _sets[setIndex];
  ACCESS_RESULT hit = set.Find(tag);
  // If it's in the cache, invalidate it
  if (hit == CACHE_HIT) {
    set.Invalidate(tag);
  }

  _access[ACCESS_TYPE_INVALIDATE][CALC_RESULT_INDEX(hit)]++;
}

// define shortcuts
#define CACHE_DIRECT_MAPPED(MAX_SETS, ALLOCATION)                              \
  CACHE<CACHE_SET::DIRECT_MAPPED, MAX_SETS, ALLOCATION>
#define CACHE_ROUND_ROBIN(MAX_SETS, MAX_ASSOCIATIVITY, ALLOCATION)             \
  CACHE<CACHE_SET::ROUND_ROBIN<MAX_ASSOCIATIVITY>, MAX_SETS, ALLOCATION>

#endif // PIN_MDCACHE_H
