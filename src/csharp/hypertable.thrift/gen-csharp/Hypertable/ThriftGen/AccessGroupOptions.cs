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

  #if !SILVERLIGHT
  [Serializable]
  #endif
  public partial class AccessGroupOptions : TBase
  {
    private short _replication;
    private int _blocksize;
    private string _compressor;
    private string _bloom_filter;
    private bool _in_memory;

    public short Replication
    {
      get
      {
        return _replication;
      }
      set
      {
        __isset.replication = true;
        this._replication = value;
      }
    }

    public int Blocksize
    {
      get
      {
        return _blocksize;
      }
      set
      {
        __isset.blocksize = true;
        this._blocksize = value;
      }
    }

    public string Compressor
    {
      get
      {
        return _compressor;
      }
      set
      {
        __isset.compressor = true;
        this._compressor = value;
      }
    }

    public string Bloom_filter
    {
      get
      {
        return _bloom_filter;
      }
      set
      {
        __isset.bloom_filter = true;
        this._bloom_filter = value;
      }
    }

    public bool In_memory
    {
      get
      {
        return _in_memory;
      }
      set
      {
        __isset.in_memory = true;
        this._in_memory = value;
      }
    }


    public Isset __isset;
    #if !SILVERLIGHT
    [Serializable]
    #endif
    public struct Isset {
      public bool replication;
      public bool blocksize;
      public bool compressor;
      public bool bloom_filter;
      public bool in_memory;
    }

    public AccessGroupOptions() {
    }

    public void Read (TProtocol iprot)
    {
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
            if (field.Type == TType.I16) {
              Replication = iprot.ReadI16();
            } else { 
              TProtocolUtil.Skip(iprot, field.Type);
            }
            break;
          case 2:
            if (field.Type == TType.I32) {
              Blocksize = iprot.ReadI32();
            } else { 
              TProtocolUtil.Skip(iprot, field.Type);
            }
            break;
          case 3:
            if (field.Type == TType.String) {
              Compressor = iprot.ReadString();
            } else { 
              TProtocolUtil.Skip(iprot, field.Type);
            }
            break;
          case 4:
            if (field.Type == TType.String) {
              Bloom_filter = iprot.ReadString();
            } else { 
              TProtocolUtil.Skip(iprot, field.Type);
            }
            break;
          case 5:
            if (field.Type == TType.Bool) {
              In_memory = iprot.ReadBool();
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
    }

    public void Write(TProtocol oprot) {
      TStruct struc = new TStruct("AccessGroupOptions");
      oprot.WriteStructBegin(struc);
      TField field = new TField();
      if (__isset.replication) {
        field.Name = "replication";
        field.Type = TType.I16;
        field.ID = 1;
        oprot.WriteFieldBegin(field);
        oprot.WriteI16(Replication);
        oprot.WriteFieldEnd();
      }
      if (__isset.blocksize) {
        field.Name = "blocksize";
        field.Type = TType.I32;
        field.ID = 2;
        oprot.WriteFieldBegin(field);
        oprot.WriteI32(Blocksize);
        oprot.WriteFieldEnd();
      }
      if (Compressor != null && __isset.compressor) {
        field.Name = "compressor";
        field.Type = TType.String;
        field.ID = 3;
        oprot.WriteFieldBegin(field);
        oprot.WriteString(Compressor);
        oprot.WriteFieldEnd();
      }
      if (Bloom_filter != null && __isset.bloom_filter) {
        field.Name = "bloom_filter";
        field.Type = TType.String;
        field.ID = 4;
        oprot.WriteFieldBegin(field);
        oprot.WriteString(Bloom_filter);
        oprot.WriteFieldEnd();
      }
      if (__isset.in_memory) {
        field.Name = "in_memory";
        field.Type = TType.Bool;
        field.ID = 5;
        oprot.WriteFieldBegin(field);
        oprot.WriteBool(In_memory);
        oprot.WriteFieldEnd();
      }
      oprot.WriteFieldStop();
      oprot.WriteStructEnd();
    }

    public override string ToString() {
      StringBuilder __sb = new StringBuilder("AccessGroupOptions(");
      bool __first = true;
      if (__isset.replication) {
        if(!__first) { __sb.Append(", "); }
        __first = false;
        __sb.Append("Replication: ");
        __sb.Append(Replication);
      }
      if (__isset.blocksize) {
        if(!__first) { __sb.Append(", "); }
        __first = false;
        __sb.Append("Blocksize: ");
        __sb.Append(Blocksize);
      }
      if (Compressor != null && __isset.compressor) {
        if(!__first) { __sb.Append(", "); }
        __first = false;
        __sb.Append("Compressor: ");
        __sb.Append(Compressor);
      }
      if (Bloom_filter != null && __isset.bloom_filter) {
        if(!__first) { __sb.Append(", "); }
        __first = false;
        __sb.Append("Bloom_filter: ");
        __sb.Append(Bloom_filter);
      }
      if (__isset.in_memory) {
        if(!__first) { __sb.Append(", "); }
        __first = false;
        __sb.Append("In_memory: ");
        __sb.Append(In_memory);
      }
      __sb.Append(")");
      return __sb.ToString();
    }

  }

}
