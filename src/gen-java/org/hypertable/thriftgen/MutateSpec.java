/**
 * Autogenerated by Thrift Compiler (1.0.0-dev)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
package org.hypertable.thriftgen;

import org.apache.thrift.scheme.IScheme;
import org.apache.thrift.scheme.SchemeFactory;
import org.apache.thrift.scheme.StandardScheme;

import org.apache.thrift.scheme.TupleScheme;
import org.apache.thrift.protocol.TTupleProtocol;
import org.apache.thrift.protocol.TProtocolException;
import org.apache.thrift.EncodingUtils;
import org.apache.thrift.TException;
import org.apache.thrift.async.AsyncMethodCallback;
import org.apache.thrift.server.AbstractNonblockingServer.*;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;
import java.util.EnumMap;
import java.util.Set;
import java.util.HashSet;
import java.util.EnumSet;
import java.util.Collections;
import java.util.BitSet;
import java.nio.ByteBuffer;
import java.util.Arrays;
import javax.annotation.Generated;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

@SuppressWarnings({"cast", "rawtypes", "serial", "unchecked"})
/**
 * Specifies options for a shared periodic mutator
 * 
 * <dl>
 *   <dt>appname</dt>
 *   <dd>String key used to share/retrieve mutator, eg: "my_ht_app"</dd>
 * 
 *   <dt>flush_interval</dt>
 *   <dd>Time interval between flushes</dd>
 * 
 *   <dt>flags</dt>
 *   <dd>Mutator flags</dt>
 * </dl>
 */
@Generated(value = "Autogenerated by Thrift Compiler (1.0.0-dev)", date = "2014-6-17")
public class MutateSpec implements org.apache.thrift.TBase<MutateSpec, MutateSpec._Fields>, java.io.Serializable, Cloneable, Comparable<MutateSpec> {
  private static final org.apache.thrift.protocol.TStruct STRUCT_DESC = new org.apache.thrift.protocol.TStruct("MutateSpec");

  private static final org.apache.thrift.protocol.TField APPNAME_FIELD_DESC = new org.apache.thrift.protocol.TField("appname", org.apache.thrift.protocol.TType.STRING, (short)1);
  private static final org.apache.thrift.protocol.TField FLUSH_INTERVAL_FIELD_DESC = new org.apache.thrift.protocol.TField("flush_interval", org.apache.thrift.protocol.TType.I32, (short)2);
  private static final org.apache.thrift.protocol.TField FLAGS_FIELD_DESC = new org.apache.thrift.protocol.TField("flags", org.apache.thrift.protocol.TType.I32, (short)3);

  private static final Map<Class<? extends IScheme>, SchemeFactory> schemes = new HashMap<Class<? extends IScheme>, SchemeFactory>();
  static {
    schemes.put(StandardScheme.class, new MutateSpecStandardSchemeFactory());
    schemes.put(TupleScheme.class, new MutateSpecTupleSchemeFactory());
  }

  public String appname; // required
  public int flush_interval; // required
  public int flags; // required

  /** The set of fields this struct contains, along with convenience methods for finding and manipulating them. */
  public enum _Fields implements org.apache.thrift.TFieldIdEnum {
    APPNAME((short)1, "appname"),
    FLUSH_INTERVAL((short)2, "flush_interval"),
    FLAGS((short)3, "flags");

    private static final Map<String, _Fields> byName = new HashMap<String, _Fields>();

    static {
      for (_Fields field : EnumSet.allOf(_Fields.class)) {
        byName.put(field.getFieldName(), field);
      }
    }

    /**
     * Find the _Fields constant that matches fieldId, or null if its not found.
     */
    public static _Fields findByThriftId(int fieldId) {
      switch(fieldId) {
        case 1: // APPNAME
          return APPNAME;
        case 2: // FLUSH_INTERVAL
          return FLUSH_INTERVAL;
        case 3: // FLAGS
          return FLAGS;
        default:
          return null;
      }
    }

    /**
     * Find the _Fields constant that matches fieldId, throwing an exception
     * if it is not found.
     */
    public static _Fields findByThriftIdOrThrow(int fieldId) {
      _Fields fields = findByThriftId(fieldId);
      if (fields == null) throw new IllegalArgumentException("Field " + fieldId + " doesn't exist!");
      return fields;
    }

    /**
     * Find the _Fields constant that matches name, or null if its not found.
     */
    public static _Fields findByName(String name) {
      return byName.get(name);
    }

    private final short _thriftId;
    private final String _fieldName;

    _Fields(short thriftId, String fieldName) {
      _thriftId = thriftId;
      _fieldName = fieldName;
    }

    public short getThriftFieldId() {
      return _thriftId;
    }

    public String getFieldName() {
      return _fieldName;
    }
  }

  // isset id assignments
  private static final int __FLUSH_INTERVAL_ISSET_ID = 0;
  private static final int __FLAGS_ISSET_ID = 1;
  private byte __isset_bitfield = 0;
  public static final Map<_Fields, org.apache.thrift.meta_data.FieldMetaData> metaDataMap;
  static {
    Map<_Fields, org.apache.thrift.meta_data.FieldMetaData> tmpMap = new EnumMap<_Fields, org.apache.thrift.meta_data.FieldMetaData>(_Fields.class);
    tmpMap.put(_Fields.APPNAME, new org.apache.thrift.meta_data.FieldMetaData("appname", org.apache.thrift.TFieldRequirementType.REQUIRED, 
        new org.apache.thrift.meta_data.FieldValueMetaData(org.apache.thrift.protocol.TType.STRING)));
    tmpMap.put(_Fields.FLUSH_INTERVAL, new org.apache.thrift.meta_data.FieldMetaData("flush_interval", org.apache.thrift.TFieldRequirementType.REQUIRED, 
        new org.apache.thrift.meta_data.FieldValueMetaData(org.apache.thrift.protocol.TType.I32)));
    tmpMap.put(_Fields.FLAGS, new org.apache.thrift.meta_data.FieldMetaData("flags", org.apache.thrift.TFieldRequirementType.REQUIRED, 
        new org.apache.thrift.meta_data.FieldValueMetaData(org.apache.thrift.protocol.TType.I32)));
    metaDataMap = Collections.unmodifiableMap(tmpMap);
    org.apache.thrift.meta_data.FieldMetaData.addStructMetaDataMap(MutateSpec.class, metaDataMap);
  }

  public MutateSpec() {
    this.appname = "";

    this.flush_interval = 1000;

    this.flags = 2;

  }

  public MutateSpec(
    String appname,
    int flush_interval,
    int flags)
  {
    this();
    this.appname = appname;
    this.flush_interval = flush_interval;
    setFlush_intervalIsSet(true);
    this.flags = flags;
    setFlagsIsSet(true);
  }

  /**
   * Performs a deep copy on <i>other</i>.
   */
  public MutateSpec(MutateSpec other) {
    __isset_bitfield = other.__isset_bitfield;
    if (other.isSetAppname()) {
      this.appname = other.appname;
    }
    this.flush_interval = other.flush_interval;
    this.flags = other.flags;
  }

  public MutateSpec deepCopy() {
    return new MutateSpec(this);
  }

  @Override
  public void clear() {
    this.appname = "";

    this.flush_interval = 1000;

    this.flags = 2;

  }

  public String getAppname() {
    return this.appname;
  }

  public MutateSpec setAppname(String appname) {
    this.appname = appname;
    return this;
  }

  public void unsetAppname() {
    this.appname = null;
  }

  /** Returns true if field appname is set (has been assigned a value) and false otherwise */
  public boolean isSetAppname() {
    return this.appname != null;
  }

  public void setAppnameIsSet(boolean value) {
    if (!value) {
      this.appname = null;
    }
  }

  public int getFlush_interval() {
    return this.flush_interval;
  }

  public MutateSpec setFlush_interval(int flush_interval) {
    this.flush_interval = flush_interval;
    setFlush_intervalIsSet(true);
    return this;
  }

  public void unsetFlush_interval() {
    __isset_bitfield = EncodingUtils.clearBit(__isset_bitfield, __FLUSH_INTERVAL_ISSET_ID);
  }

  /** Returns true if field flush_interval is set (has been assigned a value) and false otherwise */
  public boolean isSetFlush_interval() {
    return EncodingUtils.testBit(__isset_bitfield, __FLUSH_INTERVAL_ISSET_ID);
  }

  public void setFlush_intervalIsSet(boolean value) {
    __isset_bitfield = EncodingUtils.setBit(__isset_bitfield, __FLUSH_INTERVAL_ISSET_ID, value);
  }

  public int getFlags() {
    return this.flags;
  }

  public MutateSpec setFlags(int flags) {
    this.flags = flags;
    setFlagsIsSet(true);
    return this;
  }

  public void unsetFlags() {
    __isset_bitfield = EncodingUtils.clearBit(__isset_bitfield, __FLAGS_ISSET_ID);
  }

  /** Returns true if field flags is set (has been assigned a value) and false otherwise */
  public boolean isSetFlags() {
    return EncodingUtils.testBit(__isset_bitfield, __FLAGS_ISSET_ID);
  }

  public void setFlagsIsSet(boolean value) {
    __isset_bitfield = EncodingUtils.setBit(__isset_bitfield, __FLAGS_ISSET_ID, value);
  }

  public void setFieldValue(_Fields field, Object value) {
    switch (field) {
    case APPNAME:
      if (value == null) {
        unsetAppname();
      } else {
        setAppname((String)value);
      }
      break;

    case FLUSH_INTERVAL:
      if (value == null) {
        unsetFlush_interval();
      } else {
        setFlush_interval((Integer)value);
      }
      break;

    case FLAGS:
      if (value == null) {
        unsetFlags();
      } else {
        setFlags((Integer)value);
      }
      break;

    }
  }

  public Object getFieldValue(_Fields field) {
    switch (field) {
    case APPNAME:
      return getAppname();

    case FLUSH_INTERVAL:
      return Integer.valueOf(getFlush_interval());

    case FLAGS:
      return Integer.valueOf(getFlags());

    }
    throw new IllegalStateException();
  }

  /** Returns true if field corresponding to fieldID is set (has been assigned a value) and false otherwise */
  public boolean isSet(_Fields field) {
    if (field == null) {
      throw new IllegalArgumentException();
    }

    switch (field) {
    case APPNAME:
      return isSetAppname();
    case FLUSH_INTERVAL:
      return isSetFlush_interval();
    case FLAGS:
      return isSetFlags();
    }
    throw new IllegalStateException();
  }

  @Override
  public boolean equals(Object that) {
    if (that == null)
      return false;
    if (that instanceof MutateSpec)
      return this.equals((MutateSpec)that);
    return false;
  }

  public boolean equals(MutateSpec that) {
    if (that == null)
      return false;

    boolean this_present_appname = true && this.isSetAppname();
    boolean that_present_appname = true && that.isSetAppname();
    if (this_present_appname || that_present_appname) {
      if (!(this_present_appname && that_present_appname))
        return false;
      if (!this.appname.equals(that.appname))
        return false;
    }

    boolean this_present_flush_interval = true;
    boolean that_present_flush_interval = true;
    if (this_present_flush_interval || that_present_flush_interval) {
      if (!(this_present_flush_interval && that_present_flush_interval))
        return false;
      if (this.flush_interval != that.flush_interval)
        return false;
    }

    boolean this_present_flags = true;
    boolean that_present_flags = true;
    if (this_present_flags || that_present_flags) {
      if (!(this_present_flags && that_present_flags))
        return false;
      if (this.flags != that.flags)
        return false;
    }

    return true;
  }

  @Override
  public int hashCode() {
    List<Object> list = new ArrayList<Object>();

    boolean present_appname = true && (isSetAppname());
    list.add(present_appname);
    if (present_appname)
      list.add(appname);

    boolean present_flush_interval = true;
    list.add(present_flush_interval);
    if (present_flush_interval)
      list.add(flush_interval);

    boolean present_flags = true;
    list.add(present_flags);
    if (present_flags)
      list.add(flags);

    return list.hashCode();
  }

  @Override
  public int compareTo(MutateSpec other) {
    if (!getClass().equals(other.getClass())) {
      return getClass().getName().compareTo(other.getClass().getName());
    }

    int lastComparison = 0;

    lastComparison = Boolean.valueOf(isSetAppname()).compareTo(other.isSetAppname());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetAppname()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.appname, other.appname);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    lastComparison = Boolean.valueOf(isSetFlush_interval()).compareTo(other.isSetFlush_interval());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetFlush_interval()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.flush_interval, other.flush_interval);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    lastComparison = Boolean.valueOf(isSetFlags()).compareTo(other.isSetFlags());
    if (lastComparison != 0) {
      return lastComparison;
    }
    if (isSetFlags()) {
      lastComparison = org.apache.thrift.TBaseHelper.compareTo(this.flags, other.flags);
      if (lastComparison != 0) {
        return lastComparison;
      }
    }
    return 0;
  }

  public _Fields fieldForId(int fieldId) {
    return _Fields.findByThriftId(fieldId);
  }

  public void read(org.apache.thrift.protocol.TProtocol iprot) throws org.apache.thrift.TException {
    schemes.get(iprot.getScheme()).getScheme().read(iprot, this);
  }

  public void write(org.apache.thrift.protocol.TProtocol oprot) throws org.apache.thrift.TException {
    schemes.get(oprot.getScheme()).getScheme().write(oprot, this);
  }

  @Override
  public String toString() {
    StringBuilder sb = new StringBuilder("MutateSpec(");
    boolean first = true;

    sb.append("appname:");
    if (this.appname == null) {
      sb.append("null");
    } else {
      sb.append(this.appname);
    }
    first = false;
    if (!first) sb.append(", ");
    sb.append("flush_interval:");
    sb.append(this.flush_interval);
    first = false;
    if (!first) sb.append(", ");
    sb.append("flags:");
    sb.append(this.flags);
    first = false;
    sb.append(")");
    return sb.toString();
  }

  public void validate() throws org.apache.thrift.TException {
    // check for required fields
    if (appname == null) {
      throw new org.apache.thrift.protocol.TProtocolException("Required field 'appname' was not present! Struct: " + toString());
    }
    // alas, we cannot check 'flush_interval' because it's a primitive and you chose the non-beans generator.
    // alas, we cannot check 'flags' because it's a primitive and you chose the non-beans generator.
    // check for sub-struct validity
  }

  private void writeObject(java.io.ObjectOutputStream out) throws java.io.IOException {
    try {
      write(new org.apache.thrift.protocol.TCompactProtocol(new org.apache.thrift.transport.TIOStreamTransport(out)));
    } catch (org.apache.thrift.TException te) {
      throw new java.io.IOException(te);
    }
  }

  private void readObject(java.io.ObjectInputStream in) throws java.io.IOException, ClassNotFoundException {
    try {
      // it doesn't seem like you should have to do this, but java serialization is wacky, and doesn't call the default constructor.
      __isset_bitfield = 0;
      read(new org.apache.thrift.protocol.TCompactProtocol(new org.apache.thrift.transport.TIOStreamTransport(in)));
    } catch (org.apache.thrift.TException te) {
      throw new java.io.IOException(te);
    }
  }

  private static class MutateSpecStandardSchemeFactory implements SchemeFactory {
    public MutateSpecStandardScheme getScheme() {
      return new MutateSpecStandardScheme();
    }
  }

  private static class MutateSpecStandardScheme extends StandardScheme<MutateSpec> {

    public void read(org.apache.thrift.protocol.TProtocol iprot, MutateSpec struct) throws org.apache.thrift.TException {
      org.apache.thrift.protocol.TField schemeField;
      iprot.readStructBegin();
      while (true)
      {
        schemeField = iprot.readFieldBegin();
        if (schemeField.type == org.apache.thrift.protocol.TType.STOP) { 
          break;
        }
        switch (schemeField.id) {
          case 1: // APPNAME
            if (schemeField.type == org.apache.thrift.protocol.TType.STRING) {
              struct.appname = iprot.readString();
              struct.setAppnameIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          case 2: // FLUSH_INTERVAL
            if (schemeField.type == org.apache.thrift.protocol.TType.I32) {
              struct.flush_interval = iprot.readI32();
              struct.setFlush_intervalIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          case 3: // FLAGS
            if (schemeField.type == org.apache.thrift.protocol.TType.I32) {
              struct.flags = iprot.readI32();
              struct.setFlagsIsSet(true);
            } else { 
              org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
            }
            break;
          default:
            org.apache.thrift.protocol.TProtocolUtil.skip(iprot, schemeField.type);
        }
        iprot.readFieldEnd();
      }
      iprot.readStructEnd();

      // check for required fields of primitive type, which can't be checked in the validate method
      if (!struct.isSetFlush_interval()) {
        throw new org.apache.thrift.protocol.TProtocolException("Required field 'flush_interval' was not found in serialized data! Struct: " + toString());
      }
      if (!struct.isSetFlags()) {
        throw new org.apache.thrift.protocol.TProtocolException("Required field 'flags' was not found in serialized data! Struct: " + toString());
      }
      struct.validate();
    }

    public void write(org.apache.thrift.protocol.TProtocol oprot, MutateSpec struct) throws org.apache.thrift.TException {
      struct.validate();

      oprot.writeStructBegin(STRUCT_DESC);
      if (struct.appname != null) {
        oprot.writeFieldBegin(APPNAME_FIELD_DESC);
        oprot.writeString(struct.appname);
        oprot.writeFieldEnd();
      }
      oprot.writeFieldBegin(FLUSH_INTERVAL_FIELD_DESC);
      oprot.writeI32(struct.flush_interval);
      oprot.writeFieldEnd();
      oprot.writeFieldBegin(FLAGS_FIELD_DESC);
      oprot.writeI32(struct.flags);
      oprot.writeFieldEnd();
      oprot.writeFieldStop();
      oprot.writeStructEnd();
    }

  }

  private static class MutateSpecTupleSchemeFactory implements SchemeFactory {
    public MutateSpecTupleScheme getScheme() {
      return new MutateSpecTupleScheme();
    }
  }

  private static class MutateSpecTupleScheme extends TupleScheme<MutateSpec> {

    @Override
    public void write(org.apache.thrift.protocol.TProtocol prot, MutateSpec struct) throws org.apache.thrift.TException {
      TTupleProtocol oprot = (TTupleProtocol) prot;
      oprot.writeString(struct.appname);
      oprot.writeI32(struct.flush_interval);
      oprot.writeI32(struct.flags);
    }

    @Override
    public void read(org.apache.thrift.protocol.TProtocol prot, MutateSpec struct) throws org.apache.thrift.TException {
      TTupleProtocol iprot = (TTupleProtocol) prot;
      struct.appname = iprot.readString();
      struct.setAppnameIsSet(true);
      struct.flush_interval = iprot.readI32();
      struct.setFlush_intervalIsSet(true);
      struct.flags = iprot.readI32();
      struct.setFlagsIsSet(true);
    }
  }

}

