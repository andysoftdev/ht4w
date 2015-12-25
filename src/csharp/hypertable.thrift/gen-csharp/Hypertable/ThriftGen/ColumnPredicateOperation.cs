/**
 * Autogenerated by Thrift Compiler (0.9.3)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */

namespace Hypertable.ThriftGen
{
  /// <summary>
  /// The "operation" for a ColumnPredicate
  /// 
  /// EXACT_MATCH: compares the cell value for identity
  ///     (... WHERE column = "value")
  /// PREFIX_MATCH: compares the cell value for a prefix match
  ///     (... WHERE column =^ "prefix")
  /// </summary>
  public enum ColumnPredicateOperation
  {
    EXACT_MATCH = 1,
    PREFIX_MATCH = 2,
    REGEX_MATCH = 4,
    VALUE_MATCH = 7,
    QUALIFIER_EXACT_MATCH = 256,
    QUALIFIER_PREFIX_MATCH = 512,
    QUALIFIER_REGEX_MATCH = 1024,
    QUALIFIER_MATCH = 1792,
  }
}
