/**
 * Autogenerated by Thrift
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 */
#ifndef HqlService_H
#define HqlService_H

#include <TProcessor.h>
#include "hql_types.h"
#include "ClientService.h"

namespace Hypertable { namespace ThriftGen {

class HqlServiceIf : virtual public Hypertable::ThriftGen::ClientServiceIf {
 public:
  virtual ~HqlServiceIf() {}
  virtual void hql_exec(HqlResult& _return, const int64_t ns, const std::string& command, const bool noflush, const bool unbuffered) = 0;
  virtual void hql_query(HqlResult& _return, const int64_t ns, const std::string& command) = 0;
  virtual void hql_exec_as_arrays(HqlResultAsArrays& _return, const int64_t ns, const std::string& command, const bool noflush, const bool unbuffered) = 0;
  virtual void hql_exec2(HqlResult2& _return, const int64_t ns, const std::string& command, const bool noflush, const bool unbuffered) = 0;
  virtual void hql_query_as_arrays(HqlResultAsArrays& _return, const int64_t ns, const std::string& command) = 0;
  virtual void hql_query2(HqlResult2& _return, const int64_t ns, const std::string& command) = 0;
};

class HqlServiceNull : virtual public HqlServiceIf , virtual public Hypertable::ThriftGen::ClientServiceNull {
 public:
  virtual ~HqlServiceNull() {}
  void hql_exec(HqlResult& /* _return */, const int64_t /* ns */, const std::string& /* command */, const bool /* noflush */, const bool /* unbuffered */) {
    return;
  }
  void hql_query(HqlResult& /* _return */, const int64_t /* ns */, const std::string& /* command */) {
    return;
  }
  void hql_exec_as_arrays(HqlResultAsArrays& /* _return */, const int64_t /* ns */, const std::string& /* command */, const bool /* noflush */, const bool /* unbuffered */) {
    return;
  }
  void hql_exec2(HqlResult2& /* _return */, const int64_t /* ns */, const std::string& /* command */, const bool /* noflush */, const bool /* unbuffered */) {
    return;
  }
  void hql_query_as_arrays(HqlResultAsArrays& /* _return */, const int64_t /* ns */, const std::string& /* command */) {
    return;
  }
  void hql_query2(HqlResult2& /* _return */, const int64_t /* ns */, const std::string& /* command */) {
    return;
  }
};

typedef struct _HqlService_hql_exec_args__isset {
  _HqlService_hql_exec_args__isset() : ns(false), command(false), noflush(false), unbuffered(false) {}
  bool ns;
  bool command;
  bool noflush;
  bool unbuffered;
} _HqlService_hql_exec_args__isset;

class HqlService_hql_exec_args {
 public:

  HqlService_hql_exec_args() : ns(0), command(""), noflush(false), unbuffered(false) {
  }

  virtual ~HqlService_hql_exec_args() throw() {}

  int64_t ns;
  std::string command;
  bool noflush;
  bool unbuffered;

  _HqlService_hql_exec_args__isset __isset;

  bool operator == (const HqlService_hql_exec_args & rhs) const
  {
    if (!(ns == rhs.ns))
      return false;
    if (!(command == rhs.command))
      return false;
    if (!(noflush == rhs.noflush))
      return false;
    if (!(unbuffered == rhs.unbuffered))
      return false;
    return true;
  }
  bool operator != (const HqlService_hql_exec_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const HqlService_hql_exec_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class HqlService_hql_exec_pargs {
 public:


  virtual ~HqlService_hql_exec_pargs() throw() {}

  const int64_t* ns;
  const std::string* command;
  const bool* noflush;
  const bool* unbuffered;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _HqlService_hql_exec_result__isset {
  _HqlService_hql_exec_result__isset() : success(false), e(false) {}
  bool success;
  bool e;
} _HqlService_hql_exec_result__isset;

class HqlService_hql_exec_result {
 public:

  HqlService_hql_exec_result() {
  }

  virtual ~HqlService_hql_exec_result() throw() {}

  HqlResult success;
  Hypertable::ThriftGen::ClientException e;

  _HqlService_hql_exec_result__isset __isset;

  bool operator == (const HqlService_hql_exec_result & rhs) const
  {
    if (!(success == rhs.success))
      return false;
    if (!(e == rhs.e))
      return false;
    return true;
  }
  bool operator != (const HqlService_hql_exec_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const HqlService_hql_exec_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _HqlService_hql_exec_presult__isset {
  _HqlService_hql_exec_presult__isset() : success(false), e(false) {}
  bool success;
  bool e;
} _HqlService_hql_exec_presult__isset;

class HqlService_hql_exec_presult {
 public:


  virtual ~HqlService_hql_exec_presult() throw() {}

  HqlResult* success;
  Hypertable::ThriftGen::ClientException e;

  _HqlService_hql_exec_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

typedef struct _HqlService_hql_query_args__isset {
  _HqlService_hql_query_args__isset() : ns(false), command(false) {}
  bool ns;
  bool command;
} _HqlService_hql_query_args__isset;

class HqlService_hql_query_args {
 public:

  HqlService_hql_query_args() : ns(0), command("") {
  }

  virtual ~HqlService_hql_query_args() throw() {}

  int64_t ns;
  std::string command;

  _HqlService_hql_query_args__isset __isset;

  bool operator == (const HqlService_hql_query_args & rhs) const
  {
    if (!(ns == rhs.ns))
      return false;
    if (!(command == rhs.command))
      return false;
    return true;
  }
  bool operator != (const HqlService_hql_query_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const HqlService_hql_query_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class HqlService_hql_query_pargs {
 public:


  virtual ~HqlService_hql_query_pargs() throw() {}

  const int64_t* ns;
  const std::string* command;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _HqlService_hql_query_result__isset {
  _HqlService_hql_query_result__isset() : success(false), e(false) {}
  bool success;
  bool e;
} _HqlService_hql_query_result__isset;

class HqlService_hql_query_result {
 public:

  HqlService_hql_query_result() {
  }

  virtual ~HqlService_hql_query_result() throw() {}

  HqlResult success;
  Hypertable::ThriftGen::ClientException e;

  _HqlService_hql_query_result__isset __isset;

  bool operator == (const HqlService_hql_query_result & rhs) const
  {
    if (!(success == rhs.success))
      return false;
    if (!(e == rhs.e))
      return false;
    return true;
  }
  bool operator != (const HqlService_hql_query_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const HqlService_hql_query_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _HqlService_hql_query_presult__isset {
  _HqlService_hql_query_presult__isset() : success(false), e(false) {}
  bool success;
  bool e;
} _HqlService_hql_query_presult__isset;

class HqlService_hql_query_presult {
 public:


  virtual ~HqlService_hql_query_presult() throw() {}

  HqlResult* success;
  Hypertable::ThriftGen::ClientException e;

  _HqlService_hql_query_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

typedef struct _HqlService_hql_exec_as_arrays_args__isset {
  _HqlService_hql_exec_as_arrays_args__isset() : ns(false), command(false), noflush(false), unbuffered(false) {}
  bool ns;
  bool command;
  bool noflush;
  bool unbuffered;
} _HqlService_hql_exec_as_arrays_args__isset;

class HqlService_hql_exec_as_arrays_args {
 public:

  HqlService_hql_exec_as_arrays_args() : ns(0), command(""), noflush(false), unbuffered(false) {
  }

  virtual ~HqlService_hql_exec_as_arrays_args() throw() {}

  int64_t ns;
  std::string command;
  bool noflush;
  bool unbuffered;

  _HqlService_hql_exec_as_arrays_args__isset __isset;

  bool operator == (const HqlService_hql_exec_as_arrays_args & rhs) const
  {
    if (!(ns == rhs.ns))
      return false;
    if (!(command == rhs.command))
      return false;
    if (!(noflush == rhs.noflush))
      return false;
    if (!(unbuffered == rhs.unbuffered))
      return false;
    return true;
  }
  bool operator != (const HqlService_hql_exec_as_arrays_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const HqlService_hql_exec_as_arrays_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class HqlService_hql_exec_as_arrays_pargs {
 public:


  virtual ~HqlService_hql_exec_as_arrays_pargs() throw() {}

  const int64_t* ns;
  const std::string* command;
  const bool* noflush;
  const bool* unbuffered;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _HqlService_hql_exec_as_arrays_result__isset {
  _HqlService_hql_exec_as_arrays_result__isset() : success(false), e(false) {}
  bool success;
  bool e;
} _HqlService_hql_exec_as_arrays_result__isset;

class HqlService_hql_exec_as_arrays_result {
 public:

  HqlService_hql_exec_as_arrays_result() {
  }

  virtual ~HqlService_hql_exec_as_arrays_result() throw() {}

  HqlResultAsArrays success;
  Hypertable::ThriftGen::ClientException e;

  _HqlService_hql_exec_as_arrays_result__isset __isset;

  bool operator == (const HqlService_hql_exec_as_arrays_result & rhs) const
  {
    if (!(success == rhs.success))
      return false;
    if (!(e == rhs.e))
      return false;
    return true;
  }
  bool operator != (const HqlService_hql_exec_as_arrays_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const HqlService_hql_exec_as_arrays_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _HqlService_hql_exec_as_arrays_presult__isset {
  _HqlService_hql_exec_as_arrays_presult__isset() : success(false), e(false) {}
  bool success;
  bool e;
} _HqlService_hql_exec_as_arrays_presult__isset;

class HqlService_hql_exec_as_arrays_presult {
 public:


  virtual ~HqlService_hql_exec_as_arrays_presult() throw() {}

  HqlResultAsArrays* success;
  Hypertable::ThriftGen::ClientException e;

  _HqlService_hql_exec_as_arrays_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

typedef struct _HqlService_hql_exec2_args__isset {
  _HqlService_hql_exec2_args__isset() : ns(false), command(false), noflush(false), unbuffered(false) {}
  bool ns;
  bool command;
  bool noflush;
  bool unbuffered;
} _HqlService_hql_exec2_args__isset;

class HqlService_hql_exec2_args {
 public:

  HqlService_hql_exec2_args() : ns(0), command(""), noflush(false), unbuffered(false) {
  }

  virtual ~HqlService_hql_exec2_args() throw() {}

  int64_t ns;
  std::string command;
  bool noflush;
  bool unbuffered;

  _HqlService_hql_exec2_args__isset __isset;

  bool operator == (const HqlService_hql_exec2_args & rhs) const
  {
    if (!(ns == rhs.ns))
      return false;
    if (!(command == rhs.command))
      return false;
    if (!(noflush == rhs.noflush))
      return false;
    if (!(unbuffered == rhs.unbuffered))
      return false;
    return true;
  }
  bool operator != (const HqlService_hql_exec2_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const HqlService_hql_exec2_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class HqlService_hql_exec2_pargs {
 public:


  virtual ~HqlService_hql_exec2_pargs() throw() {}

  const int64_t* ns;
  const std::string* command;
  const bool* noflush;
  const bool* unbuffered;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _HqlService_hql_exec2_result__isset {
  _HqlService_hql_exec2_result__isset() : success(false), e(false) {}
  bool success;
  bool e;
} _HqlService_hql_exec2_result__isset;

class HqlService_hql_exec2_result {
 public:

  HqlService_hql_exec2_result() {
  }

  virtual ~HqlService_hql_exec2_result() throw() {}

  HqlResult2 success;
  Hypertable::ThriftGen::ClientException e;

  _HqlService_hql_exec2_result__isset __isset;

  bool operator == (const HqlService_hql_exec2_result & rhs) const
  {
    if (!(success == rhs.success))
      return false;
    if (!(e == rhs.e))
      return false;
    return true;
  }
  bool operator != (const HqlService_hql_exec2_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const HqlService_hql_exec2_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _HqlService_hql_exec2_presult__isset {
  _HqlService_hql_exec2_presult__isset() : success(false), e(false) {}
  bool success;
  bool e;
} _HqlService_hql_exec2_presult__isset;

class HqlService_hql_exec2_presult {
 public:


  virtual ~HqlService_hql_exec2_presult() throw() {}

  HqlResult2* success;
  Hypertable::ThriftGen::ClientException e;

  _HqlService_hql_exec2_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

typedef struct _HqlService_hql_query_as_arrays_args__isset {
  _HqlService_hql_query_as_arrays_args__isset() : ns(false), command(false) {}
  bool ns;
  bool command;
} _HqlService_hql_query_as_arrays_args__isset;

class HqlService_hql_query_as_arrays_args {
 public:

  HqlService_hql_query_as_arrays_args() : ns(0), command("") {
  }

  virtual ~HqlService_hql_query_as_arrays_args() throw() {}

  int64_t ns;
  std::string command;

  _HqlService_hql_query_as_arrays_args__isset __isset;

  bool operator == (const HqlService_hql_query_as_arrays_args & rhs) const
  {
    if (!(ns == rhs.ns))
      return false;
    if (!(command == rhs.command))
      return false;
    return true;
  }
  bool operator != (const HqlService_hql_query_as_arrays_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const HqlService_hql_query_as_arrays_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class HqlService_hql_query_as_arrays_pargs {
 public:


  virtual ~HqlService_hql_query_as_arrays_pargs() throw() {}

  const int64_t* ns;
  const std::string* command;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _HqlService_hql_query_as_arrays_result__isset {
  _HqlService_hql_query_as_arrays_result__isset() : success(false), e(false) {}
  bool success;
  bool e;
} _HqlService_hql_query_as_arrays_result__isset;

class HqlService_hql_query_as_arrays_result {
 public:

  HqlService_hql_query_as_arrays_result() {
  }

  virtual ~HqlService_hql_query_as_arrays_result() throw() {}

  HqlResultAsArrays success;
  Hypertable::ThriftGen::ClientException e;

  _HqlService_hql_query_as_arrays_result__isset __isset;

  bool operator == (const HqlService_hql_query_as_arrays_result & rhs) const
  {
    if (!(success == rhs.success))
      return false;
    if (!(e == rhs.e))
      return false;
    return true;
  }
  bool operator != (const HqlService_hql_query_as_arrays_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const HqlService_hql_query_as_arrays_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _HqlService_hql_query_as_arrays_presult__isset {
  _HqlService_hql_query_as_arrays_presult__isset() : success(false), e(false) {}
  bool success;
  bool e;
} _HqlService_hql_query_as_arrays_presult__isset;

class HqlService_hql_query_as_arrays_presult {
 public:


  virtual ~HqlService_hql_query_as_arrays_presult() throw() {}

  HqlResultAsArrays* success;
  Hypertable::ThriftGen::ClientException e;

  _HqlService_hql_query_as_arrays_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

typedef struct _HqlService_hql_query2_args__isset {
  _HqlService_hql_query2_args__isset() : ns(false), command(false) {}
  bool ns;
  bool command;
} _HqlService_hql_query2_args__isset;

class HqlService_hql_query2_args {
 public:

  HqlService_hql_query2_args() : ns(0), command("") {
  }

  virtual ~HqlService_hql_query2_args() throw() {}

  int64_t ns;
  std::string command;

  _HqlService_hql_query2_args__isset __isset;

  bool operator == (const HqlService_hql_query2_args & rhs) const
  {
    if (!(ns == rhs.ns))
      return false;
    if (!(command == rhs.command))
      return false;
    return true;
  }
  bool operator != (const HqlService_hql_query2_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const HqlService_hql_query2_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class HqlService_hql_query2_pargs {
 public:


  virtual ~HqlService_hql_query2_pargs() throw() {}

  const int64_t* ns;
  const std::string* command;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _HqlService_hql_query2_result__isset {
  _HqlService_hql_query2_result__isset() : success(false), e(false) {}
  bool success;
  bool e;
} _HqlService_hql_query2_result__isset;

class HqlService_hql_query2_result {
 public:

  HqlService_hql_query2_result() {
  }

  virtual ~HqlService_hql_query2_result() throw() {}

  HqlResult2 success;
  Hypertable::ThriftGen::ClientException e;

  _HqlService_hql_query2_result__isset __isset;

  bool operator == (const HqlService_hql_query2_result & rhs) const
  {
    if (!(success == rhs.success))
      return false;
    if (!(e == rhs.e))
      return false;
    return true;
  }
  bool operator != (const HqlService_hql_query2_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const HqlService_hql_query2_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _HqlService_hql_query2_presult__isset {
  _HqlService_hql_query2_presult__isset() : success(false), e(false) {}
  bool success;
  bool e;
} _HqlService_hql_query2_presult__isset;

class HqlService_hql_query2_presult {
 public:


  virtual ~HqlService_hql_query2_presult() throw() {}

  HqlResult2* success;
  Hypertable::ThriftGen::ClientException e;

  _HqlService_hql_query2_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

class HqlServiceClient : virtual public HqlServiceIf, public Hypertable::ThriftGen::ClientServiceClient {
 public:
  HqlServiceClient(boost::shared_ptr< ::apache::thrift::protocol::TProtocol> prot) :
    Hypertable::ThriftGen::ClientServiceClient(prot, prot) {}
  HqlServiceClient(boost::shared_ptr< ::apache::thrift::protocol::TProtocol> iprot, boost::shared_ptr< ::apache::thrift::protocol::TProtocol> oprot) :
    Hypertable::ThriftGen::ClientServiceClient(iprot, oprot) {}
  boost::shared_ptr< ::apache::thrift::protocol::TProtocol> getInputProtocol() {
    return piprot_;
  }
  boost::shared_ptr< ::apache::thrift::protocol::TProtocol> getOutputProtocol() {
    return poprot_;
  }
  void hql_exec(HqlResult& _return, const int64_t ns, const std::string& command, const bool noflush, const bool unbuffered);
  void send_hql_exec(const int64_t ns, const std::string& command, const bool noflush, const bool unbuffered);
  void recv_hql_exec(HqlResult& _return);
  void hql_query(HqlResult& _return, const int64_t ns, const std::string& command);
  void send_hql_query(const int64_t ns, const std::string& command);
  void recv_hql_query(HqlResult& _return);
  void hql_exec_as_arrays(HqlResultAsArrays& _return, const int64_t ns, const std::string& command, const bool noflush, const bool unbuffered);
  void send_hql_exec_as_arrays(const int64_t ns, const std::string& command, const bool noflush, const bool unbuffered);
  void recv_hql_exec_as_arrays(HqlResultAsArrays& _return);
  void hql_exec2(HqlResult2& _return, const int64_t ns, const std::string& command, const bool noflush, const bool unbuffered);
  void send_hql_exec2(const int64_t ns, const std::string& command, const bool noflush, const bool unbuffered);
  void recv_hql_exec2(HqlResult2& _return);
  void hql_query_as_arrays(HqlResultAsArrays& _return, const int64_t ns, const std::string& command);
  void send_hql_query_as_arrays(const int64_t ns, const std::string& command);
  void recv_hql_query_as_arrays(HqlResultAsArrays& _return);
  void hql_query2(HqlResult2& _return, const int64_t ns, const std::string& command);
  void send_hql_query2(const int64_t ns, const std::string& command);
  void recv_hql_query2(HqlResult2& _return);
};

class HqlServiceProcessor : virtual public ::apache::thrift::TProcessor, public Hypertable::ThriftGen::ClientServiceProcessor {
 protected:
  boost::shared_ptr<HqlServiceIf> iface_;
  virtual bool process_fn(::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, std::string& fname, int32_t seqid);
 private:
  std::map<std::string, void (HqlServiceProcessor::*)(int32_t, ::apache::thrift::protocol::TProtocol*, ::apache::thrift::protocol::TProtocol*)> processMap_;
  void process_hql_exec(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot);
  void process_hql_query(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot);
  void process_hql_exec_as_arrays(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot);
  void process_hql_exec2(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot);
  void process_hql_query_as_arrays(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot);
  void process_hql_query2(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot);
 public:
  HqlServiceProcessor(boost::shared_ptr<HqlServiceIf> iface) :
    Hypertable::ThriftGen::ClientServiceProcessor(iface),
    iface_(iface) {
    processMap_["hql_exec"] = &HqlServiceProcessor::process_hql_exec;
    processMap_["hql_query"] = &HqlServiceProcessor::process_hql_query;
    processMap_["hql_exec_as_arrays"] = &HqlServiceProcessor::process_hql_exec_as_arrays;
    processMap_["hql_exec2"] = &HqlServiceProcessor::process_hql_exec2;
    processMap_["hql_query_as_arrays"] = &HqlServiceProcessor::process_hql_query_as_arrays;
    processMap_["hql_query2"] = &HqlServiceProcessor::process_hql_query2;
  }

  virtual bool process(boost::shared_ptr< ::apache::thrift::protocol::TProtocol> piprot, boost::shared_ptr< ::apache::thrift::protocol::TProtocol> poprot);
  virtual ~HqlServiceProcessor() {}
};

class HqlServiceMultiface : virtual public HqlServiceIf, public Hypertable::ThriftGen::ClientServiceMultiface {
 public:
  HqlServiceMultiface(std::vector<boost::shared_ptr<HqlServiceIf> >& ifaces) : ifaces_(ifaces) {
    std::vector<boost::shared_ptr<HqlServiceIf> >::iterator iter;
    for (iter = ifaces.begin(); iter != ifaces.end(); ++iter) {
      Hypertable::ThriftGen::ClientServiceMultiface::add(*iter);
    }
  }
  virtual ~HqlServiceMultiface() {}
 protected:
  std::vector<boost::shared_ptr<HqlServiceIf> > ifaces_;
  HqlServiceMultiface() {}
  void add(boost::shared_ptr<HqlServiceIf> iface) {
    Hypertable::ThriftGen::ClientServiceMultiface::add(iface);
    ifaces_.push_back(iface);
  }
 public:
  void hql_exec(HqlResult& _return, const int64_t ns, const std::string& command, const bool noflush, const bool unbuffered) {
    uint32_t sz = ifaces_.size();
    for (uint32_t i = 0; i < sz; ++i) {
      if (i == sz - 1) {
        ifaces_[i]->hql_exec(_return, ns, command, noflush, unbuffered);
        return;
      } else {
        ifaces_[i]->hql_exec(_return, ns, command, noflush, unbuffered);
      }
    }
  }

  void hql_query(HqlResult& _return, const int64_t ns, const std::string& command) {
    uint32_t sz = ifaces_.size();
    for (uint32_t i = 0; i < sz; ++i) {
      if (i == sz - 1) {
        ifaces_[i]->hql_query(_return, ns, command);
        return;
      } else {
        ifaces_[i]->hql_query(_return, ns, command);
      }
    }
  }

  void hql_exec_as_arrays(HqlResultAsArrays& _return, const int64_t ns, const std::string& command, const bool noflush, const bool unbuffered) {
    uint32_t sz = ifaces_.size();
    for (uint32_t i = 0; i < sz; ++i) {
      if (i == sz - 1) {
        ifaces_[i]->hql_exec_as_arrays(_return, ns, command, noflush, unbuffered);
        return;
      } else {
        ifaces_[i]->hql_exec_as_arrays(_return, ns, command, noflush, unbuffered);
      }
    }
  }

  void hql_exec2(HqlResult2& _return, const int64_t ns, const std::string& command, const bool noflush, const bool unbuffered) {
    uint32_t sz = ifaces_.size();
    for (uint32_t i = 0; i < sz; ++i) {
      if (i == sz - 1) {
        ifaces_[i]->hql_exec2(_return, ns, command, noflush, unbuffered);
        return;
      } else {
        ifaces_[i]->hql_exec2(_return, ns, command, noflush, unbuffered);
      }
    }
  }

  void hql_query_as_arrays(HqlResultAsArrays& _return, const int64_t ns, const std::string& command) {
    uint32_t sz = ifaces_.size();
    for (uint32_t i = 0; i < sz; ++i) {
      if (i == sz - 1) {
        ifaces_[i]->hql_query_as_arrays(_return, ns, command);
        return;
      } else {
        ifaces_[i]->hql_query_as_arrays(_return, ns, command);
      }
    }
  }

  void hql_query2(HqlResult2& _return, const int64_t ns, const std::string& command) {
    uint32_t sz = ifaces_.size();
    for (uint32_t i = 0; i < sz; ++i) {
      if (i == sz - 1) {
        ifaces_[i]->hql_query2(_return, ns, command);
        return;
      } else {
        ifaces_[i]->hql_query2(_return, ns, command);
      }
    }
  }

};

}} // namespace

#endif
