/**
 * Autogenerated by Thrift Compiler (0.9.2)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */

namespace Hypertable.ThriftGen
{
  /// <summary>
  /// Mutator creation flags
  /// 
  /// NO_LOG_SYNC: Do not sync the commit log
  /// IGNORE_UNKNOWN_CFS: Don't throw exception if mutator writes to unknown column family
  /// NO_LOG: Don't write to the commit log
  /// </summary>
  public enum MutatorFlag
  {
    NO_LOG_SYNC = 1,
    IGNORE_UNKNOWN_CFS = 2,
    NO_LOG = 4,
  }
}
