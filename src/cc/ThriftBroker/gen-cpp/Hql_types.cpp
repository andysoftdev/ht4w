/**
 * Autogenerated by Thrift
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 */
#include "hql_types.h"

namespace Hypertable { namespace ThriftGen {

const char* HqlResult::ascii_fingerprint = "A95F71465AD755FCA9B583321990B6B0";
const uint8_t HqlResult::binary_fingerprint[16] = {0xA9,0x5F,0x71,0x46,0x5A,0xD7,0x55,0xFC,0xA9,0xB5,0x83,0x32,0x19,0x90,0xB6,0xB0};

uint32_t HqlResult::read(::apache::thrift::protocol::TProtocol* iprot) {

  uint32_t xfer = 0;
  std::string fname;
  ::apache::thrift::protocol::TType ftype;
  int16_t fid;

  xfer += iprot->readStructBegin(fname);

  using ::apache::thrift::protocol::TProtocolException;


  while (true)
  {
    xfer += iprot->readFieldBegin(fname, ftype, fid);
    if (ftype == ::apache::thrift::protocol::T_STOP) {
      break;
    }
    switch (fid)
    {
      case 1:
        if (ftype == ::apache::thrift::protocol::T_LIST) {
          {
            this->results.clear();
            uint32_t _size0;
            ::apache::thrift::protocol::TType _etype3;
            iprot->readListBegin(_etype3, _size0);
            this->results.resize(_size0);
            uint32_t _i4;
            for (_i4 = 0; _i4 < _size0; ++_i4)
            {
              xfer += iprot->readString(this->results[_i4]);
            }
            iprot->readListEnd();
          }
          this->__isset.results = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      case 2:
        if (ftype == ::apache::thrift::protocol::T_LIST) {
          {
            this->cells.clear();
            uint32_t _size5;
            ::apache::thrift::protocol::TType _etype8;
            iprot->readListBegin(_etype8, _size5);
            this->cells.resize(_size5);
            uint32_t _i9;
            for (_i9 = 0; _i9 < _size5; ++_i9)
            {
              xfer += this->cells[_i9].read(iprot);
            }
            iprot->readListEnd();
          }
          this->__isset.cells = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      case 3:
        if (ftype == ::apache::thrift::protocol::T_I64) {
          xfer += iprot->readI64(this->scanner);
          this->__isset.scanner = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      case 4:
        if (ftype == ::apache::thrift::protocol::T_I64) {
          xfer += iprot->readI64(this->mutator);
          this->__isset.mutator = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      default:
        xfer += iprot->skip(ftype);
        break;
    }
    xfer += iprot->readFieldEnd();
  }

  xfer += iprot->readStructEnd();

  return xfer;
}

uint32_t HqlResult::write(::apache::thrift::protocol::TProtocol* oprot) const {
  uint32_t xfer = 0;
  xfer += oprot->writeStructBegin("HqlResult");
  if (this->__isset.results) {
    xfer += oprot->writeFieldBegin("results", ::apache::thrift::protocol::T_LIST, 1);
    {
      xfer += oprot->writeListBegin(::apache::thrift::protocol::T_STRING, this->results.size());
      std::vector<std::string> ::const_iterator _iter10;
      for (_iter10 = this->results.begin(); _iter10 != this->results.end(); ++_iter10)
      {
        xfer += oprot->writeString((*_iter10));
      }
      xfer += oprot->writeListEnd();
    }
    xfer += oprot->writeFieldEnd();
  }
  if (this->__isset.cells) {
    xfer += oprot->writeFieldBegin("cells", ::apache::thrift::protocol::T_LIST, 2);
    {
      xfer += oprot->writeListBegin(::apache::thrift::protocol::T_STRUCT, this->cells.size());
      std::vector<Hypertable::ThriftGen::Cell> ::const_iterator _iter11;
      for (_iter11 = this->cells.begin(); _iter11 != this->cells.end(); ++_iter11)
      {
        xfer += (*_iter11).write(oprot);
      }
      xfer += oprot->writeListEnd();
    }
    xfer += oprot->writeFieldEnd();
  }
  if (this->__isset.scanner) {
    xfer += oprot->writeFieldBegin("scanner", ::apache::thrift::protocol::T_I64, 3);
    xfer += oprot->writeI64(this->scanner);
    xfer += oprot->writeFieldEnd();
  }
  if (this->__isset.mutator) {
    xfer += oprot->writeFieldBegin("mutator", ::apache::thrift::protocol::T_I64, 4);
    xfer += oprot->writeI64(this->mutator);
    xfer += oprot->writeFieldEnd();
  }
  xfer += oprot->writeFieldStop();
  xfer += oprot->writeStructEnd();
  return xfer;
}

const char* HqlResult2::ascii_fingerprint = "42D931B628867DE07C085E581BA7AE1F";
const uint8_t HqlResult2::binary_fingerprint[16] = {0x42,0xD9,0x31,0xB6,0x28,0x86,0x7D,0xE0,0x7C,0x08,0x5E,0x58,0x1B,0xA7,0xAE,0x1F};

uint32_t HqlResult2::read(::apache::thrift::protocol::TProtocol* iprot) {

  uint32_t xfer = 0;
  std::string fname;
  ::apache::thrift::protocol::TType ftype;
  int16_t fid;

  xfer += iprot->readStructBegin(fname);

  using ::apache::thrift::protocol::TProtocolException;


  while (true)
  {
    xfer += iprot->readFieldBegin(fname, ftype, fid);
    if (ftype == ::apache::thrift::protocol::T_STOP) {
      break;
    }
    switch (fid)
    {
      case 1:
        if (ftype == ::apache::thrift::protocol::T_LIST) {
          {
            this->results.clear();
            uint32_t _size12;
            ::apache::thrift::protocol::TType _etype15;
            iprot->readListBegin(_etype15, _size12);
            this->results.resize(_size12);
            uint32_t _i16;
            for (_i16 = 0; _i16 < _size12; ++_i16)
            {
              xfer += iprot->readString(this->results[_i16]);
            }
            iprot->readListEnd();
          }
          this->__isset.results = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      case 2:
        if (ftype == ::apache::thrift::protocol::T_LIST) {
          {
            this->cells.clear();
            uint32_t _size17;
            ::apache::thrift::protocol::TType _etype20;
            iprot->readListBegin(_etype20, _size17);
            this->cells.resize(_size17);
            uint32_t _i21;
            for (_i21 = 0; _i21 < _size17; ++_i21)
            {
              {
                this->cells[_i21].clear();
                uint32_t _size22;
                ::apache::thrift::protocol::TType _etype25;
                iprot->readListBegin(_etype25, _size22);
                this->cells[_i21].resize(_size22);
                uint32_t _i26;
                for (_i26 = 0; _i26 < _size22; ++_i26)
                {
                  xfer += iprot->readString(this->cells[_i21][_i26]);
                }
                iprot->readListEnd();
              }
            }
            iprot->readListEnd();
          }
          this->__isset.cells = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      case 3:
        if (ftype == ::apache::thrift::protocol::T_I64) {
          xfer += iprot->readI64(this->scanner);
          this->__isset.scanner = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      case 4:
        if (ftype == ::apache::thrift::protocol::T_I64) {
          xfer += iprot->readI64(this->mutator);
          this->__isset.mutator = true;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      default:
        xfer += iprot->skip(ftype);
        break;
    }
    xfer += iprot->readFieldEnd();
  }

  xfer += iprot->readStructEnd();

  return xfer;
}

uint32_t HqlResult2::write(::apache::thrift::protocol::TProtocol* oprot) const {
  uint32_t xfer = 0;
  xfer += oprot->writeStructBegin("HqlResult2");
  if (this->__isset.results) {
    xfer += oprot->writeFieldBegin("results", ::apache::thrift::protocol::T_LIST, 1);
    {
      xfer += oprot->writeListBegin(::apache::thrift::protocol::T_STRING, this->results.size());
      std::vector<std::string> ::const_iterator _iter27;
      for (_iter27 = this->results.begin(); _iter27 != this->results.end(); ++_iter27)
      {
        xfer += oprot->writeString((*_iter27));
      }
      xfer += oprot->writeListEnd();
    }
    xfer += oprot->writeFieldEnd();
  }
  if (this->__isset.cells) {
    xfer += oprot->writeFieldBegin("cells", ::apache::thrift::protocol::T_LIST, 2);
    {
      xfer += oprot->writeListBegin(::apache::thrift::protocol::T_LIST, this->cells.size());
      std::vector<Hypertable::ThriftGen::CellAsArray> ::const_iterator _iter28;
      for (_iter28 = this->cells.begin(); _iter28 != this->cells.end(); ++_iter28)
      {
        {
          xfer += oprot->writeListBegin(::apache::thrift::protocol::T_STRING, (*_iter28).size());
          std::vector<std::string> ::const_iterator _iter29;
          for (_iter29 = (*_iter28).begin(); _iter29 != (*_iter28).end(); ++_iter29)
          {
            xfer += oprot->writeString((*_iter29));
          }
          xfer += oprot->writeListEnd();
        }
      }
      xfer += oprot->writeListEnd();
    }
    xfer += oprot->writeFieldEnd();
  }
  if (this->__isset.scanner) {
    xfer += oprot->writeFieldBegin("scanner", ::apache::thrift::protocol::T_I64, 3);
    xfer += oprot->writeI64(this->scanner);
    xfer += oprot->writeFieldEnd();
  }
  if (this->__isset.mutator) {
    xfer += oprot->writeFieldBegin("mutator", ::apache::thrift::protocol::T_I64, 4);
    xfer += oprot->writeI64(this->mutator);
    xfer += oprot->writeFieldEnd();
  }
  xfer += oprot->writeFieldStop();
  xfer += oprot->writeStructEnd();
  return xfer;
}

}} // namespace
