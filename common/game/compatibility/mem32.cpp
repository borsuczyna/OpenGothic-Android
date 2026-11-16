#include "mem32.h"
#include "logbudget.h"

#include <Tempest/Log>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <cstddef>

using namespace Tempest;

Mem32::Mem32() {
  /*
   *  [0x00001000 .. 0x80000000] - (2GB) user space
   *  [0x80000000 .. 0xc0000000] - (1GB) extra space(reserved for opengothic use; pinned memory)
   *  [0xc0000000 .. 0xffffffff] - (1GB) kernel space
   */
  region.emplace_back(Region(0x1000,0x80000000));
  }

Mem32::~Mem32() {
  for(auto& rgn:region) {
    if((rgn.status==S_Allocated || rgn.status==S_Callback) && rgn.real!=nullptr) {
      std::free(rgn.real);
      rgn.real = nullptr;
      }
    }
  }

void Mem32::implSetCallbackR(Type t, std::function<void(void*, uint32_t)> fn, size_t elt) {
  auto& m       = memMap[t];
  m.read        = std::move(fn);
  m.elementSize = uint32_t(elt);
  }

void Mem32::implSetCallbackW(Type t, std::function<void(void*, uint32_t)> fn, size_t elt) {
  auto& m       = memMap[t];
  m.write       = std::move(fn);
  m.elementSize = uint32_t(elt);
  }

Mem32::ptr32_t Mem32::pin(void* mem, ptr32_t address, uint32_t size, const char* comment) {
  if(auto rgn = implAllocAt(address,size)) {
    rgn->size    = size;
    rgn->real    = mem;
    rgn->status  = S_Pin;
    rgn->comment = comment;
    return address;
    }
  throw std::bad_alloc();
  }

Mem32::ptr32_t Mem32::pin(void* mem, uint32_t size, const char* comment) {
  if(auto rgn = implAlloc(size)) {
    rgn->size    = size;
    rgn->real    = mem;
    rgn->status  = S_Pin;
    rgn->comment = comment;
    return rgn->address;
    }
  throw std::bad_alloc();
  }

Mem32::ptr32_t Mem32::pin(ptr32_t address, uint32_t size, Type type) {
  if(auto rgn = implAllocAt(address,size)) {
    rgn->type   = type;
    rgn->size   = size;
    rgn->real   = nullptr; // allocated on demand
    rgn->status = S_Callback;
    return address;
    }
  throw std::bad_alloc();
  }

Mem32::ptr32_t Mem32::alloc(ptr32_t address, uint32_t size, const char* comment) {
  if(auto rgn = implAllocAt(address,size)) {
    rgn->real = std::calloc(rgn->size,1);
    if(rgn->real==nullptr) {
      compactage();
      return 0;
      }
    rgn->size    = size;
    rgn->status  = S_Allocated;
    rgn->comment = comment;
    return address;
    }
  throw std::bad_alloc();
  }

Mem32::ptr32_t Mem32::alloc(uint32_t size, const char* comment) {
  if(auto rgn = implAlloc(size)) {
    rgn->real = std::calloc(rgn->size,1);
    if(rgn->real==nullptr) {
      compactage();
      return 0;
      }
    rgn->status  = S_Allocated;
    rgn->type    = Type::plain;
    rgn->comment = comment;
    return rgn->address;
    }
  return 0;
  }

Mem32::ptr32_t Mem32::alloc(uint32_t size, Type type, const char* comment) {
  if(auto rgn = implAlloc(size)) {
    rgn->type    = type;
    rgn->status  = S_Callback;
    rgn->comment = comment;
    return rgn->address;
    }
  return 0;
  }

void Mem32::free(ptr32_t address) {
  if(address==0)
    return;
  for(size_t i=0; i<region.size(); ++i) {
    if(region[i].address!=address)
      continue;
    if(region[i].status==S_Pin || region[i].status==S_Unused) {
      if(Compatibility::logBudget("mem32:free-pinned"))
        Log::e("mem_free: address is not an owned allocation: ",address);
      return;
      }
    std::free(region[i].real);
    region[i].real   = nullptr;
    region[i].status = S_Unused;
    region[i].type   = Type::plain;
    compactage();
    return;
    }
  Log::e("mem_free: heap block wan't allocated by script: ", reinterpret_cast<void*>(uint64_t(address)));
  }

const void* Mem32::deref(ptr32_t address, uint32_t size) {
  auto r = translate(address);
  if(r==nullptr) {
    if(Compatibility::logBudget("mem32:deref")) Log::e("deref: address translation failure: ", reinterpret_cast<void*>(uint64_t(address)));
    return nullptr;
    }
  if(uint64_t(r->address)+r->size < uint64_t(address)+size) {
    if(Compatibility::logBudget("mem32:deref")) Log::e("deref: memmory block is too small: ", reinterpret_cast<void*>(uint64_t(address)), " ", size);
    return nullptr;
    }
  const auto rgn = *r;
  memSyncRead(rgn.type, rgn, address, size);
  address -= rgn.address;
  return reinterpret_cast<uint8_t*>(rgn.real)+address;
  }

void* Mem32::derefv(ptr32_t address, uint32_t size) {
  auto rgn = translate(address);
  if(rgn==nullptr) {
    if(Compatibility::logBudget("mem32:deref")) Log::e("deref: address translation failure: ", reinterpret_cast<void*>(uint64_t(address)));
    return nullptr;
    }
  if(uint64_t(rgn->address)+rgn->size < uint64_t(address)+size) {
    if(Compatibility::logBudget("mem32:deref")) Log::e("deref: memmory block is too small: ", reinterpret_cast<void*>(uint64_t(address)), " ", size);
    return nullptr;
    }
  if(rgn->status==S_Callback) {
    if(Compatibility::logBudget("mem32:deref")) Log::e("deref: unable to deref callback-memory: ", reinterpret_cast<void*>(uint64_t(address)));
    return nullptr;
    }
  // memSyncRead(rgn->type, rgn, address, size);
  address -= rgn->address;
  return reinterpret_cast<uint8_t*>(rgn->real)+address;
  }

auto Mem32::deref(ptr32_t address) -> std::tuple<void*, uint32_t> {
  auto r = translate(address);
  if(r==nullptr) {
    if(Compatibility::logBudget("mem32:deref")) Log::e("deref: address translation failure: ", reinterpret_cast<void*>(uint64_t(address)));
    return {nullptr,0};
    }

  const auto     rgn   = *r;
  const ptr32_t  dAddr = address - rgn.address;
  const uint32_t size  = rgn.size - dAddr;
  memSyncRead(rgn.type, rgn, address, size);
  return {reinterpret_cast<uint8_t*>(rgn.real)+dAddr, size};
  }

uint64_t Mem32::bytesInUse() const {
  uint64_t ret = 0;
  for(auto& r:region)
    if(r.status!=S_Unused)
      ret += r.size;
  return ret;
  }

void Mem32::compactage() {
  for(size_t i=0; i+1<region.size(); ) {
    auto& a = region[i+0];
    auto& b = region[i+1];
    if(a.status==S_Unused && a.status==b.status && a.address+a.size==b.address) {
      a.size += b.size;
      b.size = 0;
      region.erase(region.begin()+ptrdiff_t(i+1));
      } else {
      ++i;
      }
    }
  }

void Mem32::writeInt(ptr32_t address, int32_t v) {
  auto r = translate(address);
  if(r==nullptr) {
    Log::e("mem_writeint: address translation failure: ", reinterpret_cast<void*>(uint64_t(address)));
    return;
    }
  // a copy: the callbacks of the memory allocate, and the table of the regions moves
  const Region rgn = *r;

  if(uint64_t(address-rgn.address)+4 > rgn.size) {
    // the four bytes go over the end of the block: the rest belongs to the next one (or to nobody)
    uint8_t bytes[4] = {};
    std::memcpy(bytes,&v,4);
    for(uint32_t i=0; i<4; ++i)
      writeByte(address+i,bytes[i]);
    lastWrite = address;
    return;
    }

  memSyncRead(rgn.type, rgn, address, 4); // memory hazzards othwerwise!
  const ptr32_t dAddr = address-rgn.address;
  auto ptr = reinterpret_cast<uint8_t*>(rgn.real)+dAddr;
  std::memcpy(ptr,&v,4);
  lastWrite = address;
  memSyncWrite(rgn.type, rgn, address, 4);
  }

int32_t Mem32::readInt(ptr32_t address) {
  const auto r = translate(address);
  if(r==nullptr) {
    if(Compatibility::logBudget("mem32:mem_readint")) Log::e("mem_readint:  address translation failure: ", reinterpret_cast<void*>(uint64_t(address)));
    return 0;
    }

  const auto    rgn   = *r;
  const ptr32_t dAddr = address-rgn.address;
  if(uint64_t(dAddr)+4 > rgn.size) {
    // the four bytes go over the end of the block: the rest belongs to the next one (or to nobody)
    uint8_t bytes[4] = {};
    for(uint32_t i=0; i<4; ++i)
      bytes[i] = readByte(address+i);
    int32_t ret = 0;
    std::memcpy(&ret,bytes,4);
    return ret;
    }
  memSyncRead(rgn.type, rgn, address, 4);

  auto ptr = reinterpret_cast<uint8_t*>(rgn.real)+dAddr;
  int32_t ret = 0;
  std::memcpy(&ret,ptr,4);
  return ret;
  }

/// One byte of the memory, 0 where nothing lives
uint8_t Mem32::readByte(ptr32_t address) {
  const auto r = translate(address);
  if(r==nullptr)
    return 0;
  const auto    rgn   = *r;
  const ptr32_t dAddr = address-rgn.address;
  memSyncRead(rgn.type, rgn, address, 1);
  return reinterpret_cast<uint8_t*>(rgn.real)[dAddr];
  }

void Mem32::writeByte(ptr32_t address, uint8_t v) {
  auto r = translate(address);
  if(r==nullptr) {
    if(Compatibility::logBudget("mem32:writebyte")) Log::e("mem_writeint: the end of the block is not mapped: ", reinterpret_cast<void*>(uint64_t(address)));
    return;
    }
  const Region rgn = *r;   // a copy, see writeInt
  lastWrite = address;
  memSyncRead(rgn.type, rgn, address, 1);
  reinterpret_cast<uint8_t*>(rgn.real)[address-rgn.address] = v;
  memSyncWrite(rgn.type, rgn, address, 1);
  }

void Mem32::copyBytes(ptr32_t psrc, ptr32_t pdst, uint32_t size) {
  auto s = translate(psrc);
  auto d = translate(pdst);
  if(s==nullptr || s->status==S_Unused) {
    if(Compatibility::logBudget("mem32:mem_copybytes")) Log::e("mem_copybytes: address translation failure: ", reinterpret_cast<void*>(uint64_t(psrc)));
    return;
    }
  if(d==nullptr || d->status==S_Unused) {
    if(Compatibility::logBudget("mem32:mem_copybytes")) Log::e("mem_copybytes: address translation failure: ", reinterpret_cast<void*>(uint64_t(pdst)));
    return;
    }
  const auto    src   = *s;
  const auto    dst   = *d;

  lastWrite = pdst;
  size_t sOff = psrc - src.address;
  size_t dOff = pdst - dst.address;
  size_t sz   = size;
  if(src.size<sOff+size) {
    if(Compatibility::logBudget("mem32:mem_copybytes")) Log::e("mem_copybytes: copy-size exceed source block size: ", size);
    sz = std::min(src.size-sOff,sz);
    }
  if(dst.size<dOff+size) {
    if(Compatibility::logBudget("mem32:mem_copybytes")) Log::e("mem_copybytes: copy-size exceed destination block size: ", size);
    sz = std::min(dst.size-dOff,sz);
    }
  memSyncRead(src.type, src, ptr32_t(src.address+sOff), uint32_t(sz));
  memSyncRead(dst.type, dst, ptr32_t(dst.address+dOff), uint32_t(sz));
  std::memmove(reinterpret_cast<uint8_t*>(dst.real)+dOff,
              reinterpret_cast<uint8_t*>(src.real)+sOff,
              sz);
  memSyncWrite(dst.type, dst, ptr32_t(dst.address+dOff), uint32_t(sz));
  }

Mem32::Region* Mem32::implAlloc(uint32_t size) {
  if(size==0 || uint64_t(size)+memAlign-1>UINT32_MAX)
    return nullptr;
  size = ((size+memAlign-1)/memAlign)*memAlign;

  for(size_t i=0; i<region.size(); ++i) {
    if(region[i].status!=S_Unused)
      continue;
    const uint32_t off = region[i].address % memAlign;
    if(region[i].size<size+off)
      continue;
    if(size!=region[i].size) {
      auto p2 = region[i];
      p2.address+=size;
      p2.size   -=size;
      region.insert(region.begin()+ptrdiff_t(i+1),p2);
      }
    region[i].size = size;
    return &region[i];
    }

  return nullptr;
  }

Mem32::ptr32_t Mem32::realloc(ptr32_t address, uint32_t size) {
  if(size==0) {
    free(address);
    return 0;
    }
  if(uint64_t(size)+memAlign-1>UINT32_MAX)
    return 0;
  if(address!=0) {
    const auto* original = implTranslate(address);
    if(original==nullptr || original->address!=address || original->status==S_Pin) {
      if(Compatibility::logBudget("mem32:realloc-pinned"))
        Log::e("realloc: address is not an owned allocation: ",address);
      return 0;
      }
    }
  size = ((size+memAlign-1)/memAlign)*memAlign;
  if(implRealloc(address,size))
    return address;

  auto next = implAlloc(size);
  if(next==nullptr)
    return 0;

  auto src = translate(address);
  if(src==nullptr) {
    if(address!=0) {
      if(Compatibility::logBudget("mem32:realloc")) Log::e("realloc: address translation failure: ", reinterpret_cast<void*>(uint64_t(address)));
      compactage();
      return 0;
      }
    }

  auto ret = next->address;
  if(src!=nullptr) {
    auto* resized = std::realloc(src->real,next->size);
    if(resized==nullptr)
      return 0;
    next->type    = src->type;
    next->status  = src->status;
    next->real    = resized;
    next->comment = src->comment;
    if(size>src->size)
      std::memset(reinterpret_cast<uint8_t*>(next->real)+src->size, 0, size-src->size);

    src->real     = nullptr;
    src->status   = S_Unused;
    compactage();
    } else {
    next->real    = std::calloc(next->size, 1);
    if(next->real==nullptr)
      return 0;
    next->status = S_Allocated;
    }

  return ret;
  }

Mem32::Region* Mem32::implAllocAt(ptr32_t address, uint32_t size) {
  if(size==0 || uint64_t(address)+size>uint64_t(UINT32_MAX)+1)
    return nullptr;
  for(size_t i=0; i<region.size(); ++i) {
    auto& rgn = region[i];

    if(address!=0) {
      if(!(rgn.address<=address && uint64_t(address)+size<=uint64_t(rgn.address)+rgn.size))
        continue;
      } else {
      if(rgn.size<size)
        continue;
      }

    if(rgn.status!=S_Unused) {
      Log::e("DMA: failed to pin a ",size," bytes of memory: block is in use");
      return 0;
      }

    if(address==0)
      address = region[i].address;

    if(region[i].address<address) {
      uint32_t off = (address-region[i].address);
      auto p2 = region[i];
      p2.address+=off;
      p2.size   -=off;
      region.insert(region.begin()+ptrdiff_t(i+1),p2);
      region[i].size = off;
      ++i;
      }
    if(size!=region[i].size) {
      auto p2 = region[i];
      p2.address+=size;
      p2.size   -=size;
      region.insert(region.begin()+ptrdiff_t(i+1),p2);
      }
    region[i].size = size;
    return &region[i];
    }
  return nullptr;
  }

bool Mem32::implRealloc(ptr32_t address, uint32_t nsize) {
  if(address==0)
    return false;
  // NOTE: in place only
  for(size_t i=0; i<region.size(); ++i) {
    auto& rgn = region[i];
    if(rgn.address!=address)
      continue; //TODO: binary-search

    if(nsize==rgn.size)
      return true;

    if(nsize<rgn.size) {
      auto next = std::realloc(rgn.real, nsize);
      if(next==nullptr)
        return false;
      rgn.real = next;
      Region frgn(address+nsize, rgn.size-nsize);
      rgn.size = nsize;
      region.insert(region.begin() + intptr_t(i + 1), frgn);
      return true;
      }

    if(i+1==region.size())
      return false; // can't expand

    auto& rgn1 = region[i+1];
    if(rgn1.status==S_Unused && rgn.size + rgn1.size>=nsize) {
      auto next = std::realloc(rgn.real, nsize);
      if(next==nullptr)
        return false;
      std::memset(reinterpret_cast<uint8_t*>(next)+rgn.size, 0, nsize-rgn.size);
      rgn1.address += (nsize-rgn.size);
      rgn1.size    -= (nsize-rgn.size);
      rgn.real      = next;
      rgn.size      = nsize;
      if(rgn1.size==0)
        compactage();
      return true;
      }

    return false;
    }
  return false;
  }

Mem32::Region* Mem32::implTranslate(ptr32_t address) {
  // TODO: binary-search
  for(auto& rgn:region) {
    if(rgn.address<=address && uint64_t(address)<uint64_t(rgn.address)+rgn.size && rgn.status!=S_Unused)
      return &rgn;
    }
  return nullptr;
  }

Mem32::Region* Mem32::translate(ptr32_t address) {
  if(address==0)
    return nullptr;

  auto ret = implTranslate(address);
  if(ret==nullptr)
    return nullptr;

  auto& rgn = *ret;
  if(rgn.status!=S_Callback)
    return ret;

  if(rgn.real==nullptr) {
    rgn.real = std::calloc(rgn.size, 1);
    if(rgn.real==nullptr)
      throw std::bad_alloc();
    }
  return ret;
  }

void Mem32::memSyncRead(Type type, const Region& rgn, ptr32_t address, uint32_t size) {
  if(type==Type::plain)
    return;
  auto m = memMap.find(type);
  if(m==memMap.end()) {
    // a region of a type without callbacks: the memory is read as it is, an exception would end the game
    if(Compatibility::logBudget("mem32:nocallback"))
      Log::e("memSyncRead: callback is not provided for callback memory: ", reinterpret_cast<void*>(uint64_t(address)),
             " (type ",uint32_t(type)," region ",reinterpret_cast<void*>(uint64_t(rgn.address))," size ",rgn.size,")");
    return;
    }
  memSyncRead(m->second, rgn, address, size);
  }

void Mem32::memSyncRead(Callback& cb, const Region& rgn, ptr32_t address, uint32_t size) {
  if(cb.elementSize==0 || !cb.read)
    return;
  address -= rgn.address;

  const uint32_t begin = address/cb.elementSize;
  const uint32_t end   = uint32_t(std::min((uint64_t(address)+size+cb.elementSize-1)/cb.elementSize,uint64_t(rgn.size/cb.elementSize)));

  auto ptr = reinterpret_cast<uint8_t*>(rgn.real);
  for(size_t i=begin; i<end; ++i) {
    cb.read(ptr+i*cb.elementSize, uint32_t(i));
    }
  }

void Mem32::memSyncWrite(Type type, const Region& rgn, ptr32_t address, uint32_t size) {
  if(type==Type::plain)
    return;
  auto m = memMap.find(type);
  if(m==memMap.end()) {
    Log::e("memSyncWrite: unable to write to mapped memory: ", reinterpret_cast<void*>(uint64_t(address)),
           " (type ",uint32_t(type)," region ",reinterpret_cast<void*>(uint64_t(rgn.address))," size ",rgn.size," \"",
           rgn.comment!=nullptr ? rgn.comment : "","\")");
    return;
    }
  memSyncWrite(m->second, rgn, address, size);
  }

void Mem32::memSyncWrite(Callback& cb, const Region& rgn, ptr32_t address, uint32_t size) {
  if(cb.elementSize==0)
    return;
  if(!cb.write) {
    Log::e("memSyncWrite: there is no writer of the memory: ", reinterpret_cast<void*>(uint64_t(address)),
           " (region ",reinterpret_cast<void*>(uint64_t(rgn.address))," size ",rgn.size,")");
    return;
    }

  address -= rgn.address;
  const uint32_t begin = address/cb.elementSize;
  const uint32_t end   = uint32_t(std::min((uint64_t(address)+size+cb.elementSize-1)/cb.elementSize,uint64_t(rgn.size/cb.elementSize)));

  auto ptr = reinterpret_cast<uint8_t*>(rgn.real);
  for(size_t i=begin; i<end; ++i) {
    cb.write(ptr+i*cb.elementSize, uint32_t(i));
    }
  }
