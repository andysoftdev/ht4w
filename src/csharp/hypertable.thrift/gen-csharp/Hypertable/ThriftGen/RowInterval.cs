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
  /// Specifies a range of rows
  /// 
  /// <dl>
  ///   <dt>start_row</dt>
  ///   <dd>The row to start scan with. Must not contain nulls (0x00)</dd>
  /// 
  ///   <dt>start_inclusive</dt>
  ///   <dd>Whether the start row is included in the result (default: true)</dd>
  /// 
  ///   <dt>end_row</dt>
  ///   <dd>The row to end scan with. Must not contain nulls</dd>
  /// 
  ///   <dt>end_inclusive</dt>
  ///   <dd>Whether the end row is included in the result (default: true)</dd>
  /// </dl>
  /// </summary>
  #if !SILVERLIGHT
  [Serializable]
  #endif
  public partial class RowInterval : TBase
  {
    private string _start_row;
    private bool _start_inclusive;
    private string _end_row;
    private bool _end_inclusive;
    private byte[] _start_row_binary;
    private byte[] _end_row_binary;

    public string Start_row
    {
      get
      {
        return _start_row;
      }
      set
      {
        __isset.start_row = true;
        this._start_row = value;
      }
    }

    public bool Start_inclusive
    {
      get
      {
        return _start_inclusive;
      }
      set
      {
        __isset.start_inclusive = true;
        this._start_inclusive = value;
      }
    }

    public string End_row
    {
      get
      {
        return _end_row;
      }
      set
      {
        __isset.end_row = true;
        this._end_row = value;
      }
    }

    public bool End_inclusive
    {
      get
      {
        return _end_inclusive;
      }
      set
      {
        __isset.end_inclusive = true;
        this._end_inclusive = value;
      }
    }

    public byte[] Start_row_binary
    {
      get
      {
        return _start_row_binary;
      }
      set
      {
        __isset.start_row_binary = true;
        this._start_row_binary = value;
      }
    }

    public byte[] End_row_binary
    {
      get
      {
        return _end_row_binary;
      }
      set
      {
        __isset.end_row_binary = true;
        this._end_row_binary = value;
      }
    }


    public Isset __isset;
    #if !SILVERLIGHT
    [Serializable]
    #endif
    public struct Isset {
      public bool start_row;
      public bool start_inclusive;
      public bool end_row;
      public bool end_inclusive;
      public bool start_row_binary;
      public bool end_row_binary;
    }

    public RowInterval() {
      this._start_inclusive = true;
      this.__isset.start_inclusive = true;
      this._end_inclusive = true;
      this.__isset.end_inclusive = true;
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
            if (field.Type == TType.String) {
              Start_row = iprot.ReadString();
            } else { 
              TProtocolUtil.Skip(iprot, field.Type);
            }
            break;
          case 2:
            if (field.Type == TType.Bool) {
              Start_inclusive = iprot.ReadBool();
            } else { 
              TProtocolUtil.Skip(iprot, field.Type);
            }
            break;
          case 3:
            if (field.Type == TType.String) {
              End_row = iprot.ReadString();
            } else { 
              TProtocolUtil.Skip(iprot, field.Type);
            }
            break;
          case 4:
            if (field.Type == TType.Bool) {
              End_inclusive = iprot.ReadBool();
            } else { 
              TProtocolUtil.Skip(iprot, field.Type);
            }
            break;
          case 5:
            if (field.Type == TType.String) {
              Start_row_binary = iprot.ReadBinary();
            } else { 
              TProtocolUtil.Skip(iprot, field.Type);
            }
            break;
          case 6:
            if (field.Type == TType.String) {
              End_row_binary = iprot.ReadBinary();
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
      TStruct struc = new TStruct("RowInterval");
      oprot.WriteStructBegin(struc);
      TField field = new TField();
      if (Start_row != null && __isset.start_row) {
        field.Name = "start_row";
        field.Type = TType.String;
        field.ID = 1;
        oprot.WriteFieldBegin(field);
        oprot.WriteString(Start_row);
        oprot.WriteFieldEnd();
      }
      if (__isset.start_inclusive) {
        field.Name = "start_inclusive";
        field.Type = TType.Bool;
        field.ID = 2;
        oprot.WriteFieldBegin(field);
        oprot.WriteBool(Start_inclusive);
        oprot.WriteFieldEnd();
      }
      if (End_row != null && __isset.end_row) {
        field.Name = "end_row";
        field.Type = TType.String;
        field.ID = 3;
        oprot.WriteFieldBegin(field);
        oprot.WriteString(End_row);
        oprot.WriteFieldEnd();
      }
      if (__isset.end_inclusive) {
        field.Name = "end_inclusive";
        field.Type = TType.Bool;
        field.ID = 4;
        oprot.WriteFieldBegin(field);
        oprot.WriteBool(End_inclusive);
        oprot.WriteFieldEnd();
      }
      if (Start_row_binary != null && __isset.start_row_binary) {
        field.Name = "start_row_binary";
        field.Type = TType.String;
        field.ID = 5;
        oprot.WriteFieldBegin(field);
        oprot.WriteBinary(Start_row_binary);
        oprot.WriteFieldEnd();
      }
      if (End_row_binary != null && __isset.end_row_binary) {
        field.Name = "end_row_binary";
        field.Type = TType.String;
        field.ID = 6;
        oprot.WriteFieldBegin(field);
        oprot.WriteBinary(End_row_binary);
        oprot.WriteFieldEnd();
      }
      oprot.WriteFieldStop();
      oprot.WriteStructEnd();
    }

    public override string ToString() {
      StringBuilder __sb = new StringBuilder("RowInterval(");
      bool __first = true;
      if (Start_row != null && __isset.start_row) {
        if(!__first) { __sb.Append(", "); }
        __first = false;
        __sb.Append("Start_row: ");
        __sb.Append(Start_row);
      }
      if (__isset.start_inclusive) {
        if(!__first) { __sb.Append(", "); }
        __first = false;
        __sb.Append("Start_inclusive: ");
        __sb.Append(Start_inclusive);
      }
      if (End_row != null && __isset.end_row) {
        if(!__first) { __sb.Append(", "); }
        __first = false;
        __sb.Append("End_row: ");
        __sb.Append(End_row);
      }
      if (__isset.end_inclusive) {
        if(!__first) { __sb.Append(", "); }
        __first = false;
        __sb.Append("End_inclusive: ");
        __sb.Append(End_inclusive);
      }
      if (Start_row_binary != null && __isset.start_row_binary) {
        if(!__first) { __sb.Append(", "); }
        __first = false;
        __sb.Append("Start_row_binary: ");
        __sb.Append(Start_row_binary);
      }
      if (End_row_binary != null && __isset.end_row_binary) {
        if(!__first) { __sb.Append(", "); }
        __first = false;
        __sb.Append("End_row_binary: ");
        __sb.Append(End_row_binary);
      }
      __sb.Append(")");
      return __sb.ToString();
    }

  }

}