module Groonga
  module ExpressionTree
    class Variable
      attr_reader :column
      def initialize(column)
        @column = column
      end

      def build(expression)
        expression.append_object(@column, Operator::GET_VALUE, 1)
      end
    end
  end
end
