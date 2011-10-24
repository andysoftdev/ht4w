#
# Autogenerated by Thrift Compiler (0.7.0)
#
# DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
#

require 'client_types'


module Hypertable
  module ThriftGen
        # Result type of HQL queries
        # 
        # <dl>
        #   <dt>results</dt>
        #   <dd>String results from metadata queries</dd>
        # 
        #   <dt>cells</dt>
        #   <dd>Resulting table cells of for buffered queries</dd>
        # 
        #   <dt>scanner</dt>
        #   <dd>Resulting scanner ID for unbuffered queries</dd>
        # 
        #   <dt>mutator</dt>
        #   <dd>Resulting mutator ID for unflushed modifying queries</dd>
        # </dl>
        class HqlResult
          include ::Thrift::Struct, ::Thrift::Struct_Union
          RESULTS = 1
          CELLS = 2
          SCANNER = 3
          MUTATOR = 4

          FIELDS = {
            RESULTS => {:type => ::Thrift::Types::LIST, :name => 'results', :element => {:type => ::Thrift::Types::STRING}, :optional => true},
            CELLS => {:type => ::Thrift::Types::LIST, :name => 'cells', :element => {:type => ::Thrift::Types::STRUCT, :class => Hypertable::ThriftGen::Cell}, :optional => true},
            SCANNER => {:type => ::Thrift::Types::I64, :name => 'scanner', :optional => true},
            MUTATOR => {:type => ::Thrift::Types::I64, :name => 'mutator', :optional => true}
          }

          def struct_fields; FIELDS; end

          def validate
          end

          ::Thrift::Struct.generate_accessors self
        end

        # Same as HqlResult except with cell as array
        class HqlResult2
          include ::Thrift::Struct, ::Thrift::Struct_Union
          RESULTS = 1
          CELLS = 2
          SCANNER = 3
          MUTATOR = 4

          FIELDS = {
            RESULTS => {:type => ::Thrift::Types::LIST, :name => 'results', :element => {:type => ::Thrift::Types::STRING}, :optional => true},
            CELLS => {:type => ::Thrift::Types::LIST, :name => 'cells', :element => {:type => ::Thrift::Types::LIST, :element => {:type => ::Thrift::Types::STRING}}, :optional => true},
            SCANNER => {:type => ::Thrift::Types::I64, :name => 'scanner', :optional => true},
            MUTATOR => {:type => ::Thrift::Types::I64, :name => 'mutator', :optional => true}
          }

          def struct_fields; FIELDS; end

          def validate
          end

          ::Thrift::Struct.generate_accessors self
        end

      end
    end
