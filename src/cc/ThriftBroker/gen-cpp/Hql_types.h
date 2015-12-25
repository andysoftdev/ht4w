/**
 * Autogenerated by Thrift Compiler (0.9.3)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
#ifndef Hql_TYPES_H
#define Hql_TYPES_H

#include <iosfwd>

#include <thrift/Thrift.h>
#include <thrift/TApplicationException.h>
#include <thrift/protocol/TProtocol.h>
#include <thrift/transport/TTransport.h>

#include <thrift/cxxfunctional.h>
#include "client_types.h"


namespace Hypertable { namespace ThriftGen {

class HqlResult;

class HqlResult2;

class HqlResultAsArrays;

typedef struct _HqlResult__isset {
  _HqlResult__isset() : results(false), cells(false), scanner(false), mutator(false) {}
  bool results :1;
  bool cells :1;
  bool scanner :1;
  bool mutator :1;
} _HqlResult__isset;

class HqlResult {
 public:

  HqlResult(const HqlResult&);
  HqlResult& operator=(const HqlResult&);
  HqlResult() : scanner(0), mutator(0) {
  }

  virtual ~HqlResult() throw();
  std::vector<std::string>  results;
  std::vector< ::Hypertable::ThriftGen::Cell>  cells;
  int64_t scanner;
  int64_t mutator;

  _HqlResult__isset __isset;

  void __set_results(const std::vector<std::string> & val);

  void __set_cells(const std::vector< ::Hypertable::ThriftGen::Cell> & val);

  void __set_scanner(const int64_t val);

  void __set_mutator(const int64_t val);

  bool operator == (const HqlResult & rhs) const
  {
    if (__isset.results != rhs.__isset.results)
      return false;
    else if (__isset.results && !(results == rhs.results))
      return false;
    if (__isset.cells != rhs.__isset.cells)
      return false;
    else if (__isset.cells && !(cells == rhs.cells))
      return false;
    if (__isset.scanner != rhs.__isset.scanner)
      return false;
    else if (__isset.scanner && !(scanner == rhs.scanner))
      return false;
    if (__isset.mutator != rhs.__isset.mutator)
      return false;
    else if (__isset.mutator && !(mutator == rhs.mutator))
      return false;
    return true;
  }
  bool operator != (const HqlResult &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const HqlResult & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(HqlResult &a, HqlResult &b);

inline std::ostream& operator<<(std::ostream& out, const HqlResult& obj)
{
  obj.printTo(out);
  return out;
}

typedef struct _HqlResult2__isset {
  _HqlResult2__isset() : results(false), cells(false), scanner(false), mutator(false) {}
  bool results :1;
  bool cells :1;
  bool scanner :1;
  bool mutator :1;
} _HqlResult2__isset;

class HqlResult2 {
 public:

  HqlResult2(const HqlResult2&);
  HqlResult2& operator=(const HqlResult2&);
  HqlResult2() : scanner(0), mutator(0) {
  }

  virtual ~HqlResult2() throw();
  std::vector<std::string>  results;
  std::vector< ::Hypertable::ThriftGen::CellAsArray>  cells;
  int64_t scanner;
  int64_t mutator;

  _HqlResult2__isset __isset;

  void __set_results(const std::vector<std::string> & val);

  void __set_cells(const std::vector< ::Hypertable::ThriftGen::CellAsArray> & val);

  void __set_scanner(const int64_t val);

  void __set_mutator(const int64_t val);

  bool operator == (const HqlResult2 & rhs) const
  {
    if (__isset.results != rhs.__isset.results)
      return false;
    else if (__isset.results && !(results == rhs.results))
      return false;
    if (__isset.cells != rhs.__isset.cells)
      return false;
    else if (__isset.cells && !(cells == rhs.cells))
      return false;
    if (__isset.scanner != rhs.__isset.scanner)
      return false;
    else if (__isset.scanner && !(scanner == rhs.scanner))
      return false;
    if (__isset.mutator != rhs.__isset.mutator)
      return false;
    else if (__isset.mutator && !(mutator == rhs.mutator))
      return false;
    return true;
  }
  bool operator != (const HqlResult2 &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const HqlResult2 & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(HqlResult2 &a, HqlResult2 &b);

inline std::ostream& operator<<(std::ostream& out, const HqlResult2& obj)
{
  obj.printTo(out);
  return out;
}

typedef struct _HqlResultAsArrays__isset {
  _HqlResultAsArrays__isset() : results(false), cells(false), scanner(false), mutator(false) {}
  bool results :1;
  bool cells :1;
  bool scanner :1;
  bool mutator :1;
} _HqlResultAsArrays__isset;

class HqlResultAsArrays {
 public:

  HqlResultAsArrays(const HqlResultAsArrays&);
  HqlResultAsArrays& operator=(const HqlResultAsArrays&);
  HqlResultAsArrays() : scanner(0), mutator(0) {
  }

  virtual ~HqlResultAsArrays() throw();
  std::vector<std::string>  results;
  std::vector< ::Hypertable::ThriftGen::CellAsArray>  cells;
  int64_t scanner;
  int64_t mutator;

  _HqlResultAsArrays__isset __isset;

  void __set_results(const std::vector<std::string> & val);

  void __set_cells(const std::vector< ::Hypertable::ThriftGen::CellAsArray> & val);

  void __set_scanner(const int64_t val);

  void __set_mutator(const int64_t val);

  bool operator == (const HqlResultAsArrays & rhs) const
  {
    if (__isset.results != rhs.__isset.results)
      return false;
    else if (__isset.results && !(results == rhs.results))
      return false;
    if (__isset.cells != rhs.__isset.cells)
      return false;
    else if (__isset.cells && !(cells == rhs.cells))
      return false;
    if (__isset.scanner != rhs.__isset.scanner)
      return false;
    else if (__isset.scanner && !(scanner == rhs.scanner))
      return false;
    if (__isset.mutator != rhs.__isset.mutator)
      return false;
    else if (__isset.mutator && !(mutator == rhs.mutator))
      return false;
    return true;
  }
  bool operator != (const HqlResultAsArrays &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const HqlResultAsArrays & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(HqlResultAsArrays &a, HqlResultAsArrays &b);

inline std::ostream& operator<<(std::ostream& out, const HqlResultAsArrays& obj)
{
  obj.printTo(out);
  return out;
}

}} // namespace

#endif
