/**
 * Autogenerated by Thrift Compiler (0.9.3)
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
  /// Specifies a range of cells
  /// 
  /// <dl>
  ///   <dt>start_row</dt>
  ///   <dd>The row to start scan with. Must not contain nulls (0x00)</dd>
  /// 
  ///   <dt>start_column</dt>
  ///   <dd>The column (prefix of column_family:column_qualifier) of the
  ///   start row for the scan</dd>
  /// 
  ///   <dt>start_inclusive</dt>
  ///   <dd>Whether the start row is included in the result (default: true)</dd>
  /// 
  ///   <dt>end_row</dt>
  ///   <dd>The row to end scan with. Must not contain nulls</dd>
  /// 
  ///   <dt>end_column</dt>
  ///   <dd>The column (prefix of column_family:column_qualifier) of the
  ///   end row for the scan</dd>
  /// 
  ///   <dt>end_inclusive</dt>
  ///   <dd>Whether the end row is included in the result (default: true)</dd>
  /// </dl>
  /// </summary>
  #if !SILVERLIGHT
  [Serializable]
  #endif
  public partial class CellInterval : TBase
  {
    private string _start_row;
    private string _start_column;
    private bool _start_inclusive;
    private string _end_row;
    private string _end_column;
    private bool _end_inclusive;

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

    public string Start_column
    {
      get
      {
        return _start_column;
      }
      set
      {
        __isset.start_column = true;
        this._start_column = value;
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

    public string End_column
    {
      get
      {
        return _end_column;
      }
      set
      {
        __isset.end_column = true;
        this._end_column = value;
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


    public Isset __isset;
    #if !SILVERLIGHT
    [Serializable]
    #endif
    public struct Isset {
      public bool start_row;
      public bool start_column;
      public bool start_inclusive;
      public bool end_row;
      public bool end_column;
      public bool end_inclusive;
    }

    public CellInterval() {
      this._start_inclusive = true;
      this.__isset.start_inclusive = true;
      this._end_inclusive = true;
      this.__isset.end_inclusive = true;
    }

    public void Read (TProtocol iprot)
    {
      iprot.IncrementRecursionDepth();
      try
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
              if (field.Type == TType.String) {
                Start_column = iprot.ReadString();
              } else { 
                TProtocolUtil.Skip(iprot, field.Type);
              }
              break;
            case 3:
              if (field.Type == TType.Bool) {
                Start_inclusive = iprot.ReadBool();
              } else { 
                TProtocolUtil.Skip(iprot, field.Type);
              }
              break;
            case 4:
              if (field.Type == TType.String) {
                End_row = iprot.ReadString();
              } else { 
                TProtocolUtil.Skip(iprot, field.Type);
              }
              break;
            case 5:
              if (field.Type == TType.String) {
                End_column = iprot.ReadString();
              } else { 
                TProtocolUtil.Skip(iprot, field.Type);
              }
              break;
            case 6:
              if (field.Type == TType.Bool) {
                End_inclusive = iprot.ReadBool();
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
      finally
      {
        iprot.DecrementRecursionDepth();
      }
    }

    public void Write(TProtocol oprot) {
      oprot.IncrementRecursionDepth();
      try
      {
        TStruct struc = new TStruct("CellInterval");
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
        if (Start_column != null && __isset.start_column) {
          field.Name = "start_column";
          field.Type = TType.String;
          field.ID = 2;
          oprot.WriteFieldBegin(field);
          oprot.WriteString(Start_column);
          oprot.WriteFieldEnd();
        }
        if (__isset.start_inclusive) {
          field.Name = "start_inclusive";
          field.Type = TType.Bool;
          field.ID = 3;
          oprot.WriteFieldBegin(field);
          oprot.WriteBool(Start_inclusive);
          oprot.WriteFieldEnd();
        }
        if (End_row != null && __isset.end_row) {
          field.Name = "end_row";
          field.Type = TType.String;
          field.ID = 4;
          oprot.WriteFieldBegin(field);
          oprot.WriteString(End_row);
          oprot.WriteFieldEnd();
        }
        if (End_column != null && __isset.end_column) {
          field.Name = "end_column";
          field.Type = TType.String;
          field.ID = 5;
          oprot.WriteFieldBegin(field);
          oprot.WriteString(End_column);
          oprot.WriteFieldEnd();
        }
        if (__isset.end_inclusive) {
          field.Name = "end_inclusive";
          field.Type = TType.Bool;
          field.ID = 6;
          oprot.WriteFieldBegin(field);
          oprot.WriteBool(End_inclusive);
          oprot.WriteFieldEnd();
        }
        oprot.WriteFieldStop();
        oprot.WriteStructEnd();
      }
      finally
      {
        oprot.DecrementRecursionDepth();
      }
    }

    public override string ToString() {
      StringBuilder __sb = new StringBuilder("CellInterval(");
      bool __first = true;
      if (Start_row != null && __isset.start_row) {
        if(!__first) { __sb.Append(", "); }
        __first = false;
        __sb.Append("Start_row: ");
        __sb.Append(Start_row);
      }
      if (Start_column != null && __isset.start_column) {
        if(!__first) { __sb.Append(", "); }
        __first = false;
        __sb.Append("Start_column: ");
        __sb.Append(Start_column);
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
      if (End_column != null && __isset.end_column) {
        if(!__first) { __sb.Append(", "); }
        __first = false;
        __sb.Append("End_column: ");
        __sb.Append(End_column);
      }
      if (__isset.end_inclusive) {
        if(!__first) { __sb.Append(", "); }
        __first = false;
        __sb.Append("End_inclusive: ");
        __sb.Append(End_inclusive);
      }
      __sb.Append(")");
      return __sb.ToString();
    }

  }

}
