/**
 * Autogenerated by Thrift Compiler (0.9.2)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
using System;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using System.IO;
using Thrift;
using Thrift.Collections;
using System.Runtime.Serialization;
using Thrift.Protocol;
using Thrift.Transport;

namespace Hypertable.ThriftGen
{

  /// <summary>
  /// Specifies options for a shared periodic mutator
  /// 
  /// <dl>
  ///   <dt>appname</dt>
  ///   <dd>String key used to share/retrieve mutator, eg: "my_ht_app"</dd>
  /// 
  ///   <dt>flush_interval</dt>
  ///   <dd>Time interval between flushes</dd>
  /// 
  ///   <dt>flags</dt>
  ///   <dd>Mutator flags</dt>
  /// </dl>
  /// </summary>
  #if !SILVERLIGHT
  [Serializable]
  #endif
  public partial class MutateSpec : TBase
  {

    public string Appname { get; set; }

    public int Flush_interval { get; set; }

    public int Flags { get; set; }

    public MutateSpec() {
      this.Appname = "";
      this.Flush_interval = 1000;
      this.Flags = 2;
    }

    public MutateSpec(string appname, int flush_interval, int flags) : this() {
      this.Appname = appname;
      this.Flush_interval = flush_interval;
      this.Flags = flags;
    }

    public void Read (TProtocol iprot)
    {
      bool isset_appname = false;
      bool isset_flush_interval = false;
      bool isset_flags = false;
      TField field;
      iprot.ReadStructBegin();
      while (true)
      {
        field = iprot.ReadFieldBegin();
        if (field.Type == TType.Stop) { 
          break;
        }
        switch (field.ID)
        {
          case 1:
            if (field.Type == TType.String) {
              Appname = iprot.ReadString();
              isset_appname = true;
            } else { 
              TProtocolUtil.Skip(iprot, field.Type);
            }
            break;
          case 2:
            if (field.Type == TType.I32) {
              Flush_interval = iprot.ReadI32();
              isset_flush_interval = true;
            } else { 
              TProtocolUtil.Skip(iprot, field.Type);
            }
            break;
          case 3:
            if (field.Type == TType.I32) {
              Flags = iprot.ReadI32();
              isset_flags = true;
            } else { 
              TProtocolUtil.Skip(iprot, field.Type);
            }
            break;
          default: 
            TProtocolUtil.Skip(iprot, field.Type);
            break;
        }
        iprot.ReadFieldEnd();
      }
      iprot.ReadStructEnd();
      if (!isset_appname)
        throw new TProtocolException(TProtocolException.INVALID_DATA);
      if (!isset_flush_interval)
        throw new TProtocolException(TProtocolException.INVALID_DATA);
      if (!isset_flags)
        throw new TProtocolException(TProtocolException.INVALID_DATA);
    }

    public void Write(TProtocol oprot) {
      TStruct struc = new TStruct("MutateSpec");
      oprot.WriteStructBegin(struc);
      TField field = new TField();
      field.Name = "appname";
      field.Type = TType.String;
      field.ID = 1;
      oprot.WriteFieldBegin(field);
      oprot.WriteString(Appname);
      oprot.WriteFieldEnd();
      field.Name = "flush_interval";
      field.Type = TType.I32;
      field.ID = 2;
      oprot.WriteFieldBegin(field);
      oprot.WriteI32(Flush_interval);
      oprot.WriteFieldEnd();
      field.Name = "flags";
      field.Type = TType.I32;
      field.ID = 3;
      oprot.WriteFieldBegin(field);
      oprot.WriteI32(Flags);
      oprot.WriteFieldEnd();
      oprot.WriteFieldStop();
      oprot.WriteStructEnd();
    }

    public override string ToString() {
      StringBuilder __sb = new StringBuilder("MutateSpec(");
      __sb.Append(", Appname: ");
      __sb.Append(Appname);
      __sb.Append(", Flush_interval: ");
      __sb.Append(Flush_interval);
      __sb.Append(", Flags: ");
      __sb.Append(Flags);
      __sb.Append(")");
      return __sb.ToString();
    }

  }

}
